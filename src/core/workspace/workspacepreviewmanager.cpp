#include "workspacepreviewmanager.h"
#include "urlutils.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPixmap>
#include <QDebug>
#include <QRegularExpression>
#include <QScreen>
#include <QProcessEnvironment>
#include <QDateTime>

namespace {
constexpr auto kWorkspacePreviewCacheDir = "/tmp/slm-desktop-workspace-previews";
constexpr auto kLegacyOverviewPreviewCacheDir = "/tmp/slm-desktop-overview-previews";

QString previewPathForDir(const QString &baseDir, const QString &sanitizedId)
{
    return baseDir + QLatin1Char('/') + sanitizedId + QStringLiteral(".png");
}
}

WorkspacePreviewManager::WorkspacePreviewManager(QObject *parent)
    : QObject(parent)
{
    m_framePath = QString::fromLatin1(kWorkspacePreviewCacheDir) + QStringLiteral("/__frame.png");
}

QString WorkspacePreviewManager::sanitizeId(const QString &value)
{
    QString out = value.trimmed();
    if (out.isEmpty()) {
        return QStringLiteral("window");
    }
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    if (out.isEmpty()) {
        out = QStringLiteral("window");
    }
    return out;
}

QString WorkspacePreviewManager::cachePathForView(const QString &viewId) const
{
    const QString baseDir = QString::fromLatin1(kWorkspacePreviewCacheDir);
    QDir().mkpath(baseDir);
    return previewPathForDir(baseDir, sanitizeId(viewId));
}

bool WorkspacePreviewManager::refreshFrameSnapshot()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_frameImage.isNull() && (nowMs - m_frameCapturedAtMs) < 700) {
        return true;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return false;
    }
    const QPixmap shot = screen->grabWindow(0);
    if (shot.isNull()) {
        return false;
    }
    const QImage img = shot.toImage();
    if (img.isNull()) {
        return false;
    }
    m_frameImage = img;
    m_frameCapturedAtMs = nowMs;
    return true;
}

QString WorkspacePreviewManager::capturePreviewSource(const QString &viewId, int x, int y, int width, int height)
{
    if (width < 8 || height < 8) {
        return QString();
    }

    const QString path = cachePathForView(viewId);
    const QFileInfo beforeFi(path);
    const QString legacyPath = previewPathForDir(QString::fromLatin1(kLegacyOverviewPreviewCacheDir),
                                                 sanitizeId(viewId));
    const QFileInfo legacyFi(legacyPath);
    bool saved = false;

    if (refreshFrameSnapshot()) {
        const QRect bounds(0, 0, m_frameImage.width(), m_frameImage.height());
        const QRect crop(x, y, width, height);
        const QRect clipped = crop.intersected(bounds);
        if (clipped.width() > 0 && clipped.height() > 0) {
            const QImage cropped = m_frameImage.copy(clipped);
            if (!cropped.isNull()) {
                saved = cropped.save(path, "PNG");
            }
        }
    }

    if (!saved) {
        // No external dependency fallback path (may fail on strict Wayland).
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen == nullptr) {
            return QString();
        }
        QPixmap shot = screen->grabWindow(0, x, y, width, height);
        if (!shot.isNull()) {
            QImage image = shot.toImage();
            if (!image.isNull()) {
                saved = image.save(path, "PNG");
            }
        }
    }

    if (!saved) {
        // Keep last successful snapshot if fresh capture fails.
        if (beforeFi.exists() && beforeFi.size() > 0) {
            return UrlUtils::toFileUrl(path)
                   + QStringLiteral("?rev=") + QString::number(m_revision);
        }
        if (legacyFi.exists() && legacyFi.size() > 0) {
            // Compatibility fallback during cache path migration.
            return UrlUtils::toFileUrl(legacyPath)
                   + QStringLiteral("?rev=") + QString::number(m_revision);
        }
        qWarning() << "WorkspacePreviewManager: capture failed for" << viewId
                   << "geom=" << x << y << width << height;
        return QString();
    }

    m_revision++;
    return UrlUtils::toFileUrl(path)
           + QStringLiteral("?rev=") + QString::number(m_revision);
}
