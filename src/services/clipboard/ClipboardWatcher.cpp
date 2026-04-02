#include "ClipboardWatcher.h"
#include "WaylandClipboardWatcher.h"

#include "contentclassifier.h"
#include "clipboardtypes.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace Slm::Clipboard {

ClipboardWatcher::ClipboardWatcher(QObject *parent)
    : QObject(parent)
    , m_clipboard(QGuiApplication::clipboard())
{
    const QString platform = QGuiApplication::platformName();
    const bool forceQtFallback = qEnvironmentVariableIntValue("SLM_CLIPBOARD_FORCE_QT_FALLBACK") == 1;
    m_backendMode = selectBackendMode(platform, forceQtFallback);

    if (m_backendMode == BackendMode::NativeWayland) {
        m_wlWatcher = new WaylandClipboardWatcher(this);
        connect(m_wlWatcher, &WaylandClipboardWatcher::itemCaptured,
                this, &ClipboardWatcher::itemCaptured);
        if (!m_wlWatcher->init()) {
            qWarning() << "[ClipboardWatcher] Failed to initialize Wayland clipboard watcher";
            delete m_wlWatcher;
            m_wlWatcher = nullptr;
            m_backendMode = BackendMode::QtFallback;
        } else {
            qInfo() << "[ClipboardWatcher] Using native Wayland clipboard watcher";
        }
    }

    if (!m_wlWatcher && m_clipboard) {
        connect(m_clipboard, &QClipboard::changed,
                this, &ClipboardWatcher::onClipboardChanged);
        qInfo() << "[ClipboardWatcher] Using standard QClipboard watcher";
    }
}

ClipboardWatcher::BackendMode ClipboardWatcher::backendMode() const
{
    return m_backendMode;
}

QString ClipboardWatcher::backendModeString() const
{
    return m_backendMode == BackendMode::NativeWayland
               ? QStringLiteral("native-wayland")
               : QStringLiteral("qt-fallback");
}

void ClipboardWatcher::setSuppressed(bool suppressed)
{
    m_suppressed = suppressed;
    if (m_wlWatcher) {
        m_wlWatcher->setSuppressed(suppressed);
    }
}

void ClipboardWatcher::onClipboardChanged(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard || m_suppressed) {
        return;
    }
    const QVariantMap item = readClipboardItem();
    if (item.isEmpty()) {
        return;
    }
    emit itemCaptured(item);
}

QVariantMap ClipboardWatcher::readClipboardItem() const
{
    QVariantMap row;
    if (!m_clipboard) {
        return row;
    }
    const QMimeData *mime = m_clipboard->mimeData(QClipboard::Clipboard);
    if (!mime) {
        return row;
    }

    QString mimeType;
    QString content;
    QString preview;
    QString type;

    if (mime->hasImage()) {
        const QVariant imageVar = mime->imageData();
        const QImage image = imageVar.value<QImage>();
        if (!image.isNull()) {
            mimeType = QStringLiteral("image/png");
            content = cacheImage(image);
            if (content.isEmpty()) {
                return row;
            }
            type = QString::fromLatin1(kTypeImage);
            preview = ContentClassifier::generatePreview(type, QString(), mimeType, content);
        }
    } else if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        QStringList lines;
        lines.reserve(urls.size());
        for (const QUrl &u : urls) {
            if (!u.isValid()) {
                continue;
            }
            lines.push_back(u.toString());
        }
        if (!lines.isEmpty()) {
            mimeType = QStringLiteral("text/uri-list");
            content = lines.join(QLatin1Char('\n'));
            type = ContentClassifier::classify(mimeType, content, mime->formats());
            preview = ContentClassifier::generatePreview(type, content, mimeType, QString());
        }
    } else if (mime->hasText()) {
        mimeType = mime->hasFormat(QStringLiteral("text/plain"))
                       ? QStringLiteral("text/plain")
                       : QStringLiteral("text/plain;charset=utf-8");
        content = mime->text();
        if (content.trimmed().isEmpty() || ContentClassifier::isSensitiveText(content)) {
            return row;
        }
        type = ContentClassifier::classify(mimeType, content, mime->formats());
        preview = ContentClassifier::generatePreview(type, content, mimeType, QString());
    }

    if (content.trimmed().isEmpty() || type.isEmpty()) {
        return row;
    }

    row.insert(QStringLiteral("content"), content);
    row.insert(QStringLiteral("mimeType"), mimeType);
    row.insert(QStringLiteral("type"), type);
    row.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    row.insert(QStringLiteral("sourceApplication"), QString());
    row.insert(QStringLiteral("preview"), preview);
    row.insert(QStringLiteral("isPinned"), false);
    return row;
}

QString ClipboardWatcher::cacheImage(const QImage &image) const
{
    QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheBase.isEmpty()) {
        cacheBase = QDir::homePath() + QStringLiteral("/.cache/slm-desktop");
    }
    QDir dir(cacheBase);
    if (!dir.mkpath(QStringLiteral("desktop/clipboard"))) {
        return QString();
    }
    const QString fileName = QStringLiteral("clip-")
            + QString::number(QDateTime::currentMSecsSinceEpoch())
            + QStringLiteral(".png");
    const QString outPath = dir.filePath(QStringLiteral("desktop/clipboard/")) + fileName;
    if (!image.save(outPath, "PNG")) {
        return QString();
    }
    return outPath;
}

} // namespace Slm::Clipboard
