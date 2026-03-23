#include "statusnotifierhost.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>
#include <QtEndian>
#include <QDebug>
#include <algorithm>

namespace {
constexpr const char *kWatcherPath = "/StatusNotifierWatcher";
constexpr const char *kItemInterface = "org.kde.StatusNotifierItem";
constexpr const char *kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr const char *kDefaultItemPath = "/StatusNotifierItem";
const QStringList kWatcherServices = {
    QStringLiteral("org.kde.StatusNotifierWatcher"),
    QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
    QStringLiteral("org.ayatana.StatusNotifierWatcher"),
};
}

StatusNotifierHost::StatusNotifierHost(QObject *parent)
    : QObject(parent)
{
    m_debugEnabled = qEnvironmentVariableIntValue("SLM_INDICATOR_DEBUG") > 0;
    m_metricsEnabled = qEnvironmentVariableIntValue("SLM_INDICATOR_METRICS") > 0;
    resetMetrics();

    auto bus = QDBusConnection::sessionBus();
    m_serviceWatcher = new QDBusServiceWatcher(this);
    m_serviceWatcher->setConnection(bus);
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &StatusNotifierHost::onWatchedServiceUnregistered);

    registerWatcherServices();
}

StatusNotifierHost::~StatusNotifierHost()
{
    auto bus = QDBusConnection::sessionBus();
    for (const QString &service : m_registeredWatcherServices) {
        bus.unregisterObject(QString::fromLatin1(kWatcherPath));
        bus.unregisterService(service);
    }
}

QVariantList StatusNotifierHost::entries() const
{
    QList<ItemEntry> sorted = m_items.values();
    std::sort(sorted.begin(), sorted.end(), [](const ItemEntry &a, const ItemEntry &b) {
        const QString at = a.title.trimmed().isEmpty() ? a.service : a.title;
        const QString bt = b.title.trimmed().isEmpty() ? b.service : b.title;
        return at.localeAwareCompare(bt) < 0;
    });

    QVariantList out;
    out.reserve(sorted.size());
    for (const ItemEntry &item : sorted) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), item.id);
        row.insert(QStringLiteral("service"), item.service);
        row.insert(QStringLiteral("path"), item.path);
        row.insert(QStringLiteral("title"), item.title);
        row.insert(QStringLiteral("iconName"), item.iconName);
        row.insert(QStringLiteral("iconSource"), item.iconSource);
        row.insert(QStringLiteral("status"), item.status);
        row.insert(QStringLiteral("menuPath"), item.menuPath);
        out.push_back(row);
    }
    return out;
}

QStringList StatusNotifierHost::registeredStatusNotifierItems() const
{
    return m_registeredItems;
}

bool StatusNotifierHost::isStatusNotifierHostRegistered() const
{
    return true;
}

int StatusNotifierHost::protocolVersion() const
{
    return 0;
}

void StatusNotifierHost::activate(const QString &itemId, int x, int y)
{
    const bool ok = callItemMethod(itemId, QStringLiteral("Activate"), x, y);
    if (m_metricsEnabled) {
        m_metrics[QStringLiteral("activate_calls")] = m_metrics.value(QStringLiteral("activate_calls")).toULongLong() + 1;
        m_metrics[QStringLiteral("activate_ok")] = m_metrics.value(QStringLiteral("activate_ok")).toULongLong() + (ok ? 1 : 0);
        m_metrics[QStringLiteral("activate_fail")] = m_metrics.value(QStringLiteral("activate_fail")).toULongLong() + (ok ? 0 : 1);
    }
    if (m_debugEnabled) {
        qInfo().noquote() << "[slm][indicator]" << "activate"
                          << "item=" << itemId
                          << "ok=" << (ok ? 1 : 0);
    }
}

