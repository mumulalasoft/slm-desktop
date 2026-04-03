#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QString>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logFmThumb)

class FileManagerApi;

/**
 * ThumbnailImageProvider
 *
 * Qt image provider yang memungkinkan QML Image{} memuat thumbnail file
 * via FileManagerApi::ensureThumbnail() secara sinkron.
 *
 * Cara pakai di QML:
 *   Image {
 *       source: "image://thumbnail/256/" + encodeURIComponent(filePath)
 *   }
 *
 * Format URL: image://thumbnail/{size}/{path}
 *   - size : integer pixel (e.g. 128, 256, 512)
 *   - path : path file absolut (URL-encoded bila mengandung spasi/karakter khusus)
 *
 * Thumbnail di-generate oleh freedesktop thumbnail spec (~/.cache/thumbnails/).
 * Jika thumbnail tidak tersedia, provider return gambar kosong (tidak crash).
 *
 * Threading: QQuickImageProvider dipanggil dari thread image loading Qt,
 * bukan main thread. FileManagerApi::ensureThumbnail() harus thread-safe
 * untuk read-only access (sudah dipastikan di implementasi).
 */
class ThumbnailImageProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailImageProvider(FileManagerApi *api);

    QImage requestImage(const QString &id,
                        QSize *size,
                        const QSize &requestedSize) override;

private:
    FileManagerApi *m_api;
};
