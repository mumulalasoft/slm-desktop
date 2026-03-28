#pragma once

#include <QObject>
#include <QImage>
#include <QClipboard>
#include <QVariantMap>
#include <QString>

namespace Slm::Clipboard {

class WaylandClipboardWatcher;

class ClipboardWatcher : public QObject
{
    Q_OBJECT

public:
    enum class BackendMode {
        QtFallback,
        NativeWayland
    };

    explicit ClipboardWatcher(QObject *parent = nullptr);

    void setSuppressed(bool suppressed);
    BackendMode backendMode() const;
    QString backendModeString() const;
    static BackendMode selectBackendMode(const QString &platformName, bool forceQtFallback)
    {
        if (forceQtFallback) {
            return BackendMode::QtFallback;
        }
        if (platformName.contains(QLatin1String("wayland"), Qt::CaseInsensitive)) {
            return BackendMode::NativeWayland;
        }
        return BackendMode::QtFallback;
    }

signals:
    void itemCaptured(const QVariantMap &item);

private slots:
    void onClipboardChanged(QClipboard::Mode mode);

private:
    QVariantMap readClipboardItem() const;
    QString cacheImage(const QImage &image) const;

    QClipboard *m_clipboard = nullptr;
    WaylandClipboardWatcher *m_wlWatcher = nullptr;
    BackendMode m_backendMode = BackendMode::QtFallback;
    bool m_suppressed = false;
};

} // namespace Slm::Clipboard