void StatusNotifierHost::secondaryActivate(const QString &itemId, int x, int y)
{
    const bool ok = callItemMethod(itemId, QStringLiteral("SecondaryActivate"), x, y);
    if (m_metricsEnabled) {
        m_metrics[QStringLiteral("secondary_calls")] = m_metrics.value(QStringLiteral("secondary_calls")).toULongLong() + 1;
        m_metrics[QStringLiteral("secondary_ok")] = m_metrics.value(QStringLiteral("secondary_ok")).toULongLong() + (ok ? 1 : 0);
        m_metrics[QStringLiteral("secondary_fail")] = m_metrics.value(QStringLiteral("secondary_fail")).toULongLong() + (ok ? 0 : 1);
    }
    if (m_debugEnabled) {
        qInfo().noquote() << "[slm][indicator]" << "secondary"
                          << "item=" << itemId
                          << "ok=" << (ok ? 1 : 0);
    }
}

void StatusNotifierHost::contextMenu(const QString &itemId, int x, int y)
{
    if (m_metricsEnabled) {
        m_metrics[QStringLiteral("context_calls")] = m_metrics.value(QStringLiteral("context_calls")).toULongLong() + 1;
    }
    // Deterministic fallback chain for items without DBusMenu support:
    // ContextMenu -> SecondaryActivate -> Activate.
    if (callItemMethod(itemId, QStringLiteral("ContextMenu"), x, y)) {
        if (m_metricsEnabled) {
            m_metrics[QStringLiteral("context_path_contextmenu")] =
                m_metrics.value(QStringLiteral("context_path_contextmenu")).toULongLong() + 1;
        }
        if (m_debugEnabled) {
            qInfo().noquote() << "[slm][indicator]" << "context"
                              << "item=" << itemId
                              << "path=ContextMenu";
        }
        return;
    }
    if (callItemMethod(itemId, QStringLiteral("SecondaryActivate"), x, y)) {
        if (m_metricsEnabled) {
            m_metrics[QStringLiteral("context_path_secondary")] =
                m_metrics.value(QStringLiteral("context_path_secondary")).toULongLong() + 1;
        }
        if (m_debugEnabled) {
            qInfo().noquote() << "[slm][indicator]" << "context"
                              << "item=" << itemId
                              << "path=SecondaryActivate";
        }
        return;
    }
    const bool activated = callItemMethod(itemId, QStringLiteral("Activate"), x, y);
    if (m_metricsEnabled) {
        if (activated) {
            m_metrics[QStringLiteral("context_path_activate")] =
                m_metrics.value(QStringLiteral("context_path_activate")).toULongLong() + 1;
        } else {
            m_metrics[QStringLiteral("context_path_noaction")] =
                m_metrics.value(QStringLiteral("context_path_noaction")).toULongLong() + 1;
        }
    }
    if (m_debugEnabled) {
        qInfo().noquote() << "[slm][indicator]" << "context"
                          << "item=" << itemId
                          << "path=Activate"
                          << "ok=" << (activated ? 1 : 0);
    }
}

void StatusNotifierHost::scroll(const QString &itemId, int delta, const QString &orientation)
{
    const auto it = m_items.constFind(itemId);
    if (it == m_items.constEnd()) {
        if (m_metricsEnabled) {
            m_metrics[QStringLiteral("scroll_fail")] = m_metrics.value(QStringLiteral("scroll_fail")).toULongLong() + 1;
        }
        return;
    }

    const ItemEntry &item = it.value();
    auto bus = QDBusConnection::sessionBus();
    QDBusInterface iface(item.service, item.path, QString::fromLatin1(kItemInterface), bus);
    if (!iface.isValid()) {
        if (m_metricsEnabled) {
            m_metrics[QStringLiteral("scroll_fail")] = m_metrics.value(QStringLiteral("scroll_fail")).toULongLong() + 1;
        }
        return;
    }
    iface.call(QDBus::NoBlock,
               QStringLiteral("Scroll"),
               delta,
               orientation.trimmed().isEmpty() ? QStringLiteral("vertical") : orientation.trimmed());
    if (m_metricsEnabled) {
        m_metrics[QStringLiteral("scroll_calls")] = m_metrics.value(QStringLiteral("scroll_calls")).toULongLong() + 1;
    }
}

QVariantMap StatusNotifierHost::metricsSnapshot() const
{
    QVariantMap out = m_metrics;
    out.insert(QStringLiteral("enabled"), m_metricsEnabled);
    return out;
}

