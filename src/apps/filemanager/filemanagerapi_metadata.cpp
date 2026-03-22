#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QStorageInfo>
#include <QUrl>
#include <thread>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace {

static QStringList uniqueExpandedPathsMetadata(const QVariantList &paths)
{
    QStringList out;
    QSet<QString> seen;
    for (const QVariant &v : paths) {
        QString p = v.toString().trimmed();
        if (p == QStringLiteral("~")) {
            p = QDir::homePath();
        } else if (p.startsWith(QStringLiteral("~/"))) {
            p = QDir::homePath() + p.mid(1);
        }
        if (p.isEmpty()) {
            continue;
        }
        QFileInfo fi(p);
        const QString abs = fi.absoluteFilePath();
        if (seen.contains(abs)) {
            continue;
        }
        seen.insert(abs);
        out.push_back(abs);
    }
    return out;
}

static QString contentTypeForPathMetadata(const QString &path)
{
    const QByteArray local = QFile::encodeName(path);
    GFile *file = g_file_new_for_path(local.constData());
    GError *gerr = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &gerr);
    QString contentType;
    if (info != nullptr) {
        const char *ct = g_file_info_get_content_type(info);
        if (ct != nullptr) {
            contentType = QString::fromUtf8(ct).trimmed();
        }
        g_object_unref(info);
    }
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_object_unref(file);
    return contentType;
}

} // namespace

QVariantMap FileManagerApi::statPath(const QString &path) const
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    if (!fi.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    QVariantMap out = fileInfoMap(fi);
    out.insert(QStringLiteral("absolutePath"), fi.absoluteFilePath());
    out.insert(QStringLiteral("created"), fi.birthTime().isValid()
               ? fi.birthTime().toString(Qt::ISODateWithMs) : QString());
    out.insert(QStringLiteral("modified"), fi.lastModified().isValid()
               ? fi.lastModified().toString(Qt::ISODateWithMs) : QString());
    out.insert(QStringLiteral("owner"), fi.owner());
    out.insert(QStringLiteral("group"), fi.group());

    const QFileDevice::Permissions perms = fi.permissions();
    const bool ownerRead = perms.testFlag(QFileDevice::ReadOwner);
    const bool ownerWrite = perms.testFlag(QFileDevice::WriteOwner);
    const bool ownerExec = perms.testFlag(QFileDevice::ExeOwner);
    const bool groupRead = perms.testFlag(QFileDevice::ReadGroup);
    const bool groupWrite = perms.testFlag(QFileDevice::WriteGroup);
    const bool groupExec = perms.testFlag(QFileDevice::ExeGroup);
    const bool otherRead = perms.testFlag(QFileDevice::ReadOther);
    const bool otherWrite = perms.testFlag(QFileDevice::WriteOther);
    const bool otherExec = perms.testFlag(QFileDevice::ExeOther);

    auto octDigit = [](bool r, bool w, bool x) -> int {
        return (r ? 4 : 0) + (w ? 2 : 0) + (x ? 1 : 0);
    };
    const QString permOctal = QString::number(octDigit(ownerRead, ownerWrite, ownerExec))
                            + QString::number(octDigit(groupRead, groupWrite, groupExec))
                            + QString::number(octDigit(otherRead, otherWrite, otherExec));
    const QString permSymbolic = QString(ownerRead ? 'r' : '-')
            + QString(ownerWrite ? 'w' : '-')
            + QString(ownerExec ? 'x' : '-')
            + QString(groupRead ? 'r' : '-')
            + QString(groupWrite ? 'w' : '-')
            + QString(groupExec ? 'x' : '-')
            + QString(otherRead ? 'r' : '-')
            + QString(otherWrite ? 'w' : '-')
            + QString(otherExec ? 'x' : '-');

    out.insert(QStringLiteral("permOwnerRead"), ownerRead);
    out.insert(QStringLiteral("permOwnerWrite"), ownerWrite);
    out.insert(QStringLiteral("permOwnerExec"), ownerExec);
    out.insert(QStringLiteral("permGroupRead"), groupRead);
    out.insert(QStringLiteral("permGroupWrite"), groupWrite);
    out.insert(QStringLiteral("permGroupExec"), groupExec);
    out.insert(QStringLiteral("permOtherRead"), otherRead);
    out.insert(QStringLiteral("permOtherWrite"), otherWrite);
    out.insert(QStringLiteral("permOtherExec"), otherExec);
    out.insert(QStringLiteral("permissionsOctal"), permOctal);
    out.insert(QStringLiteral("permissionsSymbolic"), permSymbolic);

    const QString mime = contentTypeForPathMetadata(fi.absoluteFilePath());
    out.insert(QStringLiteral("mimeType"), mime);

    const QStorageInfo storage(p);
    if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0) {
        out.insert(QStringLiteral("volumeBytesTotal"), static_cast<qlonglong>(storage.bytesTotal()));
        out.insert(QStringLiteral("volumeBytesAvailable"), static_cast<qlonglong>(storage.bytesAvailable()));
        out.insert(QStringLiteral("volumeRootPath"), storage.rootPath());
        out.insert(QStringLiteral("volumeDisplayName"), storage.displayName());
        out.insert(QStringLiteral("volumeDevice"), QString::fromUtf8(storage.device()));
        out.insert(QStringLiteral("volumeFileSystem"), QString::fromUtf8(storage.fileSystemType()));
    } else {
        out.insert(QStringLiteral("volumeBytesTotal"), static_cast<qlonglong>(-1));
        out.insert(QStringLiteral("volumeBytesAvailable"), static_cast<qlonglong>(-1));
        out.insert(QStringLiteral("volumeRootPath"), QString());
        out.insert(QStringLiteral("volumeDisplayName"), QString());
        out.insert(QStringLiteral("volumeDevice"), QString());
        out.insert(QStringLiteral("volumeFileSystem"), QString());
    }

    if (fi.isFile()) {
        QImageReader reader(fi.absoluteFilePath());
        const QSize size = reader.size();
        if (size.isValid()) {
            out.insert(QStringLiteral("imageWidth"), size.width());
            out.insert(QStringLiteral("imageHeight"), size.height());
        } else {
            out.insert(QStringLiteral("imageWidth"), 0);
            out.insert(QStringLiteral("imageHeight"), 0);
        }
    } else {
        out.insert(QStringLiteral("imageWidth"), 0);
        out.insert(QStringLiteral("imageHeight"), 0);
    }
    return out;
}

QVariantMap FileManagerApi::startStatPaths(const QVariantList &paths, const QString &requestId)
{
    const QString rid = requestId.trimmed().isEmpty()
        ? QString::number(QDateTime::currentMSecsSinceEpoch())
        : requestId.trimmed();
    const QStringList list = uniqueExpandedPathsMetadata(paths);
    std::thread([this, rid, list]() {
        QVariantList results;
        results.reserve(list.size());
        for (const QString &p : list) {
            QVariantMap row = statPath(p);
            if (!row.contains(QStringLiteral("path"))) {
                row.insert(QStringLiteral("path"), p);
            }
            results.push_back(row);
        }
        QMetaObject::invokeMethod(this, [this, rid, results]() {
            emit statPathsFinished(rid, results);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("requestId"), rid},
                       {QStringLiteral("count"), list.size()},
                       {QStringLiteral("async"), true}});
}
