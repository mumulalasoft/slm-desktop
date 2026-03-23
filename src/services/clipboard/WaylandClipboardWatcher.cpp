#include "WaylandClipboardWatcher.h"
#include "contentclassifier.h"
#include "clipboardtypes.h"

#include "wayland-client-protocol.h"
#include "ext-data-control-v1-client-protocol.h"

#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QMap>
#include <QSocketNotifier>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace Slm::Clipboard {

struct WaylandOffer {
    ext_data_control_offer_v1 *wl_offer = nullptr;
    QStringList mimeTypes;
};

class WaylandClipboardWatcher::Private {
public:
    WaylandClipboardWatcher *q;
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    ext_data_control_manager_v1 *manager = nullptr;
    QList<wl_seat*> seats;
    QList<ext_data_control_device_v1*> devices;
    QSocketNotifier *notifier = nullptr;
    bool suppressed = false;

    QMap<ext_data_control_offer_v1*, WaylandOffer*> pendingOffers;
    WaylandOffer *currentOffer = nullptr;

    Private(WaylandClipboardWatcher *parent) : q(parent) {}

    ~Private() {
        if (currentOffer) {
            ext_data_control_offer_v1_destroy(currentOffer->wl_offer);
            delete currentOffer;
        }
        for (auto *o : pendingOffers) {
            ext_data_control_offer_v1_destroy(o->wl_offer);
            delete o;
        }
        for (auto *d : devices) ext_data_control_device_v1_destroy(d);
        if (manager) ext_data_control_manager_v1_destroy(manager);
        if (registry) wl_registry_destroy(registry);
        if (display) wl_display_disconnect(display);
    }

    void onSelectionChanged(ext_data_control_offer_v1 *offer) {
        if (suppressed) return;
        
        auto *wOffer = currentOffer;
        if (!wOffer || wOffer->wl_offer != offer) return;

        QString bestMime;
        if (wOffer->mimeTypes.contains("text/plain;charset=utf-8")) bestMime = "text/plain;charset=utf-8";
        else if (wOffer->mimeTypes.contains("text/plain")) bestMime = "text/plain";
        else if (wOffer->mimeTypes.contains("image/png")) bestMime = "image/png";
        else if (wOffer->mimeTypes.contains("text/uri-list")) bestMime = "text/uri-list";

        if (!bestMime.isEmpty()) {
            readDataFromOffer(offer, bestMime);
        }
    }

    void readDataFromOffer(ext_data_control_offer_v1 *offer, const QString &mimeType) {
        int fds[2];
        if (pipe(fds) == -1) return;

        ext_data_control_offer_v1_receive(offer, mimeType.toUtf8().constData(), fds[1]);
        close(fds[1]);
        wl_display_flush(display);

        QByteArray data;
        char buf[4096];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
            data.append(buf, n);
        }
        close(fds[0]);

        if (data.isEmpty()) return;

        QVariantMap item;
        QString type;
        QString content;
        QString preview;

        if (mimeType.startsWith("text/plain")) {
            content = QString::fromUtf8(data);
            if (content.trimmed().isEmpty() || ContentClassifier::isSensitiveText(content)) return;
            type = ContentClassifier::classify(mimeType, content, currentOffer->mimeTypes);
            preview = ContentClassifier::generatePreview(type, content, mimeType, QString());
        } else if (mimeType == "text/uri-list") {
            content = QString::fromUtf8(data);
            type = ContentClassifier::classify(mimeType, content, currentOffer->mimeTypes);
            preview = ContentClassifier::generatePreview(type, content, mimeType, QString());
        } else if (mimeType == "image/png") {
            QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            if (cacheBase.isEmpty()) cacheBase = QDir::homePath() + "/.cache/slm-desktop";
            QDir dir(cacheBase);
            dir.mkpath("desktop/clipboard");
            
            QString fileName = QString("clip-") + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
            QString outPath = dir.filePath("desktop/clipboard/") + fileName;
            
            QFile file(outPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                content = outPath;
                type = QString::fromLatin1(kTypeImage);
                preview = ContentClassifier::generatePreview(type, QString(), mimeType, content);
            }
        }

        if (type.isEmpty() || content.isEmpty()) return;

        item.insert("content", content);
        item.insert("mimeType", mimeType);
        item.insert("type", type);
        item.insert("timestamp", QDateTime::currentMSecsSinceEpoch());
        item.insert("sourceApplication", QString());
        item.insert("preview", preview);
        item.insert("isPinned", false);