void StatusNotifierHost::resetMetrics()
{
    m_metrics.clear();
    m_metrics.insert(QStringLiteral("activate_calls"), 0);
    m_metrics.insert(QStringLiteral("activate_ok"), 0);
    m_metrics.insert(QStringLiteral("activate_fail"), 0);
    m_metrics.insert(QStringLiteral("secondary_calls"), 0);
    m_metrics.insert(QStringLiteral("secondary_ok"), 0);
    m_metrics.insert(QStringLiteral("secondary_fail"), 0);
    m_metrics.insert(QStringLiteral("context_calls"), 0);
    m_metrics.insert(QStringLiteral("context_path_contextmenu"), 0);
    m_metrics.insert(QStringLiteral("context_path_secondary"), 0);
    m_metrics.insert(QStringLiteral("context_path_activate"), 0);
    m_metrics.insert(QStringLiteral("context_path_noaction"), 0);
    m_metrics.insert(QStringLiteral("scroll_calls"), 0);
    m_metrics.insert(QStringLiteral("scroll_fail"), 0);
}

void StatusNotifierHost::RegisterStatusNotifierItem(const QString &serviceOrPath)
{
    QString service = serviceOrPath.trimmed();
    QString path = QString::fromLatin1(kDefaultItemPath);
    const QString sender = message().service();

    if (service.startsWith(QLatin1Char('/'))) {
        path = normalizePath(service);
        service = sender;
    } else if (service.isEmpty()) {
        service = sender;
    }

    if (service.isEmpty()) {
        return;
    }

    addOrUpdateItem(service, path);
}

void StatusNotifierHost::RegisterStatusNotifierHost(const QString &service)
{
    Q_UNUSED(service)
    emit StatusNotifierHostRegistered();
}

void StatusNotifierHost::onWatchedServiceUnregistered(const QString &serviceName)
{
    QStringList removeIds;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        if (it.value().service == serviceName) {
            removeIds.push_back(it.key());
        }
    }

    for (const QString &id : removeIds) {
        removeItem(id);
    }
}

void StatusNotifierHost::onItemSignalPing()
{
    const QString service = message().service();
    const QString path = normalizePath(message().path());
    if (service.isEmpty() || path.isEmpty()) {
        return;
    }
    const QString id = makeItemId(service, path);
    if (m_items.contains(id)) {
        refreshItem(id);
    }
}

QString StatusNotifierHost::normalizePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString::fromLatin1(kDefaultItemPath);
    }
    if (!trimmed.startsWith(QLatin1Char('/'))) {
        return QString::fromLatin1(kDefaultItemPath);
    }
    return trimmed;
}

QString StatusNotifierHost::makeItemId(const QString &service, const QString &path)
{
    return service + path;
}

QString StatusNotifierHost::dbusObjectPathToString(const QVariant &value)
{
    if (value.canConvert<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    const QString s = value.toString().trimmed();
    return s.startsWith(QLatin1Char('/')) ? s : QString();
}

QString StatusNotifierHost::iconPixmapToDataUri(const QVariant &value)
{
    if (!value.isValid() || !value.canConvert<QDBusArgument>()) {
        return QString();
    }

    QDBusArgument arg = value.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::ArrayType) {
        return QString();
    }

    struct Candidate {
        int width = 0;
        int height = 0;
        QByteArray bytes;
    };
    Candidate best;

    arg.beginArray();
    while (!arg.atEnd()) {
        int width = 0;
        int height = 0;
        QByteArray bytes;
        arg.beginStructure();
        arg >> width >> height >> bytes;
        arg.endStructure();

        if (width <= 0 || height <= 0 || bytes.size() < (width * height * 4)) {
            continue;
        }
        const int area = width * height;
        const int bestArea = best.width * best.height;
        if (area > bestArea) {
            best.width = width;
            best.height = height;
            best.bytes = bytes;
        }
    }
    arg.endArray();

    if (best.width <= 0 || best.height <= 0 || best.bytes.isEmpty()) {
        return QString();
    }

    QImage image(best.width, best.height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return QString();
    }

    const int required = best.width * best.height * 4;
    if (best.bytes.size() < required) {
        return QString();
    }

    const uchar *src = reinterpret_cast<const uchar *>(best.bytes.constData());
    for (int y = 0; y < best.height; ++y) {
        QRgb *dst = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < best.width; ++x) {
            const quint32 packed = qFromBigEndian<quint32>(src);
            src += 4;
            const int a = (packed >> 24) & 0xff;
            const int r = (packed >> 16) & 0xff;
            const int g = (packed >> 8) & 0xff;
            const int b = packed & 0xff;
            dst[x] = qRgba(r, g, b, a);
        }
    }

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return QString();
    }
    if (!image.save(&buffer, "PNG")) {
        return QString();
    }

    return QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(pngBytes.toBase64()));
}

