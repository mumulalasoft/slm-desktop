#include "globalmenumanager.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QSequentialIterable>
#include <QStandardPaths>
#include <limits>
#include <utility>

namespace {
constexpr const char *kRegistrarService = "com.canonical.AppMenu.Registrar";
constexpr const char *kRegistrarPath = "/com/canonical/AppMenu/Registrar";
constexpr const char *kRegistrarIface = "com.canonical.AppMenu.Registrar";
constexpr const char *kKdeRegistrarService = "org.kde.kappmenu";
constexpr const char *kKdeRegistrarPath = "/KAppMenu";
constexpr const char *kKdeRegistrarIface = "org.kde.kappmenu";
constexpr const char *kMenuIface = "com.canonical.dbusmenu";
constexpr const char *kSessionStateService = "org.slm.SessionState";
constexpr const char *kSessionStatePath = "/org/slm/SessionState";
constexpr const char *kSessionStateIface = "org.slm.SessionState1";
constexpr const char *kSlmMenuService = "org.slm.Desktop.GlobalMenu";
constexpr const char *kSlmMenuPath = "/org/slm/Desktop/GlobalMenu";
constexpr const char *kSlmMenuIface = "org.slm.Desktop.GlobalMenu";
constexpr int kPollFastMs = 700;
constexpr int kPollIdleMs = 2400;
constexpr int kExternalMenuDebounceMs = 45;
constexpr int kMaxDbusMenuIconBytes = 512 * 1024;
constexpr int kMaxDbusMenuIconBase64Chars = kMaxDbusMenuIconBytes * 2;
} // namespace

class ExternalMenuSignalProxy final : public QObject {
    Q_OBJECT
public:
    ExternalMenuSignalProxy(GlobalMenuManager *owner,
                            QString service,
                            QString path,
                            QObject *parent = nullptr)
        : QObject(parent)
        , m_owner(owner)
        , m_service(std::move(service))
        , m_path(std::move(path))
    {
    }

public slots:
    void onLayoutUpdated(int, int)
    {
        if (m_owner) {
            m_owner->notifyExternalMenuChanged(m_service, m_path);
        }
    }

    void onItemsPropertiesUpdated(const QDBusArgument &, const QDBusArgument &)
    {
        if (m_owner) {
            m_owner->notifyExternalMenuChanged(m_service, m_path);
        }
    }

    void onItemActivationRequested(int, uint)
    {
        if (m_owner) {
            m_owner->notifyExternalMenuChanged(m_service, m_path);
        }
    }

private:
    GlobalMenuManager *m_owner = nullptr;
    QString m_service;
    QString m_path;
};

struct DbusMenuNode {
    int id = 0;
    QVariantMap properties;
    QList<DbusMenuNode> children;
};

const QDBusArgument &operator>>(const QDBusArgument &argument, DbusMenuNode &node)
{
    node = DbusMenuNode();
    argument.beginStructure();
    argument >> node.id;
    argument >> node.properties;
    argument.beginArray();
    while (!argument.atEnd()) {
        DbusMenuNode child;
        argument >> child;
        node.children.push_back(child);
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

struct RegistrarMenuNode {
    uint windowId = 0;
    QString service;
    QDBusObjectPath path;
};

const QDBusArgument &operator>>(const QDBusArgument &argument, RegistrarMenuNode &node)
{
    node = RegistrarMenuNode();
    argument.beginStructure();
    argument >> node.windowId;
    argument >> node.service;
    argument >> node.path;
    argument.endStructure();
    return argument;
}

static QString parseDbusShortcutText(const QVariant &shortcutVariant)
{
    // DBusMenu "shortcut" can be nested arrays, e.g. [["Ctrl","Q"]].
    const auto flattenTokens = [](const QVariant &value, auto &&self) -> QStringList {
        QStringList out;
        if (value.metaType().id() == QMetaType::QString) {
            const QString token = value.toString().trimmed();
            if (!token.isEmpty()) {
                out << token;
            }
            return out;
        }
        if (value.canConvert<QVariantList>()) {
            for (const QVariant &item : value.toList()) {
                out << self(item, self);
            }
            return out;
        }
        if (value.canConvert<QSequentialIterable>()) {
            const QSequentialIterable seq = value.value<QSequentialIterable>();
            for (const QVariant &item : seq) {
                out << self(item, self);
            }
            return out;
        }
        const QString fallback = value.toString().trimmed();
        if (!fallback.isEmpty()) {
            out << fallback;
        }
        return out;
    };

    const QStringList tokens = flattenTokens(shortcutVariant, flattenTokens);
    return tokens.join(QLatin1Char('+'));
}

GlobalMenuManager::GlobalMenuManager(QObject *parent)
    : QObject(parent)
{
    m_debugEnabled = qEnvironmentVariableIntValue("SLM_GLOBALMENU_DEBUG") > 0;

    QDBusConnection::sessionBus().connect(QString::fromLatin1(kSessionStateService),
                                          QString::fromLatin1(kSessionStatePath),
                                          QString::fromLatin1(kSessionStateIface),
                                          QStringLiteral("ActiveAppChanged"),
                                          this,
                                          SLOT(onSessionActiveAppChanged(QString)));

    m_timer = new QTimer(this);
    m_timer->setInterval(kPollIdleMs);
    connect(m_timer, &QTimer::timeout, this, &GlobalMenuManager::refresh);
    m_timer->start();

    m_externalMenuDebounceTimer = new QTimer(this);
    m_externalMenuDebounceTimer->setSingleShot(true);
    m_externalMenuDebounceTimer->setInterval(kExternalMenuDebounceMs);
    connect(m_externalMenuDebounceTimer,
            &QTimer::timeout,
            this,
            &GlobalMenuManager::flushExternalMenuChanges);

    m_canonicalRegistrarWatcher = new QDBusServiceWatcher(QString::fromLatin1(kRegistrarService),
                                                          QDBusConnection::sessionBus(),
                                                          QDBusServiceWatcher::WatchForRegistration
                                                              | QDBusServiceWatcher::WatchForUnregistration
                                                              | QDBusServiceWatcher::WatchForOwnerChange,
                                                          this);
    connect(m_canonicalRegistrarWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this](const QString &) { refresh(); });
    connect(m_canonicalRegistrarWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) { refresh(); });
    connect(m_canonicalRegistrarWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this](const QString &, const QString &, const QString &) {
                m_connectionPidCache.clear();
                refresh();
            });

    m_kdeRegistrarWatcher = new QDBusServiceWatcher(QString::fromLatin1(kKdeRegistrarService),
                                                    QDBusConnection::sessionBus(),
                                                    QDBusServiceWatcher::WatchForRegistration
                                                        | QDBusServiceWatcher::WatchForUnregistration
                                                        | QDBusServiceWatcher::WatchForOwnerChange,
                                                    this);
    connect(m_kdeRegistrarWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this](const QString &) { refresh(); });
    connect(m_kdeRegistrarWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) { refresh(); });
    connect(m_kdeRegistrarWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this](const QString &, const QString &, const QString &) {
                m_connectionPidCache.clear();
                refresh();
            });

    m_slmMenuWatcher = new QDBusServiceWatcher(QString::fromLatin1(kSlmMenuService),
                                               QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForRegistration
                                                   | QDBusServiceWatcher::WatchForUnregistration
                                                   | QDBusServiceWatcher::WatchForOwnerChange,
                                               this);
    connect(m_slmMenuWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this](const QString &) { refresh(); });
    connect(m_slmMenuWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) { refresh(); });
    connect(m_slmMenuWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this](const QString &, const QString &, const QString &) { refresh(); });

    refresh();
}