        emit q->itemCaptured(item);
    }
};

static void offer_handle_offer(void *data, struct ext_data_control_offer_v1 *id, const char *mime_type) {
    auto *offer = static_cast<WaylandOffer*>(data);
    offer->mimeTypes.append(QString::fromUtf8(mime_type));
}

static const struct ext_data_control_offer_v1_listener offer_listener = {
    offer_handle_offer
};

static void device_handle_data_offer(void *data, struct ext_data_control_device_v1 *ext_data_control_device_v1, struct ext_data_control_offer_v1 *id) {
    auto *d = static_cast<WaylandClipboardWatcher::Private*>(data);
    auto *offer = new WaylandOffer();
    offer->wl_offer = id;
    d->pendingOffers.insert(id, offer);
    ext_data_control_offer_v1_add_listener(id, &offer_listener, offer);
}

static void device_handle_selection(void *data, struct ext_data_control_device_v1 *ext_data_control_device_v1, struct ext_data_control_offer_v1 *id) {
    auto *d = static_cast<WaylandClipboardWatcher::Private*>(data);
    
    if (d->currentOffer) {
        ext_data_control_offer_v1_destroy(d->currentOffer->wl_offer);
        delete d->currentOffer;
        d->currentOffer = nullptr;
    }

    if (!id) return;

    auto *offer = d->pendingOffers.take(id);
    if (!offer) {
        offer = new WaylandOffer();
        offer->wl_offer = id;
    }

    d->currentOffer = offer;
    d->onSelectionChanged(id);
}

static void device_handle_finished(void *data, struct ext_data_control_device_v1 *ext_data_control_device_v1) {
    Q_UNUSED(data) Q_UNUSED(ext_data_control_device_v1)
}

static void device_handle_primary_selection(void *data, struct ext_data_control_device_v1 *ext_data_control_device_v1, struct ext_data_control_offer_v1 *id) {
    Q_UNUSED(data) Q_UNUSED(ext_data_control_device_v1)
    if (id) ext_data_control_offer_v1_destroy(id);
}

static const struct ext_data_control_device_v1_listener device_listener = {
    device_handle_data_offer,
    device_handle_selection,
    device_handle_finished,
    device_handle_primary_selection
};

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    auto *d = static_cast<WaylandClipboardWatcher::Private*>(data);
    if (strcmp(interface, ext_data_control_manager_v1_interface.name) == 0) {
        d->manager = static_cast<ext_data_control_manager_v1*>(wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, 1));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        auto *seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        d->seats.append(seat);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    Q_UNUSED(data) Q_UNUSED(registry) Q_UNUSED(name)
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

WaylandClipboardWatcher::WaylandClipboardWatcher(QObject *parent)
    : QObject(parent)
{
}

WaylandClipboardWatcher::~WaylandClipboardWatcher() = default;

bool WaylandClipboardWatcher::init() {
    if (d) return true;
    
    d = std::make_unique<Private>(this);

    d->display = wl_display_connect(nullptr);
    if (!d->display) {
        d.reset();
        return false;
    }

    d->registry = wl_display_get_registry(d->display);
    wl_registry_add_listener(d->registry, &registry_listener, d.get());

    wl_display_roundtrip(d->display);

    if (!d->manager) {
        qWarning() << "[WaylandClipboardWatcher] ext_data_control_manager_v1 not supported";
        d.reset();
        return false;
    }

    for (auto *seat : d->seats) {
        auto *device = ext_data_control_manager_v1_get_data_device(d->manager, seat);
        ext_data_control_device_v1_add_listener(device, &device_listener, d.get());
        d->devices.append(device);
    }

    wl_display_flush(d->display);

    d->notifier = new QSocketNotifier(wl_display_get_fd(d->display), QSocketNotifier::Read, this);
    connect(d->notifier, &QSocketNotifier::activated, this, [this] {
        while (wl_display_prepare_read(d->display) != 0) {
            wl_display_dispatch_pending(d->display);
        }
        wl_display_flush(d->display);
        if (wl_display_read_events(d->display) == 0) {
            wl_display_dispatch_pending(d->display);
        } else {
            wl_display_cancel_read(d->display);
        }
    });

    return true;
}

void WaylandClipboardWatcher::setSuppressed(bool suppressed) {
    if (d) d->suppressed = suppressed;
}

} // namespace Slm::Clipboard