QString StatusNotifierHost::toolTipToText(const QVariant &value)
{
    if (!value.isValid() || !value.canConvert<QDBusArgument>()) {
        return QString();
    }

    QDBusArgument arg = value.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::StructureType) {
        return QString();
    }

    QString iconName;
    QVariant iconPixmap;
    QString title;
    QString description;
    arg.beginStructure();
    arg >> iconName >> iconPixmap >> title >> description;
    arg.endStructure();
    Q_UNUSED(iconName)
    Q_UNUSED(iconPixmap)

    const QString t = title.trimmed();
    const QString d = description.trimmed();
    if (!t.isEmpty()) {
        return t;
    }
    if (!d.isEmpty()) {
        return d;
    }
    return QString();
}

QString StatusNotifierHost::resolveIconFromThemePath(const QString &iconName,
                                                     const QString &iconThemePath)
{
    const QString name = iconName.trimmed();
    const QString paths = iconThemePath.trimmed();
    if (name.isEmpty() || paths.isEmpty()) {
        return QString();
    }

    auto tryFile = [](const QString &candidate) -> QString {
        QFileInfo info(candidate);
        if (!info.exists() || !info.isFile()) {
            return QString();
        }
        return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
    };

    if (name.startsWith(QLatin1Char('/'))) {
        const QString direct = tryFile(name);
        if (!direct.isEmpty()) {
            return direct;
        }
    }

    QStringList roots = paths.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &chunk : paths.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = chunk.trimmed();
        if (!trimmed.isEmpty() && !roots.contains(trimmed)) {
            roots.push_back(trimmed);
        }
    }
    for (QString &root : roots) {
        root = root.trimmed();
    }
    roots.erase(std::remove_if(roots.begin(), roots.end(), [](const QString &s) { return s.isEmpty(); }),
                roots.end());

    const bool hasExt = name.contains(QLatin1Char('.'));
    const QStringList exts = hasExt
                                 ? QStringList{QString()}
                                 : QStringList{QStringLiteral(".svg"),
                                               QStringLiteral(".png"),
                                               QStringLiteral(".xpm")};
    const QStringList sizeDirs = {
        QStringLiteral("scalable"),
        QStringLiteral("symbolic"),
        QStringLiteral("512x512"),
        QStringLiteral("256x256"),
        QStringLiteral("128x128"),
        QStringLiteral("64x64"),
        QStringLiteral("48x48"),
        QStringLiteral("32x32"),
        QStringLiteral("24x24"),
        QStringLiteral("22x22"),
        QStringLiteral("16x16")
    };
    const QStringList contexts = {
        QStringLiteral("status"),
        QStringLiteral("apps"),
        QStringLiteral("panel"),
        QStringLiteral("actions"),
        QStringLiteral("devices")
    };

    for (const QString &root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        const QDir dir(root);
        for (const QString &ext : exts) {
            const QString direct = tryFile(dir.filePath(name + ext));
            if (!direct.isEmpty()) {
                return direct;
            }
        }
        for (const QString &sizeDir : sizeDirs) {
            for (const QString &ctx : contexts) {
                for (const QString &ext : exts) {
                    const QString candidate = tryFile(dir.filePath(sizeDir + QLatin1Char('/')
                                                                   + ctx + QLatin1Char('/')
                                                                   + name + ext));
                    if (!candidate.isEmpty()) {
                        return candidate;
                    }
                }
            }
        }
    }

    return QString();
}