bool GlobalMenuManager::available() const
{
    return m_available;
}

QVariantList GlobalMenuManager::topLevelMenus() const
{
    return m_topLevelMenus;
}

QString GlobalMenuManager::activeMenuService() const
{
    return m_activeMenuService;
}

QString GlobalMenuManager::activeAppId() const
{
    return m_lastActiveAppId;
}

bool GlobalMenuManager::appSwitching() const
{
    return m_appSwitching;
}

void GlobalMenuManager::refresh()
{
    const bool oldAvailable = m_available;
    const QVariantList oldMenus = m_topLevelMenus;
    const QString oldService = m_activeMenuService;
    const QString oldPath = m_activeMenuPath;
    const quint32 oldWindowId = m_activeWindowId;
    const QString oldSource = m_lastSelectionSource;
    const int oldScore = m_lastSelectionScore;
    m_lastSelectionSource = QStringLiteral("none");
    m_lastSelectionScore = 0;

    if (m_overrideEnabled) {
        unbindActiveMenuSignals();
        invalidateMenuCache(oldService, oldPath);
        m_available = !m_overrideMenus.isEmpty();
        m_topLevelMenus = m_overrideMenus;
        m_activeMenuService = m_overrideContext;
        m_activeMenuPath.clear();
        m_activeWindowId = 0;
        if (oldAvailable != m_available ||
            oldMenus != m_topLevelMenus ||
            oldService != m_activeMenuService) {
            emit changed();
        }
        updatePollingInterval();
        return;
    }

    seedRegisteredMenus();

    const quint32 windowId = detectActiveWindowId();
    m_activeWindowId = windowId;
    if (windowId != 0) {
        QString service;
        QString path;
        if (resolveMenuForWindow(windowId, &service, &path) && !service.isEmpty() && !path.isEmpty()) {
            m_activeMenuService = service;
            m_activeMenuPath = path;
            m_topLevelMenus = queryTopLevelMenus(service, path);
            m_available = !m_topLevelMenus.isEmpty();
            m_lastSelectionSource = QStringLiteral("window-id");
            if (m_available) {
                RegisteredMenu item;
                item.service = service;
                item.path = path;
                item.seenMs = QDateTime::currentMSecsSinceEpoch();
                m_registeredMenus.insert(windowId, item);
            }
        } else {
            m_available = false;
            m_activeMenuService.clear();
            m_activeMenuPath.clear();
            m_topLevelMenus.clear();
        }
    } else {
        m_available = false;
        m_activeMenuService.clear();
        m_activeMenuPath.clear();
        m_topLevelMenus.clear();
    }

    if (!m_available) {
        QString service;
        QString path;
        int score = 0;
        if (selectMenuForActiveApp(&service, &path, &score) && !service.isEmpty() && !path.isEmpty()) {
            m_activeMenuService = service;
            m_activeMenuPath = path;
            m_topLevelMenus = queryTopLevelMenus(service, path);
            m_available = !m_topLevelMenus.isEmpty();
            m_lastSelectionSource = QStringLiteral("registrar-match");
            m_lastSelectionScore = score;
        }
    }

    if (oldService != m_activeMenuService || oldPath != m_activeMenuPath) {
        // Focus/app changed: invalidate stale submenu rows for both old and new active bindings.
        invalidateMenuCache(oldService, oldPath);
        invalidateMenuCache(m_activeMenuService, m_activeMenuPath);
    }

    if (m_available && m_activeMenuService != QString::fromLatin1(kSlmMenuService)) {
        bindActiveMenuSignals();
    } else {
        unbindActiveMenuSignals();
    }

    if (!m_available) {
        QDBusInterface slmMenu(QString::fromLatin1(kSlmMenuService),
                               QString::fromLatin1(kSlmMenuPath),
                               QString::fromLatin1(kSlmMenuIface),
                               QDBusConnection::sessionBus());
        if (slmMenu.isValid()) {
            QDBusReply<QVariantList> menusReply = slmMenu.call(QStringLiteral("GetTopLevelMenus"));
            if (menusReply.isValid()) {
                const QVariantList rows = menusReply.value();
                if (!rows.isEmpty()) {
                    m_activeMenuService = QString::fromLatin1(kSlmMenuService);
                    m_activeMenuPath = QString::fromLatin1(kSlmMenuPath);
                    m_topLevelMenus = rows;
                    m_available = true;
                    m_lastSelectionSource = QStringLiteral("slm-fallback");
                }
            }
        }
    }

    if (oldAvailable != m_available ||
        oldMenus != m_topLevelMenus ||
        oldService != m_activeMenuService) {
        emit changed();
    }
    if (m_debugEnabled) {
        const bool changedKey =
            (oldAvailable != m_available) ||
            (oldService != m_activeMenuService) ||
            (oldPath != m_activeMenuPath) ||
            (oldWindowId != m_activeWindowId) ||
            (oldSource != m_lastSelectionSource) ||
            (oldScore != m_lastSelectionScore);
        if (changedKey) {
            qInfo().noquote()
                << "[slm][globalmenu]"
                << "available=" << (m_available ? 1 : 0)
                << "source=" << m_lastSelectionSource
                << "score=" << m_lastSelectionScore
                << "service=" << m_activeMenuService
                << "path=" << m_activeMenuPath
                << "window=" << QString::number(m_activeWindowId)
                << "menus=" << QString::number(m_topLevelMenus.size())
                << "registered=" << QString::number(m_registeredMenus.size())
                << "activeAppToken=" << m_lastActiveAppId;
        }
    }
    updatePollingInterval();
}

