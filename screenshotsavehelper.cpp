#include "screenshotsavehelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QRegularExpression>
#include <QStorageInfo>

namespace {
QString mapFileError(const QFileDevice::FileError err)
{
    switch (err) {
    case QFileDevice::PermissionsError:
        return QStringLiteral("permission-denied");
    case QFileDevice::ResourceError:
        return QStringLiteral("no-space");
    case QFileDevice::RenameError:
        return QStringLiteral("target-busy");
    case QFileDevice::OpenError:
    case QFileDevice::ReadError:
    case QFileDevice::WriteError:
    case QFileDevice::CopyError:
    case QFileDevice::UnspecifiedError:
    case QFileDevice::TimeOutError:
    case QFileDevice::FileError::AbortError:
        return QStringLiteral("io-error");
    case QFileDevice::NoError:
    case QFileDevice::PositionError:
    case QFileDevice::ResizeError:
    case QFileDevice::RemoveError:
    default:
        return QStringLiteral("save-failed");
    }
}

QString mapWriterError(const QImageWriter::ImageWriterError err, const QString &errorText)
{
    if (err == QImageWriter::UnsupportedFormatError) {
        return QStringLiteral("unsupported-format");
    }
    if (err == QImageWriter::InvalidImageError) {
        return QStringLiteral("invalid-image");
    }
    const QString low = errorText.toLower();
    if (low.contains(QStringLiteral("permission")) || low.contains(QStringLiteral("denied"))) {
        return QStringLiteral("permission-denied");
    }
    if (low.contains(QStringLiteral("no space")) || low.contains(QStringLiteral("space left"))) {
        return QStringLiteral("no-space");
    }
    return QStringLiteral("io-error");
}
} // namespace

ScreenshotSaveHelper::ScreenshotSaveHelper(QObject *parent)
    : QObject(parent)
{
}

QString ScreenshotSaveHelper::validateFileName(const QString &name) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Filename is empty");
    }
    if (trimmed == QStringLiteral(".") || trimmed == QStringLiteral("..")) {
        return QStringLiteral("Invalid filename");
    }
    static const QRegularExpression invalidChars(QStringLiteral(R"([\\/:*?"<>|])"));
    if (invalidChars.match(trimmed).hasMatch()) {
        return QStringLiteral("Filename contains invalid characters");
    }
    if (trimmed.endsWith(QLatin1Char('.'))) {
        return QStringLiteral("Invalid filename");
    }
    return QString();
}

QString ScreenshotSaveHelper::ensurePngFileName(const QString &name) const
{
    return ensureFileNameForExtension(name, QStringLiteral("png"));
}

QString ScreenshotSaveHelper::ensureFileNameForExtension(const QString &name, const QString &extension) const
{
    QString out = name.trimmed();
    if (out.isEmpty()
        || out == QStringLiteral(".")
        || out == QStringLiteral("..")
        || out.endsWith(QLatin1Char('.'))) {
        out = QStringLiteral("Screenshot");
    }
    QString ext = extension.trimmed().toLower();
    if (ext.startsWith(QLatin1Char('.'))) {
        ext.remove(0, 1);
    }
    if (ext.isEmpty()) {
        ext = QStringLiteral("png");
    }

    const int dot = out.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        out = out.left(dot);
    }
    out += QStringLiteral(".") + ext;
    return out;
}

QString ScreenshotSaveHelper::buildTargetPathWithExtension(const QString &folder,
                                                           const QString &name,
                                                           const QString &extension) const
{
    QString base = folder.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("~/Pictures/Screenshots");
    }
    const QString fileName = ensureFileNameForExtension(name, extension);
    if (base.endsWith(QLatin1Char('/'))) {
        return base + fileName;
    }
    return base + QStringLiteral("/") + fileName;
}

QString ScreenshotSaveHelper::buildTargetPath(const QString &folder, const QString &name) const
{
    return buildTargetPathWithExtension(folder, name, QStringLiteral("png"));
}

bool ScreenshotSaveHelper::convertImageFile(const QString &sourcePath,
                                            const QString &targetPath,
                                            const QString &format,
                                            int quality) const
{
    const QString src = sourcePath.trimmed();
    const QString dst = targetPath.trimmed();
    if (src.isEmpty() || dst.isEmpty()) {
        return false;
    }

    QImage image(src);
    if (image.isNull()) {
        return false;
    }

    const QByteArray fmt = format.trimmed().toLatin1().toLower();
    QImageWriter writer(dst, fmt);
    if (quality >= 0 && quality <= 100) {
        writer.setQuality(quality);
    }
    return writer.write(image);
}

