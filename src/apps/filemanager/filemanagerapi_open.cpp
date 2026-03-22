#include "filemanagerapi.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#pragma pop_macro("signals")

#include <thread>

namespace {

static QString contentTypeForPathOpen(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    GFile *file = g_file_new_for_path(encoded.constData());
    if (file == nullptr) {
        return QString();
    }
    GError *gerr = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &gerr);
    QString out;
    if (info != nullptr) {
        const char *ct = g_file_info_get_content_type(info);
        if (ct != nullptr) {
            out = QString::fromUtf8(ct);
        }
        g_object_unref(info);
    }
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_object_unref(file);
    return out;
}

} // namespace

QVariantMap FileManagerApi::openPath(const QString &path) const
{
    const QString p = expandPath(path);
    QString uri = path.trimmed();
    const QUrl parsed(uri);
    if (!(parsed.isValid() && !parsed.scheme().isEmpty())) {
        uri = QUrl::fromLocalFile(p).toString(QUrl::FullyEncoded);
    }
    GError *gerr = nullptr;
    const bool ok = g_app_info_launch_default_for_uri(uri.toUtf8().constData(), nullptr, &gerr);
    QString err;
    if (gerr != nullptr) {
        err = QString::fromUtf8(gerr->message).trimmed();
        g_error_free(gerr);
    }
    if (!ok) {
        return makeResult(false,
                          err.isEmpty() ? QStringLiteral("open-failed") : err,
                          {{QStringLiteral("path"), p}, {QStringLiteral("uri"), uri}});
    }
    return makeResult(true, QString(), {{QStringLiteral("path"), p}, {QStringLiteral("uri"), uri}});
}

QVariantMap FileManagerApi::openPathWithApp(const QString &path, const QString &appId) const
{
    const QString p = expandPath(path);
    const QString app = appId.trimmed();
    if (p.isEmpty() || app.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-argument"));
    }
    QString desktopId = app;
    GDesktopAppInfo *desktopInfo = g_desktop_app_info_new(desktopId.toUtf8().constData());
    if (!desktopInfo && !desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        desktopId += QStringLiteral(".desktop");
        desktopInfo = g_desktop_app_info_new(desktopId.toUtf8().constData());
    }
    if (!desktopInfo) {
        return makeResult(false, QStringLiteral("app-not-found"));
    }

    GFile *file = g_file_new_for_path(QFile::encodeName(p).constData());
    GList *files = nullptr;
    files = g_list_append(files, file);
    GError *gerr = nullptr;
    const bool ok = g_app_info_launch(G_APP_INFO(desktopInfo), files, nullptr, &gerr);
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_list_free(files);
    g_object_unref(file);
    g_object_unref(desktopInfo);
    if (!ok) {
        return makeResult(false, QStringLiteral("launch-failed"));
    }
    return makeResult(true, QString(), {{QStringLiteral("path"), p}, {QStringLiteral("appId"), app}});
}

QVariantMap FileManagerApi::setDefaultOpenWithApp(const QString &path, const QString &appId) const
{
    const QString p = expandPath(path);
    QString desktopId = appId.trimmed();
    if (p.isEmpty() || desktopId.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-argument"));
    }
    const QString contentType = contentTypeForPathOpen(p);
    if (contentType.isEmpty()) {
        return makeResult(false, QStringLiteral("content-type-unavailable"));
    }

    GDesktopAppInfo *desktopInfo = g_desktop_app_info_new(desktopId.toUtf8().constData());
    if (!desktopInfo && !desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        desktopId += QStringLiteral(".desktop");
        desktopInfo = g_desktop_app_info_new(desktopId.toUtf8().constData());
    }
    if (!desktopInfo) {
        return makeResult(false, QStringLiteral("app-not-found"));
    }

    GError *gerr = nullptr;
    const bool ok = g_app_info_set_as_default_for_type(G_APP_INFO(desktopInfo),
                                                        contentType.toUtf8().constData(),
                                                        &gerr);
    QString err;
    if (gerr != nullptr) {
        err = QString::fromUtf8(gerr->message).trimmed();
        g_error_free(gerr);
    }
    g_object_unref(desktopInfo);
    if (!ok) {
        return makeResult(false, err.isEmpty() ? QStringLiteral("set-default-failed") : err);
    }
    return makeResult(true, QString(),
                      {{QStringLiteral("path"), p},
                       {QStringLiteral("appId"), desktopId},
                       {QStringLiteral("contentType"), contentType}});
}