void GlobalMenuManager::resetAfterResume()
{
    // Resume recovery path:
    // clear transient caches + bindings first, then rebind from active window.
    unbindActiveMenuSignals();
    m_connectionPidCache.clear();
    m_registeredMenus.clear();
    m_menuItemsCache.clear();
    m_pendingExternalMenuKeys.clear();
    for (auto it = m_externalMenuProxies.begin(); it != m_externalMenuProxies.end(); ++it) {
        if (!it.value().isNull()) {
            it.value()->deleteLater();
        }
    }
    m_externalMenuProxies.clear();

    m_available = false;
    m_topLevelMenus.clear();
    m_activeMenuService.clear();
    m_activeMenuPath.clear();
    m_activeWindowId = 0;
    emit changed();
    refresh();
}

void GlobalMenuManager::updatePollingInterval()
{
    if (!m_timer) {
        return;
    }
    const int nextInterval = (m_overrideEnabled || m_available || m_activeWindowId != 0)
        ? kPollFastMs
        : kPollIdleMs;
    if (m_timer->interval() != nextInterval) {
        m_timer->setInterval(nextInterval);
    }
}

bool GlobalMenuManager::activateMenu(int menuId)
{
    if (menuId <= 0) {
        return false;
    }

    if (m_overrideEnabled) {
        QString label;
        for (const QVariant &v : std::as_const(m_overrideMenus)) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("id")).toInt() == menuId) {
                label = row.value(QStringLiteral("label")).toString();
                break;
            }
        }
        emit overrideMenuActivated(menuId, label, m_overrideContext);
        return true;
    }

    if (m_activeMenuService.isEmpty() || m_activeMenuPath.isEmpty()) {
        return false;
    }

    if (m_activeMenuService == QString::fromLatin1(kSlmMenuService)) {
        QDBusInterface slmMenu(QString::fromLatin1(kSlmMenuService),
                               QString::fromLatin1(kSlmMenuPath),
                               QString::fromLatin1(kSlmMenuIface),
                               QDBusConnection::sessionBus());
        if (!slmMenu.isValid()) {
            return false;
        }
        QDBusReply<bool> reply = slmMenu.call(QStringLiteral("ActivateMenu"), menuId);
        if (!reply.isValid() || !reply.value()) {
            return false;
        }
        QString label;
        for (const QVariant &v : std::as_const(m_topLevelMenus)) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("id")).toInt() == menuId) {
                label = row.value(QStringLiteral("label")).toString();
                break;
            }
        }
        emit overrideMenuActivated(menuId, label, QStringLiteral("slm-dbus-menu"));
        return true;
    }

    QDBusInterface menuIface(m_activeMenuService,
                             m_activeMenuPath,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return false;
    }

    QDBusReply<void> reply = menuIface.call(QStringLiteral("Event"),
                                            menuId,
                                            QStringLiteral("clicked"),
                                            QVariant::fromValue(QDBusVariant(QVariant())),
                                            static_cast<uint>(0));
    return reply.isValid();
}