QVariantMap ScreenshotSaveHelper::saveImageFile(const QString &sourcePath,
                                                const QString &targetPath,
                                                const QString &format,
                                                int quality,
                                                bool overwrite) const
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("error"), QStringLiteral("io-error"));
    out.insert(QStringLiteral("targetPath"), targetPath.trimmed());

    const QString src = sourcePath.trimmed();
    const QString dst = targetPath.trimmed();
    if (src.isEmpty() || dst.isEmpty()) {
        out.insert(QStringLiteral("error"), QStringLiteral("invalid-args"));
        return out;
    }
    if (!QFileInfo::exists(src)) {
        out.insert(QStringLiteral("error"), QStringLiteral("source-missing"));
        return out;
    }

    QFileInfo dstInfo(dst);
    const QString dstDir = dstInfo.absolutePath();
    if (!dstDir.isEmpty()) {
        QDir dir;
        if (!dir.mkpath(dstDir)) {
            out.insert(QStringLiteral("error"), QStringLiteral("mkdir-failed"));
            return out;
        }
    }

    if (QFileInfo::exists(dst)) {
        if (!overwrite) {
            out.insert(QStringLiteral("error"), QStringLiteral("target-exists"));
            return out;
        }
        QFile dstFile(dst);
        if (!dstFile.remove()) {
            const QString code = mapFileError(dstFile.error());
            out.insert(QStringLiteral("error"),
                       code.isEmpty() ? QStringLiteral("overwrite-remove-failed") : code);
            return out;
        }
    }

    const QString fmt = format.trimmed().toLower();
    bool ok = false;
    QString errorCode = QStringLiteral("save-failed");
    if (fmt == QStringLiteral("png")) {
        if (src == dst) {
            ok = true;
        } else {
            QFile srcFile(src);
            ok = srcFile.rename(dst);
            if (!ok) {
                errorCode = mapFileError(srcFile.error());
                QFile copyFile(src);
                ok = copyFile.copy(dst);
                if (ok) {
                    QFile cleanup(src);
                    if (!cleanup.remove()) {
                        errorCode = mapFileError(cleanup.error());
                        ok = false;
                    }
                } else {
                    errorCode = mapFileError(copyFile.error());
                }
            }
        }
    } else {
        QImage image(src);
        if (image.isNull()) {
            errorCode = QStringLiteral("invalid-image");
            ok = false;
        } else {
            QImageWriter writer(dst, fmt.toLatin1());
            if (quality >= 0 && quality <= 100) {
                writer.setQuality(quality);
            }
            ok = writer.write(image);
            if (!ok) {
                errorCode = mapWriterError(writer.error(), writer.errorString());
            }
        }
        if (ok) {
            QFile cleanup(src);
            if (!cleanup.remove()) {
                errorCode = mapFileError(cleanup.error());
                ok = false;
            }
        }
    }

    out.insert(QStringLiteral("ok"), ok);
    if (!ok) {
        out.insert(QStringLiteral("error"),
                   errorCode.isEmpty() ? QStringLiteral("save-failed") : errorCode);
        return out;
    }
    out.insert(QStringLiteral("error"), QString());
    return out;
}

QVariantMap ScreenshotSaveHelper::storageInfoForPath(const QString &path) const
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("error"), QStringLiteral("storage-unavailable"));

    QString p = path.trimmed();
    if (p.isEmpty()) {
        out.insert(QStringLiteral("error"), QStringLiteral("invalid-args"));
        return out;
    }
    if (p == QStringLiteral("~")) {
        p = QDir::homePath();
    } else if (p.startsWith(QStringLiteral("~/"))) {
        p = QDir::homePath() + p.mid(1);
    }

    QFileInfo info(p);
    QString probe = info.exists() ? info.absoluteFilePath() : info.absolutePath();
    if (probe.isEmpty()) {
        probe = p;
    }

    QFileInfo probeInfo(probe);
    while (!probeInfo.exists()) {
        const QDir parent = probeInfo.dir();
        const QString parentPath = parent.absolutePath();
        if (parentPath.isEmpty() || parentPath == probeInfo.absoluteFilePath()) {
            break;
        }
        probeInfo = QFileInfo(parentPath);
    }

    QStorageInfo storage(probeInfo.absoluteFilePath());
    if (!storage.isValid() || !storage.isReady()) {
        out.insert(QStringLiteral("error"), QStringLiteral("storage-unavailable"));
        return out;
    }

    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("error"), QString());
    out.insert(QStringLiteral("rootPath"), storage.rootPath());
    out.insert(QStringLiteral("displayName"), storage.displayName());
    out.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(storage.bytesAvailable()));
    out.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(storage.bytesTotal()));
    return out;
}

bool ScreenshotSaveHelper::shouldPromptOverwrite(bool targetExists, bool forceOverwrite) const
{
    return targetExists && !forceOverwrite;
}

QString ScreenshotSaveHelper::resolveChosenFolder(const QString &currentFolder,
                                                  const QVariantList &selectedPaths,
                                                  bool chooserAccepted) const
{
    if (!chooserAccepted || selectedPaths.isEmpty()) {
        return currentFolder.trimmed();
    }
    const QString selected = selectedPaths.first().toString().trimmed();
    if (selected.isEmpty()) {
        return currentFolder.trimmed();
    }
    return selected;
}

bool ScreenshotSaveHelper::shouldResumeSaveDialog(const QString &sourcePath,
                                                  bool chooserAccepted) const
{
    Q_UNUSED(chooserAccepted)
    return !sourcePath.trimmed().isEmpty();
}