QVariantMap FileManagerApi::openPathInFileManager(const QString &path) const
{
    const QString p = expandPath(path);
    const QFileInfo fi(p);
    if (!fi.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    const QString target = fi.isDir() ? p : fi.absolutePath();
    const QString fileUri = QUrl::fromLocalFile(fi.absoluteFilePath()).toString(QUrl::FullyEncoded);
    const QString targetUri = QUrl::fromLocalFile(target).toString(QUrl::FullyEncoded);

    const QString method = fi.isDir()
            ? QStringLiteral("org.freedesktop.FileManager1.ShowFolders")
            : QStringLiteral("org.freedesktop.FileManager1.ShowItems");
    QStringList args;
    args << QStringLiteral("call")
         << QStringLiteral("--session")
         << QStringLiteral("--dest")
         << QStringLiteral("org.freedesktop.FileManager1")
         << QStringLiteral("--object-path")
         << QStringLiteral("/org/freedesktop/FileManager1")
         << QStringLiteral("--method")
         << method
         << QStringLiteral("['%1']").arg(fi.isDir() ? targetUri : fileUri)
         << QStringLiteral("");

    const bool dbusOk = QProcess::startDetached(QStringLiteral("gdbus"), args);
    if (dbusOk) {
        return makeResult(true, QString(), {{QStringLiteral("path"), target}});
    }

    GError *gerr = nullptr;
    const bool ok = g_app_info_launch_default_for_uri(targetUri.toUtf8().constData(), nullptr, &gerr);
    QString err;
    if (gerr != nullptr) {
        err = QString::fromUtf8(gerr->message).trimmed();
        g_error_free(gerr);
    }
    if (!ok) {
        return makeResult(false,
                          err.isEmpty() ? QStringLiteral("open-failed") : err,
                          {{QStringLiteral("path"), target}});
    }
    return makeResult(true, QString(), {{QStringLiteral("path"), target}});
}

QVariantMap FileManagerApi::startOpenPath(const QString &path, const QString &requestId)
{
    const QString p = expandPath(path);
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    std::thread([this, rid, p]() {
        const QVariantMap result = openPath(p);
        QMetaObject::invokeMethod(this, [this, rid, result]() {
            emit openActionFinished(rid, QStringLiteral("open"), result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("requestId"), rid},
                                        {QStringLiteral("path"), p},
                                        {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startOpenPathWithApp(const QString &path,
                                                 const QString &appId,
                                                 const QString &requestId)
{
    const QString p = expandPath(path);
    const QString app = appId.trimmed();
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    if (p.isEmpty() || app.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-argument"));
    }
    std::thread([this, rid, p, app]() {
        const QVariantMap result = openPathWithApp(p, app);
        QMetaObject::invokeMethod(this, [this, rid, result]() {
            emit openActionFinished(rid, QStringLiteral("open-with"), result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("requestId"), rid},
                                        {QStringLiteral("path"), p},
                                        {QStringLiteral("appId"), app},
                                        {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startSetDefaultOpenWithApp(const QString &path,
                                                       const QString &appId,
                                                       const QString &requestId)
{
    const QString p = expandPath(path);
    const QString app = appId.trimmed();
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    if (p.isEmpty() || app.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-argument"));
    }
    std::thread([this, rid, p, app]() {
        const QVariantMap result = setDefaultOpenWithApp(p, app);
        QMetaObject::invokeMethod(this, [this, rid, result]() {
            emit openActionFinished(rid, QStringLiteral("set-default-open-with"), result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("requestId"), rid},
                                        {QStringLiteral("path"), p},
                                        {QStringLiteral("appId"), app},
                                        {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startOpenPathInFileManager(const QString &path, const QString &requestId)
{
    const QString p = expandPath(path);
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    std::thread([this, rid, p]() {
        const QVariantMap result = openPathInFileManager(p);
        QMetaObject::invokeMethod(this, [this, rid, result]() {
            emit openActionFinished(rid, QStringLiteral("open-in-file-manager"), result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("requestId"), rid},
                                        {QStringLiteral("path"), p},
                                        {QStringLiteral("async"), true}});
}