QVariantList GlobalMenuManager::menuItemsFor(int menuId) const
{
    if (menuId <= 0) {
        return {};
    }
    if (m_activeMenuService.isEmpty() || m_activeMenuPath.isEmpty()) {
        return {};
    }
    if (m_activeMenuService == QString::fromLatin1(kSlmMenuService)) {
        QDBusInterface slmMenu(QString::fromLatin1(kSlmMenuService),
                               QString::fromLatin1(kSlmMenuPath),
                               QString::fromLatin1(kSlmMenuIface),
                               QDBusConnection::sessionBus());
        if (!slmMenu.isValid()) {
            return {};
        }
        QDBusReply<QVariantList> reply = slmMenu.call(QStringLiteral("GetMenuItems"), menuId);
        if (!reply.isValid()) {
            return {};
        }
        return reply.value();
    }
    return queryMenuItemsNative(m_activeMenuService, m_activeMenuPath, menuId);
}

bool GlobalMenuManager::activateMenuItem(int menuId, int itemId)
{
    if (menuId <= 0 || itemId <= 0) {
        return false;
    }
    if (m_activeMenuService == QString::fromLatin1(kSlmMenuService)) {
        QDBusInterface slmMenu(QString::fromLatin1(kSlmMenuService),
                               QString::fromLatin1(kSlmMenuPath),
                               QString::fromLatin1(kSlmMenuIface),
                               QDBusConnection::sessionBus());
        if (!slmMenu.isValid()) {
            return false;
        }
        QDBusReply<bool> reply = slmMenu.call(QStringLiteral("ActivateMenuItem"), menuId, itemId);
        if (!reply.isValid() || !reply.value()) {
            return false;
        }
        QString label;
        const QVariantList rows = menuItemsFor(menuId);
        for (const QVariant &v : rows) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("separator")).toBool()) {
                continue;
            }
            if (row.value(QStringLiteral("id")).toInt() == itemId) {
                label = row.value(QStringLiteral("label")).toString();
                break;
            }
        }
        emit overrideMenuItemActivated(menuId, itemId, label, QStringLiteral("slm-dbus-menu"));
        return true;
    }

    QDBusInterface menuIface(m_activeMenuService,
                             m_activeMenuPath,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return false;
    }
    QDBusReply<void> reply = menuIface.call(QStringLiteral("Event"),
                                            itemId,
                                            QStringLiteral("clicked"),
                                            QVariant::fromValue(QDBusVariant(QVariant())),
                                            static_cast<uint>(0));
    return reply.isValid();
}

QVariantList GlobalMenuManager::queryDbusMenuTopLevel(const QString &service, const QString &path) const
{
    const QString svc = service.trimmed();
    const QString obj = path.trimmed();
    if (svc.isEmpty() || obj.isEmpty()) {
        return {};
    }
    ensureExternalMenuSignalBinding(svc, obj);
    return queryTopLevelMenus(svc, obj);
}

QVariantList GlobalMenuManager::queryDbusMenuItems(const QString &service,
                                                   const QString &path,
                                                   int menuId) const
{
    const QString svc = service.trimmed();
    const QString obj = path.trimmed();
    if (svc.isEmpty() || obj.isEmpty() || menuId <= 0) {
        return {};
    }
    ensureExternalMenuSignalBinding(svc, obj);
    return queryMenuItemsNative(svc, obj, menuId);
}

bool GlobalMenuManager::activateDbusMenuItem(const QString &service,
                                             const QString &path,
                                             int itemId)
{
    const QString svc = service.trimmed();
    const QString obj = path.trimmed();
    if (svc.isEmpty() || obj.isEmpty() || itemId <= 0) {
        return false;
    }

    QDBusInterface menuIface(svc,
                             obj,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return false;
    }

    QDBusReply<void> reply = menuIface.call(QStringLiteral("Event"),
                                            itemId,
                                            QStringLiteral("clicked"),
                                            QVariant::fromValue(QDBusVariant(QVariant())),
                                            static_cast<uint>(0));
    return reply.isValid();
}

QVariantMap GlobalMenuManager::diagnosticSnapshot() const
{
    QVariantMap out;
    out.insert(QStringLiteral("available"), m_available);
    out.insert(QStringLiteral("activeMenuService"), m_activeMenuService);
    out.insert(QStringLiteral("activeMenuPath"), m_activeMenuPath);
    out.insert(QStringLiteral("activeWindowId"), static_cast<uint>(m_activeWindowId));
    out.insert(QStringLiteral("topLevelMenuCount"), m_topLevelMenus.size());
    out.insert(QStringLiteral("overrideEnabled"), m_overrideEnabled);
    out.insert(QStringLiteral("overrideContext"), m_overrideContext);
    out.insert(QStringLiteral("lastActiveAppToken"), m_lastActiveAppId);
    out.insert(QStringLiteral("registeredMenuCount"), m_registeredMenus.size());
    out.insert(QStringLiteral("selectionSource"), m_lastSelectionSource);
    out.insert(QStringLiteral("selectionScore"), m_lastSelectionScore);
    out.insert(QStringLiteral("pollIntervalMs"), m_timer ? m_timer->interval() : 0);
    return out;
}

void GlobalMenuManager::setOverrideMenus(const QVariantList &menus, const QString &context)
{
    m_overrideEnabled = true;
    m_overrideMenus = menus;
    m_overrideContext = context.trimmed().isEmpty() ? QStringLiteral("override") : context.trimmed();
    refresh();
}

void GlobalMenuManager::clearOverrideMenus()
{
    if (!m_overrideEnabled) {
        return;
    }
    m_overrideEnabled = false;
    m_overrideMenus.clear();
    m_overrideContext.clear();
    refresh();
}

