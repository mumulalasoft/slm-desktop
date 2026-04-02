#include "ThumbnailImageProvider.h"

#include <QUrl>
#include <QFileInfo>
#include <QPainter>

#include "src/apps/filemanager/include/filemanagerapi.h"

Q_LOGGING_CATEGORY(logFmThumb, "slm.filemanager.thumbnails")

ThumbnailImageProvider::ThumbnailImageProvider(FileManagerApi *api)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_api(api)
{
    Q_ASSERT(api);
}

// id format: "{size}/{path}"
// Contoh: "256//home/user/photo.jpg"
//         "128//tmp/file%20name.png"  (URL-encoded)
QImage ThumbnailImageProvider::requestImage(const QString &id,
                                             QSize *size,
                                             const QSize &requestedSize)
{
    // Parse size prefix
    const int slashIdx = id.indexOf(QLatin1Char('/'));
    int thumbSize = 256;
    QString filePath;

    if (slashIdx > 0) {
        bool ok = false;
        const int parsed = id.left(slashIdx).toInt(&ok);
        thumbSize = ok ? qBound(32, parsed, 1024) : 256;
        filePath = QUrl::fromPercentEncoding(
            id.mid(slashIdx + 1).toUtf8());
    } else {
        filePath = QUrl::fromPercentEncoding(id.toUtf8());
    }

    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        qCDebug(logFmThumb) << "Thumbnail request for missing path:" << filePath;
        const QSize fallback = requestedSize.isValid() ? requestedSize : QSize(thumbSize, thumbSize);
        if (size) *size = fallback;
        return QImage(fallback, QImage::Format_ARGB32_Premultiplied);
    }

    // FileManagerApi::ensureThumbnail() checks the freedesktop thumbnail cache
    // and generates if absent. Returns path to thumbnail PNG or "".
    const QString thumbPath = m_api->ensureThumbnail(filePath, thumbSize);

    QImage img;
    if (!thumbPath.isEmpty()) {
        img.load(thumbPath);
    }

    if (img.isNull()) {
        qCDebug(logFmThumb) << "No thumbnail for:" << filePath;
        const QSize fallback = requestedSize.isValid()
            ? requestedSize : QSize(thumbSize, thumbSize);
        if (size) *size = fallback;
        return QImage(fallback, QImage::Format_ARGB32_Premultiplied);
    }

    // Scale to requested size bila berbeda
    if (requestedSize.isValid() && requestedSize != img.size())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);

    if (size) *size = img.size();
    qCDebug(logFmThumb) << "Thumbnail served:" << filePath << img.size();
    return img;
}
