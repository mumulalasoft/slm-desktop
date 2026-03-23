#pragma once

#include <QObject>
#include <QVariantMap>
#include <QThread>
#include <memory>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct ext_data_control_manager_v1;
struct ext_data_control_device_v1;
struct ext_data_control_offer_v1;

namespace Slm::Clipboard {

class WaylandClipboardWatcher : public QObject
{
    Q_OBJECT

public:
    class Private;
    
    explicit WaylandClipboardWatcher(QObject *parent = nullptr);
    ~WaylandClipboardWatcher() override;

    bool init();
    void setSuppressed(bool suppressed);

signals:
    void itemCaptured(const QVariantMap &item);

private:
    std::unique_ptr<Private> d;
};

} // namespace Slm::Clipboard