void GlobalMenuManager::onSessionActiveAppChanged(const QString &appId)
{
    const QString normalized = normalizeAppToken(appId);
    if (normalized == m_lastActiveAppId) {
        return;
    }
    m_lastActiveAppId = normalized;

    // Signal app-switching state for QML transition animations.
    if (!m_appSwitching) {
        m_appSwitching = true;
        emit appSwitchingChanged();
    }
    if (!m_appSwitchTimer) {
        m_appSwitchTimer = new QTimer(this);
        m_appSwitchTimer->setSingleShot(true);
        m_appSwitchTimer->setInterval(160);
        connect(m_appSwitchTimer, &QTimer::timeout, this, [this]() {
            if (m_appSwitching) {
                m_appSwitching = false;
                emit appSwitchingChanged();
            }
        });
    }
    m_appSwitchTimer->start();

    emit appSwitched(m_lastActiveAppId);
    refresh();
}

void GlobalMenuManager::bindActiveMenuSignals()
{
    if (m_activeMenuService.isEmpty() || m_activeMenuPath.isEmpty()) {
        unbindActiveMenuSignals();
        return;
    }
    if (m_boundMenuService == m_activeMenuService && m_boundMenuPath == m_activeMenuPath) {
        return;
    }

    unbindActiveMenuSignals();

    auto bindSignal = [&](const QString &signalName) {
        QDBusConnection::sessionBus().connect(m_activeMenuService,
                                              m_activeMenuPath,
                                              QString::fromLatin1(kMenuIface),
                                              signalName,
                                              this,
                                              SLOT(onActiveMenuLayoutChanged()));
    };
    bindSignal(QStringLiteral("LayoutUpdated"));
    bindSignal(QStringLiteral("ItemsPropertiesUpdated"));
    bindSignal(QStringLiteral("ItemPropertyUpdated"));
    bindSignal(QStringLiteral("ItemActivationRequested"));

    m_boundMenuService = m_activeMenuService;
    m_boundMenuPath = m_activeMenuPath;
}

void GlobalMenuManager::unbindActiveMenuSignals()
{
    if (m_boundMenuService.isEmpty() || m_boundMenuPath.isEmpty()) {
        m_boundMenuService.clear();
        m_boundMenuPath.clear();
        return;
    }
    auto unbindSignal = [&](const QString &signalName) {
        QDBusConnection::sessionBus().disconnect(m_boundMenuService,
                                                 m_boundMenuPath,
                                                 QString::fromLatin1(kMenuIface),
                                                 signalName,
                                                 this,
                                                 SLOT(onActiveMenuLayoutChanged()));
    };
    unbindSignal(QStringLiteral("LayoutUpdated"));
    unbindSignal(QStringLiteral("ItemsPropertiesUpdated"));
    unbindSignal(QStringLiteral("ItemPropertyUpdated"));
    unbindSignal(QStringLiteral("ItemActivationRequested"));

    m_boundMenuService.clear();
    m_boundMenuPath.clear();
}

void GlobalMenuManager::onActiveMenuLayoutChanged()
{
    if (m_overrideEnabled) {
        return;
    }
    invalidateMenuCache(m_activeMenuService, m_activeMenuPath);
    // DBusMenu content changed; refresh top-level menus immediately.
    refresh();
}

void GlobalMenuManager::seedRegisteredMenus()
{
    QHash<quint32, RegisteredMenu> next;
    seedRegisteredMenusFromRegistrar(QString::fromLatin1(kRegistrarService),
                                     QString::fromLatin1(kRegistrarPath),
                                     QString::fromLatin1(kRegistrarIface),
                                     QStringLiteral("GetMenus"),
                                     &next);
    seedRegisteredMenusFromRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                     QString::fromLatin1(kKdeRegistrarPath),
                                     QString::fromLatin1(kKdeRegistrarIface),
                                     QStringLiteral("getMenus"),
                                     &next);
    seedRegisteredMenusFromRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                     QString::fromLatin1(kKdeRegistrarPath),
                                     QString::fromLatin1(kKdeRegistrarIface),
                                     QStringLiteral("GetMenus"),
                                     &next);

    if (next.isEmpty()) {
        return;
    }
    m_registeredMenus = next;
}

void GlobalMenuManager::seedRegisteredMenusFromRegistrar(const QString &registrarService,
                                                         const QString &registrarPath,
                                                         const QString &registrarIface,
                                                         const QString &method,
                                                         QHash<quint32, RegisteredMenu> *out) const
{
    if (!out) {
        return;
    }
    QDBusInterface registrar(registrarService,
                             registrarPath,
                             registrarIface,
                             QDBusConnection::sessionBus());
    if (!registrar.isValid()) {
        return;
    }

    QDBusMessage reply = registrar.call(method);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return;
    }

    const QVariant first = reply.arguments().constFirst();
    if (!first.canConvert<QDBusArgument>()) {
        return;
    }
    const QDBusArgument dbusArg = first.value<QDBusArgument>();
    dbusArg.beginArray();
    while (!dbusArg.atEnd()) {
        RegistrarMenuNode row;
        dbusArg >> row;
        if (row.windowId == 0) {
            continue;
        }
        const QString service = row.service.trimmed();
        const QString path = row.path.path().trimmed();
        if (service.isEmpty() || path.isEmpty()) {
            continue;
        }
        RegisteredMenu item;
        item.service = service;
        item.path = path;
        item.seenMs = QDateTime::currentMSecsSinceEpoch();
        out->insert(row.windowId, item);
    }
    dbusArg.endArray();
}

