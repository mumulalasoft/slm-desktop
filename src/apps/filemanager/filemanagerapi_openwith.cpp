#include "filemanagerapi.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMetaObject>
#include <QSet>
#include <QUrl>
#include <QtGlobal>

#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#pragma pop_macro("signals")

#include <thread>

namespace {

static QString iconNameFromGIconLocal(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return QString::fromUtf8(names[0]);
        }
    }
    return QString();
}

static QString contentTypeForPathLocal(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        return QString();
    }
    GFile *file = g_file_new_for_path(QFile::encodeName(fi.absoluteFilePath()).constData());
    if (!file) {
        return QString();
    }
    GError *error = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &error);
    QString contentType;
    if (info != nullptr) {
        const char *ct = g_file_info_get_content_type(info);
        if (ct) {
            contentType = QString::fromUtf8(ct);
        }
        g_object_unref(info);
    }
    if (error) {
        g_error_free(error);
    }
    g_object_unref(file);
    if (contentType.isEmpty()) {
        contentType = QMimeDatabase().mimeTypeForFile(fi.absoluteFilePath(),
                                                      QMimeDatabase::MatchContent).name();
    }
    return contentType;
}

} // namespace

QVariantList FileManagerApi::openWithApplications(const QString &path, int limit) const
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    if (!fi.exists()) {
        return {};
    }
    const QString contentType = contentTypeForPathLocal(p);
    if (contentType.isEmpty()) {
        return {};
    }

    const QByteArray ctUtf8 = contentType.toUtf8();
    GAppInfo *defaultApp = g_app_info_get_default_for_type(ctUtf8.constData(), false);
    const int hardLimit = limit <= 0 ? 256 : qMin(limit, 256);
    QVariantList out;
    QSet<QString> seen;

    // Phase 1: apps registered in GIO's MIME database for this content type.
    GList *apps = g_app_info_get_all_for_type(ctUtf8.constData());
    for (GList *it = apps; it != nullptr && out.size() < hardLimit; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }
        const QString id = QString::fromUtf8(g_app_info_get_id(info));
        if (id.isEmpty()) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), id);
        row.insert(QStringLiteral("name"), QString::fromUtf8(g_app_info_get_display_name(info)));
        row.insert(QStringLiteral("defaultApp"),
                   defaultApp != nullptr && g_app_info_equal(defaultApp, info));
        row.insert(QStringLiteral("iconName"), iconNameFromGIconLocal(g_app_info_get_icon(info)));
        out.push_back(row);
        seen.insert(id);
    }
    g_list_free_full(apps, g_object_unref);

    // Phase 2: supplement with sandbox (Flatpak) apps not in GIO's per-process
    // MIME cache.  g_app_info_get_all() enumerates every installed app including
    // those whose .desktop files live under Flatpak's export dirs.  We check
    // g_app_info_get_supported_types() directly so we don't rely on the MIME
    // database being populated for this process.
    GList *allApps = g_app_info_get_all();
    for (GList *it = allApps; it != nullptr && out.size() < hardLimit; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }
        const QString id = QString::fromUtf8(g_app_info_get_id(info));
        if (id.isEmpty() || seen.contains(id)) {
            continue;
        }
        const char * const *types = g_app_info_get_supported_types(info);
        if (!types) {
            continue;
        }
        bool supports = false;
        for (int j = 0; types[j] != nullptr && !supports; ++j) {
            // Exact match or content-type hierarchy (e.g. app declares "image/*"
            // and file is "image/png").
            if (qstrcmp(types[j], ctUtf8.constData()) == 0
                    || g_content_type_is_a(ctUtf8.constData(), types[j])) {
                supports = true;
            }
        }
        if (!supports) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), id);
        row.insert(QStringLiteral("name"), QString::fromUtf8(g_app_info_get_display_name(info)));
        row.insert(QStringLiteral("defaultApp"),
                   defaultApp != nullptr && g_app_info_equal(defaultApp, info));
        row.insert(QStringLiteral("iconName"), iconNameFromGIconLocal(g_app_info_get_icon(info)));
        row.insert(QStringLiteral("sandbox"), true);
        out.push_back(row);
        seen.insert(id);
    }
    g_list_free_full(allApps, g_object_unref);

    if (defaultApp != nullptr) {
        g_object_unref(defaultApp);
    }
    return out;
}

QVariantMap FileManagerApi::startOpenWithApplications(const QString &path, int limit, const QString &requestId)
{
    const QString p = expandPath(path);
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    std::thread([this, p, limit, rid]() {
        const QVariantList rows = openWithApplications(p, limit);
        const QString error = rows.isEmpty() ? QStringLiteral("no-applications") : QString();
        QMetaObject::invokeMethod(this, [this, rid, p, rows, error]() {
            emit openWithApplicationsReady(rid, p, rows, error);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("requestId"), rid},
                       {QStringLiteral("path"), p},
                       {QStringLiteral("async"), true}});
}