void StatusNotifierHost::registerWatcherServices()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    const bool objectOk = bus.registerObject(QString::fromLatin1(kWatcherPath),
                                             this,
                                             QDBusConnection::ExportAllSlots |
                                                 QDBusConnection::ExportAllSignals |
                                                 QDBusConnection::ExportAllProperties);
    if (!objectOk) {
        return;
    }

    for (const QString &service : kWatcherServices) {
        if (bus.registerService(service)) {
            m_registeredWatcherServices.push_back(service);
        }
    }
}

void StatusNotifierHost::addOrUpdateItem(const QString &service, const QString &path)
{
    const QString normalizedPath = normalizePath(path);
    const QString itemId = makeItemId(service, normalizedPath);

    ItemEntry &item = m_items[itemId];
    item.id = itemId;
    item.service = service;
    item.path = normalizedPath;
    if (item.iconThemePath.trimmed().isEmpty()) {
        item.iconThemePath.clear();
    }
    if (item.iconName.trimmed().isEmpty()) {
        item.iconName = QStringLiteral("application-x-executable-symbolic");
    }
    if (item.title.trimmed().isEmpty()) {
        item.title = service;
    }
    if (item.status.trimmed().isEmpty()) {
        item.status = QStringLiteral("Active");
    }

    if (!m_registeredItems.contains(itemId)) {
        m_registeredItems.push_back(itemId);
        emit registeredStatusNotifierItemsChanged();
        emit StatusNotifierItemRegistered(itemId);
    }

    if (m_serviceWatcher) {
        const auto watched = m_serviceWatcher->watchedServices();
        if (!watched.contains(service)) {
            QStringList next = watched;
            next.push_back(service);
            m_serviceWatcher->setWatchedServices(next);
        }
    }

    subscribeItemSignals(itemId);
    refreshItem(itemId);
    emitEntriesChanged();
}

void StatusNotifierHost::removeItem(const QString &itemId)
{
    if (!m_items.remove(itemId)) {
        return;
    }
    m_subscribedItems.remove(itemId);
    m_registeredItems.removeAll(itemId);
    emit registeredStatusNotifierItemsChanged();
    emit StatusNotifierItemUnregistered(itemId);
    emitEntriesChanged();
}

void StatusNotifierHost::refreshItem(const QString &itemId)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end()) {
        return;
    }

    const ItemEntry item = it.value();
    auto bus = QDBusConnection::sessionBus();
    QDBusInterface props(item.service, item.path, QString::fromLatin1(kPropertiesInterface), bus);
    QDBusReply<QVariantMap> reply = props.call(QStringLiteral("GetAll"), QString::fromLatin1(kItemInterface));
    if (!reply.isValid()) {
        return;
    }

    const QVariantMap all = reply.value();
    ItemEntry updated = item;

    const QString iconName = all.value(QStringLiteral("IconName")).toString().trimmed();
    const QString attentionIconName = all.value(QStringLiteral("AttentionIconName")).toString().trimmed();
    const QString iconThemePath = all.value(QStringLiteral("IconThemePath")).toString().trimmed();
    if (!iconThemePath.isEmpty()) {
        updated.iconThemePath = iconThemePath;
    }
    if (!iconName.isEmpty()) {
        updated.iconName = iconName;
        const QString themed = resolveIconFromThemePath(updated.iconName, updated.iconThemePath);
        updated.iconSource = themed;
    } else {
        const QString iconSource = iconPixmapToDataUri(all.value(QStringLiteral("IconPixmap")));
        if (!iconSource.isEmpty()) {
            updated.iconSource = iconSource;
        }
    }

    const QString title = all.value(QStringLiteral("Title")).toString().trimmed();
    const QString idName = all.value(QStringLiteral("Id")).toString().trimmed();
    const QString toolTipText = toolTipToText(all.value(QStringLiteral("ToolTip")));
    if (!title.isEmpty()) {
        updated.title = title;
    } else if (!idName.isEmpty()) {
        updated.title = idName;
    } else if (!toolTipText.isEmpty()) {
        updated.title = toolTipText;
    } else {
        updated.title = updated.service;
    }

    const QString status = all.value(QStringLiteral("Status")).toString().trimmed();
    if (!status.isEmpty()) {
        updated.status = status;
    }

    const QString menuPath = dbusObjectPathToString(all.value(QStringLiteral("Menu")));
    updated.menuPath = menuPath;

    if (updated.iconName.isEmpty()) {
        if (!attentionIconName.isEmpty() &&
            updated.status.compare(QStringLiteral("NeedsAttention"), Qt::CaseInsensitive) == 0) {
            updated.iconName = attentionIconName;
            const QString themed = resolveIconFromThemePath(updated.iconName, updated.iconThemePath);
            if (!themed.isEmpty()) {
                updated.iconSource = themed;
            }
        } else {
            updated.iconName = QStringLiteral("application-x-executable-symbolic");
        }
    } else if (updated.iconSource.isEmpty()) {
        const QString themed = resolveIconFromThemePath(updated.iconName, updated.iconThemePath);
        if (!themed.isEmpty()) {
            updated.iconSource = themed;
        }
    }
    m_items[itemId] = updated;
}