qint64 GlobalMenuManager::connectionPid(const QString &service) const
{
    const QString key = service.trimmed();
    if (key.isEmpty()) {
        return 0;
    }
    const auto it = m_connectionPidCache.constFind(key);
    if (it != m_connectionPidCache.cend()) {
        return *it;
    }

    QDBusInterface dbusIface(QStringLiteral("org.freedesktop.DBus"),
                             QStringLiteral("/org/freedesktop/DBus"),
                             QStringLiteral("org.freedesktop.DBus"),
                             QDBusConnection::sessionBus());
    qint64 pid = 0;
    if (dbusIface.isValid()) {
        QDBusReply<uint> reply = dbusIface.call(QStringLiteral("GetConnectionUnixProcessID"), key);
        if (reply.isValid()) {
            pid = static_cast<qint64>(reply.value());
        }
    }
    m_connectionPidCache.insert(key, pid);
    return pid;
}

QString GlobalMenuManager::normalizeAppToken(const QString &raw) const
{
    QString out = raw.trimmed().toLower();
    if (out.endsWith(QStringLiteral(".desktop"))) {
        out.chop(8);
    }
    out = QFileInfo(out).fileName();
    out.remove(QRegularExpression(QStringLiteral("[^a-z0-9._-]")));
    return out;
}

bool GlobalMenuManager::selectMenuForActiveApp(QString *service, QString *path, int *score) const
{
    if (!service || !path || m_registeredMenus.isEmpty()) {
        return false;
    }

    auto scoreFor = [&](const RegisteredMenu &item) -> int {
        int score = 0;
        const QString active = m_lastActiveAppId;
        if (!active.isEmpty()) {
            const QString svc = item.service.toLower();
            if (svc.contains(active)) {
                score += 120;
            }
            const QStringList activeTokens =
                active.split(QRegularExpression(QStringLiteral("[._-]+")), Qt::SkipEmptyParts);
            for (const QString &token : activeTokens) {
                if (token.size() < 3) {
                    continue;
                }
                if (svc.contains(token)) {
                    score += 60;
                }
            }

            const qint64 pid = connectionPid(item.service);
            if (pid > 0) {
                QFile commFile(QStringLiteral("/proc/%1/comm").arg(pid));
                if (commFile.open(QIODevice::ReadOnly)) {
                    const QString comm = QString::fromUtf8(commFile.readAll()).trimmed().toLower();
                    if (!comm.isEmpty()) {
                        if (comm == active || comm.contains(active)) {
                            score += 120;
                        } else {
                            for (const QString &token : activeTokens) {
                                if (token.size() >= 3 && comm.contains(token)) {
                                    score += 50;
                                }
                            }
                        }
                    }
                }
                QFile cmdFile(QStringLiteral("/proc/%1/cmdline").arg(pid));
                if (cmdFile.open(QIODevice::ReadOnly)) {
                    QString cmd = QString::fromUtf8(cmdFile.readAll()).toLower();
                    cmd.replace(QLatin1Char('\0'), QLatin1Char(' '));
                    if (cmd.contains(active)) {
                        score += 100;
                    } else {
                        for (const QString &token : activeTokens) {
                            if (token.size() >= 3 && cmd.contains(token)) {
                                score += 40;
                            }
                        }
                    }
                }
            }
        }

        if (item.seenMs > 0) {
            const qint64 ageMs = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - item.seenMs);
            score += qMax(0, 40 - static_cast<int>(ageMs / 250));
        }
        return score;
    };

    int bestScore = std::numeric_limits<int>::min();
    RegisteredMenu best;
    for (auto it = m_registeredMenus.cbegin(); it != m_registeredMenus.cend(); ++it) {
        const int score = scoreFor(it.value());
        if (score > bestScore) {
            bestScore = score;
            best = it.value();
        }
    }
    if (best.service.isEmpty() || best.path.isEmpty()) {
        return false;
    }

    *service = best.service;
    *path = best.path;
    if (score) {
        *score = bestScore;
    }
    return true;
}

quint32 GlobalMenuManager::detectActiveWindowId() const
{
    const QString xpropPath = QStandardPaths::findExecutable(QStringLiteral("xprop"));
    if (xpropPath.isEmpty()) {
        return 0;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(xpropPath, QStringList{QStringLiteral("-root"), QStringLiteral("_NET_ACTIVE_WINDOW")});
    if (!proc.waitForFinished(220) || proc.exitCode() != 0) {
        return 0;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QRegularExpression hexRe(QStringLiteral("0x([0-9a-fA-F]+)"));
    const QRegularExpressionMatch match = hexRe.match(output);
    if (!match.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const quint32 id = match.captured(1).toUInt(&ok, 16);
    return ok ? id : 0;
}

bool GlobalMenuManager::resolveMenuForWindow(quint32 windowId, QString *service, QString *path) const
{
    if (resolveMenuForWindowViaRegistrar(QString::fromLatin1(kRegistrarService),
                                         QString::fromLatin1(kRegistrarPath),
                                         QString::fromLatin1(kRegistrarIface),
                                         QStringLiteral("GetMenuForWindow"),
                                         windowId, service, path)) {
        return true;
    }
    if (resolveMenuForWindowViaRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                         QString::fromLatin1(kKdeRegistrarPath),
                                         QString::fromLatin1(kKdeRegistrarIface),
                                         QStringLiteral("getMenuForWindow"),
                                         windowId, service, path)) {
        return true;
    }
    if (resolveMenuForWindowViaRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                         QString::fromLatin1(kKdeRegistrarPath),
                                         QString::fromLatin1(kKdeRegistrarIface),
                                         QStringLiteral("GetMenuForWindow"),
                                         windowId, service, path)) {
        return true;
    }
    return false;
}

bool GlobalMenuManager::resolveMenuForWindowViaRegistrar(const QString &registrarService,
                                                         const QString &registrarPath,
                                                         const QString &registrarIface,
                                                         const QString &method,
                                                         quint32 windowId,
                                                         QString *service,
                                                         QString *path) const
{
    if (!service || !path) {
        return false;
    }

    QDBusInterface registrar(registrarService,
                             registrarPath,
                             registrarIface,
                             QDBusConnection::sessionBus());
    if (!registrar.isValid()) {
        return false;
    }

    QDBusMessage reply = registrar.call(method,
                                        QVariant::fromValue(static_cast<uint>(windowId)));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return false;
    }
    const QList<QVariant> args = reply.arguments();
    if (args.size() < 2) {
        return false;
    }

    *service = args.at(0).toString().trimmed();

    const QVariant pathVariant = args.at(1);
    if (pathVariant.canConvert<QDBusObjectPath>()) {
        *path = pathVariant.value<QDBusObjectPath>().path().trimmed();
    } else {
        *path = pathVariant.toString().trimmed();
    }
    return !service->isEmpty() && !path->isEmpty();
}

