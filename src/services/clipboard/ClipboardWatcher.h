#pragma once

#include <QObject>
#include <QImage>
#include <QClipboard>
#include <QVariantMap>

namespace Slm::Clipboard {

class WaylandClipboardWatcher;

class ClipboardWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardWatcher(QObject *parent = nullptr);

    void setSuppressed(bool suppressed);

signals:
    void itemCaptured(const QVariantMap &item);

private slots:
    void onClipboardChanged(QClipboard::Mode mode);

private:
    QVariantMap readClipboardItem() const;
    QString cacheImage(const QImage &image) const;

    QClipboard *m_clipboard = nullptr;
    WaylandClipboardWatcher *m_wlWatcher = nullptr;
    bool m_suppressed = false;
};

} // namespace Slm::Clipboard