void StatusNotifierHost::subscribeItemSignals(const QString &itemId)
{
    if (m_subscribedItems.contains(itemId)) {
        return;
    }
    const auto it = m_items.constFind(itemId);
    if (it == m_items.constEnd()) {
        return;
    }

    const QString service = it.value().service;
    const QString path = it.value().path;
    auto bus = QDBusConnection::sessionBus();

    const bool iconConnected = bus.connect(service,
                                           path,
                                           QString::fromLatin1(kItemInterface),
                                           QStringLiteral("NewIcon"),
                                           this,
                                           SLOT(onItemSignalPing()));
    const bool titleConnected = bus.connect(service,
                                            path,
                                            QString::fromLatin1(kItemInterface),
                                            QStringLiteral("NewTitle"),
                                            this,
                                            SLOT(onItemSignalPing()));
    const bool statusConnected = bus.connect(service,
                                             path,
                                             QString::fromLatin1(kItemInterface),
                                             QStringLiteral("NewStatus"),
                                             this,
                                             SLOT(onItemSignalPing()));
    const bool tooltipConnected = bus.connect(service,
                                              path,
                                              QString::fromLatin1(kItemInterface),
                                              QStringLiteral("NewToolTip"),
                                              this,
                                              SLOT(onItemSignalPing()));
    const bool menuConnected = bus.connect(service,
                                           path,
                                           QString::fromLatin1(kItemInterface),
                                           QStringLiteral("NewMenu"),
                                           this,
                                           SLOT(onItemSignalPing()));
    const bool attentionConnected = bus.connect(service,
                                                path,
                                                QString::fromLatin1(kItemInterface),
                                                QStringLiteral("NewAttentionIcon"),
                                                this,
                                                SLOT(onItemSignalPing()));
    const bool overlayConnected = bus.connect(service,
                                              path,
                                              QString::fromLatin1(kItemInterface),
                                              QStringLiteral("NewOverlayIcon"),
                                              this,
                                              SLOT(onItemSignalPing()));
    const bool themePathConnected = bus.connect(service,
                                                path,
                                                QString::fromLatin1(kItemInterface),
                                                QStringLiteral("NewIconThemePath"),
                                                this,
                                                SLOT(onItemSignalPing()));

    if (iconConnected || titleConnected || statusConnected ||
        tooltipConnected || menuConnected || attentionConnected ||
        overlayConnected || themePathConnected) {
        m_subscribedItems.insert(itemId);
    }
}

void StatusNotifierHost::emitEntriesChanged()
{
    emit entriesChanged();
}

bool StatusNotifierHost::callItemMethod(const QString &itemId, const QString &method, int x, int y)
{
    const auto it = m_items.constFind(itemId);
    if (it == m_items.constEnd()) {
        return false;
    }

    const ItemEntry &item = it.value();
    auto bus = QDBusConnection::sessionBus();
    QDBusInterface iface(item.service, item.path, QString::fromLatin1(kItemInterface), bus);
    if (!iface.isValid()) {
        return false;
    }
    iface.setTimeout(120);
    const QDBusMessage reply = iface.call(method, x, y);
    return reply.type() != QDBusMessage::ErrorMessage;
}