QVariantList GlobalMenuManager::queryTopLevelMenus(const QString &service, const QString &path) const
{
    QVariantList out;
    QDBusInterface menuIface(service,
                             path,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return out;
    }

    QDBusMessage reply = menuIface.call(QStringLiteral("GetLayout"),
                                        0,
                                        1,
                                        QStringList{QStringLiteral("label"),
                                                    QStringLiteral("visible"),
                                                    QStringLiteral("enabled")});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return out;
    }
    const QList<QVariant> args = reply.arguments();
    if (args.size() < 2 || !args.at(1).canConvert<QDBusArgument>()) {
        return out;
    }

    DbusMenuNode root;
    const QDBusArgument dbusArg = args.at(1).value<QDBusArgument>();
    dbusArg >> root;

    for (const DbusMenuNode &child : std::as_const(root.children)) {
        const QString label = normalizeLabel(child.properties.value(QStringLiteral("label")).toString());
        if (label.isEmpty()) {
            continue;
        }
        const bool visible = child.properties.value(QStringLiteral("visible"), true).toBool();
        if (!visible) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("id"), child.id);
        item.insert(QStringLiteral("label"), label);
        item.insert(QStringLiteral("enabled"),
                    child.properties.value(QStringLiteral("enabled"), true).toBool());
        out << item;
    }
    return out;
}

QVariantList GlobalMenuManager::queryMenuItemsNative(const QString &service,
                                                     const QString &path,
                                                     int menuId) const
{
    QVariantList out;
    if (menuId <= 0) {
        return out;
    }
    const QString cacheKey = menuCacheKey(service, path);
    if (!cacheKey.isEmpty()) {
        const auto cacheIt = m_menuItemsCache.constFind(cacheKey);
        if (cacheIt != m_menuItemsCache.cend()) {
            const auto menuIt = cacheIt->constFind(menuId);
            if (menuIt != cacheIt->cend()) {
                return *menuIt;
            }
        }
    }

    QDBusInterface menuIface(service,
                             path,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return out;
    }

    QDBusMessage reply = menuIface.call(QStringLiteral("GetLayout"),
                                        menuId,
                                        1,
                                        QStringList{QStringLiteral("label"),
                                                    QStringLiteral("visible"),
                                                    QStringLiteral("enabled"),
                                                    QStringLiteral("type"),
                                                    QStringLiteral("children-display"),
                                                    QStringLiteral("toggle-type"),
                                                    QStringLiteral("toggle-state"),
                                                    QStringLiteral("icon-name"),
                                                    QStringLiteral("icon-data"),
                                                    QStringLiteral("shortcut")});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return out;
    }
    const QList<QVariant> args = reply.arguments();
    if (args.size() < 2 || !args.at(1).canConvert<QDBusArgument>()) {
        return out;
    }

    DbusMenuNode root;
    const QDBusArgument dbusArg = args.at(1).value<QDBusArgument>();
    dbusArg >> root;
    for (const DbusMenuNode &child : std::as_const(root.children)) {
        const QString type = child.properties.value(QStringLiteral("type")).toString().trimmed().toLower();
        const bool separator = (type == QStringLiteral("separator"));
        const bool visible = child.properties.value(QStringLiteral("visible"), true).toBool();
        if (!visible) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), child.id);
        row.insert(QStringLiteral("separator"), separator);
        if (!separator) {
            const QString toggleType =
                child.properties.value(QStringLiteral("toggle-type")).toString().trimmed().toLower();
            const int toggleState =
                child.properties.value(QStringLiteral("toggle-state"), 0).toInt();
            const QString iconName =
                child.properties.value(QStringLiteral("icon-name")).toString().trimmed();
            const QString iconSource =
                parseDbusIconDataUri(child.properties.value(QStringLiteral("icon-data")));
            const QString shortcutText =
                parseDbusShortcutText(child.properties.value(QStringLiteral("shortcut")));
            const QString childrenDisplay =
                child.properties.value(QStringLiteral("children-display")).toString().trimmed().toLower();

            row.insert(QStringLiteral("label"),
                       normalizeLabel(child.properties.value(QStringLiteral("label")).toString()));
            row.insert(QStringLiteral("enabled"),
                       child.properties.value(QStringLiteral("enabled"), true).toBool());
            row.insert(QStringLiteral("checkable"), !toggleType.isEmpty());
            row.insert(QStringLiteral("checked"), toggleState > 0);
            row.insert(QStringLiteral("iconName"), iconName);
            row.insert(QStringLiteral("iconSource"), iconSource);
            row.insert(QStringLiteral("shortcutText"), shortcutText);
            row.insert(QStringLiteral("hasSubmenu"), childrenDisplay == QStringLiteral("submenu"));
        }
        out << row;
    }
    if (!cacheKey.isEmpty()) {
        m_menuItemsCache[cacheKey].insert(menuId, out);
    }
    return out;
}

QString GlobalMenuManager::normalizeLabel(const QString &raw) const
{
    QString label = raw;
    label.replace(QStringLiteral("_"), QString());
    label = label.replace(QStringLiteral("&"), QString());
    return label.trimmed();
}

QString GlobalMenuManager::parseDbusIconDataUri(const QVariant &iconDataVariant)
{
    QByteArray bytes;
    if (iconDataVariant.canConvert<QByteArray>()) {
        bytes = iconDataVariant.toByteArray();
        if (bytes.size() > kMaxDbusMenuIconBytes) {
            return {};
        }
    } else {
        const QString maybeText = iconDataVariant.toString();
        if (!maybeText.isEmpty()) {
            if (maybeText.size() > kMaxDbusMenuIconBase64Chars) {
                return {};
            }
            bytes = QByteArray::fromBase64(maybeText.toUtf8());
            if (bytes.size() > kMaxDbusMenuIconBytes) {
                return {};
            }
        }
    }
    if (bytes.isEmpty()) {
        return {};
    }

    QString mimeType = QStringLiteral("image/png");
    QBuffer probe(&bytes);
    probe.open(QIODevice::ReadOnly);
    const QString fmt = QString::fromLatin1(QImageReader::imageFormat(&probe)).toLower();
    if (!fmt.isEmpty()) {
        if (fmt == QStringLiteral("svg") || fmt == QStringLiteral("svgz")) {
            mimeType = QStringLiteral("image/svg+xml");
        } else {
            mimeType = QStringLiteral("image/") + fmt;
        }
    }
    return QStringLiteral("data:%1;base64,%2")
        .arg(mimeType, QString::fromLatin1(bytes.toBase64()));
}

QString GlobalMenuManager::menuCacheKey(const QString &service, const QString &path) const
{
    const QString svc = service.trimmed();
    const QString obj = path.trimmed();
    if (svc.isEmpty() || obj.isEmpty()) {
        return {};
    }
    return svc + QLatin1Char('|') + obj;
}

void GlobalMenuManager::invalidateMenuCache(const QString &service, const QString &path)
{
    const QString key = menuCacheKey(service, path);
    if (key.isEmpty()) {
        return;
    }
    m_menuItemsCache.remove(key);
}

void GlobalMenuManager::ensureExternalMenuSignalBinding(const QString &service, const QString &path) const
{
    const QString svc = service.trimmed();
    const QString obj = path.trimmed();
    if (svc.isEmpty() || obj.isEmpty()) {
        return;
    }
    const QString key = menuCacheKey(svc, obj);
    if (m_externalMenuProxies.contains(key) && !m_externalMenuProxies.value(key).isNull()) {
        return;
    }

    auto *self = const_cast<GlobalMenuManager *>(this);
    auto *proxy = new ExternalMenuSignalProxy(self, svc, obj, self);
    auto bus = QDBusConnection::sessionBus();
    const bool l = bus.connect(svc,
                               obj,
                               QString::fromLatin1(kMenuIface),
                               QStringLiteral("LayoutUpdated"),
                               proxy,
                               SLOT(onLayoutUpdated(int,int)));
    const bool p = bus.connect(svc,
                               obj,
                               QString::fromLatin1(kMenuIface),
                               QStringLiteral("ItemsPropertiesUpdated"),
                               proxy,
                               SLOT(onItemsPropertiesUpdated(QDBusArgument,QDBusArgument)));
    const bool a = bus.connect(svc,
                               obj,
                               QString::fromLatin1(kMenuIface),
                               QStringLiteral("ItemActivationRequested"),
                               proxy,
                               SLOT(onItemActivationRequested(int,uint)));
    if (!(l || p || a)) {
        proxy->deleteLater();
        return;
    }
    m_externalMenuProxies.insert(key, proxy);
}

void GlobalMenuManager::notifyExternalMenuChanged(const QString &service, const QString &path)
{
    const QString key = menuCacheKey(service, path);
    if (key.isEmpty()) {
        return;
    }
    m_pendingExternalMenuKeys.insert(key);
    if (m_externalMenuDebounceTimer && !m_externalMenuDebounceTimer->isActive()) {
        m_externalMenuDebounceTimer->start();
    }
}

void GlobalMenuManager::flushExternalMenuChanges()
{
    if (m_pendingExternalMenuKeys.isEmpty()) {
        return;
    }

    const auto pending = std::exchange(m_pendingExternalMenuKeys, QSet<QString>{});
    for (const QString &key : pending) {
        const int splitAt = key.indexOf(QLatin1Char('|'));
        if (splitAt <= 0 || splitAt >= (key.size() - 1)) {
            continue;
        }
        const QString service = key.left(splitAt);
        const QString path = key.mid(splitAt + 1);
        invalidateMenuCache(service, path);
        emit dbusMenuChanged(service, path);
    }
}

#include "globalmenumanager.moc"
