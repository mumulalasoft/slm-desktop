#include "globalmenuservice.h"

#include "../../../dbuslogutils.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <utility>

namespace {
constexpr const char kService[] = "org.slm.Desktop.GlobalMenu";
constexpr const char kPath[] = "/org/slm/Desktop/GlobalMenu";
constexpr int kDefaultBaseId = 2000;

constexpr int kFileId = kDefaultBaseId + 1;
constexpr int kEditId = kDefaultBaseId + 2;
constexpr int kViewId = kDefaultBaseId + 3;
constexpr int kToolsId = kDefaultBaseId + 4;
constexpr int kWorkspaceId = kDefaultBaseId + 5;
constexpr int kHelpId = kDefaultBaseId + 6;

constexpr int kGoId = 2101;
constexpr int kDevicesId = 2102;
constexpr int kTabsId = 2201;
constexpr int kHistoryId = 2202;
constexpr int kProfilesId = 2203;
constexpr int kProjectId = 2301;
constexpr int kRunId = 2302;
constexpr int kDebugId = 2303;
constexpr int kGitId = 2304;
}

GlobalMenuService::GlobalMenuService(QObject *parent)
    : QObject(parent)
    , m_auditLogger(&m_permissionStore)
{
    m_menus = defaultMenus();
    setupSecurity();
    registerDbusService();
}

GlobalMenuService::~GlobalMenuService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool GlobalMenuService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString GlobalMenuService::context() const
{
    return m_context;
}

QVariantMap GlobalMenuService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("context"), m_context},
        {QStringLiteral("menu_count"), m_menus.size()},
    };
}

QVariantList GlobalMenuService::GetTopLevelMenus() const
{
    return effectiveMenus();
}

QVariantList GlobalMenuService::GetMenuItems(int menuId) const
{
    return defaultMenuItemsFor(menuId);
}

bool GlobalMenuService::ActivateMenu(int menuId)
{
    if (!checkPermission(Slm::Permissions::Capability::GlobalMenuInvoke, QStringLiteral("ActivateMenu"))) {
        return false;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (menuId <= 0) {
        SlmDbusLog::logEvent(QString::fromLatin1(kService),
                             QStringLiteral("ActivateMenu"),
                             requestId,
                             caller,
                             QString(),
                             QStringLiteral("reject"),
                             {{QStringLiteral("ok"), false},
                              {QStringLiteral("reason"), QStringLiteral("invalid_menu_id")},
                              {QStringLiteral("menu_id"), menuId}});
        return false;
    }

    const QString label = labelForId(menuId);
    if (label.isEmpty()) {
        SlmDbusLog::logEvent(QString::fromLatin1(kService),
                             QStringLiteral("ActivateMenu"),
                             requestId,
                             caller,
                             QString(),
                             QStringLiteral("reject"),
                             {{QStringLiteral("ok"), false},
                              {QStringLiteral("reason"), QStringLiteral("menu_not_found")},
                              {QStringLiteral("menu_id"), menuId}});
        return false;
    }

    emit MenuActivated(menuId, label, m_context);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ActivateMenu"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), true},
                          {QStringLiteral("menu_id"), menuId},
                          {QStringLiteral("label"), label},
                          {QStringLiteral("context"), m_context}});
    return true;
}

bool GlobalMenuService::ActivateMenuItem(int menuId, int itemId)
{
    if (!checkPermission(Slm::Permissions::Capability::GlobalMenuInvoke, QStringLiteral("ActivateMenuItem"))) {
        return false;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (menuId <= 0 || itemId <= 0) {
        SlmDbusLog::logEvent(QString::fromLatin1(kService),
                             QStringLiteral("ActivateMenuItem"),
                             requestId,
                             caller,
                             QString(),
                             QStringLiteral("reject"),
                             {{QStringLiteral("ok"), false},
                              {QStringLiteral("reason"), QStringLiteral("invalid_id")},
                              {QStringLiteral("menu_id"), menuId},
                              {QStringLiteral("item_id"), itemId}});
        return false;
    }
    const QString label = labelForMenuItem(menuId, itemId);
    if (label.isEmpty()) {
        return false;
    }
    emit MenuItemActivated(menuId, itemId, label, m_context);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ActivateMenuItem"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), true},
                          {QStringLiteral("menu_id"), menuId},
                          {QStringLiteral("item_id"), itemId},
                          {QStringLiteral("label"), label},
                          {QStringLiteral("context"), m_context}});
    return true;
}

bool GlobalMenuService::SetTopLevelMenus(const QVariantList &menus, const QString &contextValue)
{
    if (!checkPermission(Slm::Permissions::Capability::GlobalMenuConfigure, QStringLiteral("SetTopLevelMenus"))) {
        return false;
    }
    QVariantList out;
    out.reserve(menus.size());
    int fallbackId = kDefaultBaseId + 1;
    for (const QVariant &v : menus) {
        const QVariantMap in = v.toMap();
        const QString label = normalizeLabel(in.value(QStringLiteral("label")).toString());
        if (label.isEmpty()) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), normalizeMenuId(in, fallbackId++));
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("enabled"), in.value(QStringLiteral("enabled"), true).toBool());
        out << row;
    }

    if (out.isEmpty()) {
        return false;
    }

    m_menus = out;
    m_context = contextValue.trimmed().isEmpty() ? QStringLiteral("external")
                                                 : contextValue.trimmed();
    emit menusChanged();
    return true;
}

bool GlobalMenuService::ResetToDefault()
{
    m_menus = defaultMenus();
    m_context = QStringLiteral("slm-default");
    emit menusChanged();
    return true;
}

QVariantMap GlobalMenuService::DiagnosticSnapshot() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service_registered"), m_serviceRegistered},
        {QStringLiteral("context"), m_context},
        {QStringLiteral("menus"), m_menus},
    };
}

QVariantList GlobalMenuService::defaultMenus()
{
    return {
        QVariantMap{
            {QStringLiteral("id"), kFileId},
            {QStringLiteral("label"), QStringLiteral("File")},
            {QStringLiteral("enabled"), true},
        },
        QVariantMap{
            {QStringLiteral("id"), kEditId},
            {QStringLiteral("label"), QStringLiteral("Edit")},
            {QStringLiteral("enabled"), true},
        },
        QVariantMap{
            {QStringLiteral("id"), kViewId},
            {QStringLiteral("label"), QStringLiteral("View")},
            {QStringLiteral("enabled"), true},
        },
        QVariantMap{
            {QStringLiteral("id"), kToolsId},
            {QStringLiteral("label"), QStringLiteral("Tools")},
            {QStringLiteral("enabled"), true},
        },
        QVariantMap{
            {QStringLiteral("id"), kWorkspaceId},
            {QStringLiteral("label"), QStringLiteral("Workspace")},
            {QStringLiteral("enabled"), true},
        },
        QVariantMap{
            {QStringLiteral("id"), kHelpId},
            {QStringLiteral("label"), QStringLiteral("Help")},
            {QStringLiteral("enabled"), true},
        },
    };
}

QString GlobalMenuService::normalizeLabel(const QString &raw)
{
    QString label = raw;
    label.replace(QStringLiteral("_"), QString());
    label.replace(QStringLiteral("&"), QString());
    return label.trimmed();
}

int GlobalMenuService::normalizeMenuId(const QVariantMap &row, int fallbackId) const
{
    bool ok = false;
    const int parsed = row.value(QStringLiteral("id")).toInt(&ok);
    if (ok && parsed > 0) {
        return parsed;
    }
    return fallbackId > 0 ? fallbackId : (kDefaultBaseId + 1);
}

QString GlobalMenuService::labelForId(int menuId) const
{
    const QVariantList menus = effectiveMenus();
    for (const QVariant &v : menus) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("id")).toInt() == menuId) {
            return row.value(QStringLiteral("label")).toString();
        }
    }
    return QString();
}

QVariantList GlobalMenuService::defaultMenuItemsFor(int menuId) const
{
    auto mk = [](int id,
                 const QString &label,
                 bool enabled = true,
                 const QString &shortcutText = QString()) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("shortcutText"), shortcutText},
            {QStringLiteral("separator"), false},
        };
    };
    auto sep = []() {
        return QVariantMap{
            {QStringLiteral("id"), -1},
            {QStringLiteral("label"), QString()},
            {QStringLiteral("enabled"), false},
            {QStringLiteral("separator"), true},
        };
    };

    switch (menuId) {
    case kFileId:
        return {mk(1, QStringLiteral("New Window")),
                mk(2, QStringLiteral("New Tab")),
                sep(),
                mk(3, QStringLiteral("Open...")),
                mk(6, QStringLiteral("Print..."), true, QStringLiteral("Ctrl+P")),
                mk(4, QStringLiteral("Close Window")),
                sep(),
                mk(5, QStringLiteral("Quit SLM Desktop"))};
    case kEditId:
        return {mk(1, QStringLiteral("Undo")),
                mk(2, QStringLiteral("Redo")),
                sep(),
                mk(3, QStringLiteral("Cut")),
                mk(4, QStringLiteral("Copy")),
                mk(5, QStringLiteral("Paste")),
                mk(6, QStringLiteral("Select All"))};
    case kViewId:
        return {mk(1, QStringLiteral("as Icons")),
                mk(2, QStringLiteral("as List")),
                mk(3, QStringLiteral("as Columns")),
                sep(),
                mk(4, QStringLiteral("Show Sidebar")),
                mk(5, QStringLiteral("Show Status Bar"))};
    case kToolsId:
        return {mk(1, QStringLiteral("Extensions Manager")),
                mk(2, QStringLiteral("Automation Scripts")),
                mk(3, QStringLiteral("Open Terminal Here")),
                sep(),
                mk(4, QStringLiteral("Permissions"))};
    case kWorkspaceId:
        return {mk(1, QStringLiteral("Move to Workspace 1")),
                mk(2, QStringLiteral("Move to Workspace 2")),
                mk(3, QStringLiteral("Split View Left")),
                mk(4, QStringLiteral("Split View Right")),
                sep(),
                mk(5, QStringLiteral("Pin to All Workspaces"))};
    case kGoId:
        return {mk(1, QStringLiteral("Back")),
                mk(2, QStringLiteral("Forward")),
                mk(3, QStringLiteral("Home")),
                mk(4, QStringLiteral("Documents")),
                mk(5, QStringLiteral("Downloads")),
                mk(6, QStringLiteral("Pictures"))};
    case kDevicesId:
        return {mk(1, QStringLiteral("Mount Storage")),
                mk(2, QStringLiteral("Unmount Storage")),
                sep(),
                mk(3, QStringLiteral("Send to Device"))};
    case kTabsId:
        return {mk(1, QStringLiteral("New Tab")),
                mk(2, QStringLiteral("Close Tab")),
                mk(3, QStringLiteral("Reopen Closed Tab"))};
    case kHistoryId:
        return {mk(1, QStringLiteral("Back")),
                mk(2, QStringLiteral("Forward")),
                mk(3, QStringLiteral("Show Full History"))};
    case kProfilesId:
        return {mk(1, QStringLiteral("Switch Profile")),
                mk(2, QStringLiteral("Manage Profiles"))};
    case kProjectId:
        return {mk(1, QStringLiteral("Open Project")),
                mk(2, QStringLiteral("Recent Projects"))};
    case kRunId:
        return {mk(1, QStringLiteral("Run")),
                mk(2, QStringLiteral("Run Configuration"))};
    case kDebugId:
        return {mk(1, QStringLiteral("Start Debugging")),
                mk(2, QStringLiteral("Attach to Process"))};
    case kGitId:
        return {mk(1, QStringLiteral("Commit")),
                mk(2, QStringLiteral("Push")),
                mk(3, QStringLiteral("Pull"))};
    case kHelpId:
        return {mk(1, QStringLiteral("SLM Help Center")),
                mk(2, QStringLiteral("Keyboard Shortcuts")),
                mk(3, QStringLiteral("Report an Issue"))};
    default:
        return {};
    }
}

QString GlobalMenuService::labelForMenuItem(int menuId, int itemId) const
{
    const QVariantList items = defaultMenuItemsFor(menuId);
    for (const QVariant &v : items) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("separator")).toBool()) {
            continue;
        }
        if (row.value(QStringLiteral("id")).toInt() == itemId) {
            return row.value(QStringLiteral("label")).toString();
        }
    }
    return QString();
}

bool GlobalMenuService::contextHasToken(const QString &token) const
{
    const QString ctx = m_context.trimmed().toLower();
    const QString tk = token.trimmed().toLower();
    return !ctx.isEmpty() && !tk.isEmpty() && ctx.contains(tk);
}

QVariantList GlobalMenuService::contextualMenusForContext() const
{
    auto menu = [](int id, const QString &label) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("enabled"), true},
        };
    };

    QVariantList out;
    if (contextHasToken(QStringLiteral("browser"))) {
        out << menu(kTabsId, QStringLiteral("Tabs"))
            << menu(kHistoryId, QStringLiteral("History"))
            << menu(kProfilesId, QStringLiteral("Profiles"));
    }
    if (contextHasToken(QStringLiteral("ide")) || contextHasToken(QStringLiteral("code"))) {
        out << menu(kProjectId, QStringLiteral("Project"))
            << menu(kRunId, QStringLiteral("Run"))
            << menu(kDebugId, QStringLiteral("Debug"))
            << menu(kGitId, QStringLiteral("Git"));
    }
    if (contextHasToken(QStringLiteral("filemanager")) || contextHasToken(QStringLiteral("files"))) {
        out << menu(kGoId, QStringLiteral("Go"))
            << menu(kDevicesId, QStringLiteral("Devices"));
    }
    return out;
}

QVariantList GlobalMenuService::effectiveMenus() const
{
    QVariantList out = m_menus;
    const QVariantList contextual = contextualMenusForContext();
    for (const QVariant &v : contextual) {
        const QVariantMap row = v.toMap();
        const int id = row.value(QStringLiteral("id")).toInt();
        bool exists = false;
        for (const QVariant &existing : std::as_const(out)) {
            if (existing.toMap().value(QStringLiteral("id")).toInt() == id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            out << row;
        }
    }
    return out;
}

void GlobalMenuService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const QDBusReply<bool> isReg = iface->isServiceRegistered(QString::fromLatin1(kService));
    if (isReg.isValid() && isReg.value()) {
        iface->unregisterService(QString::fromLatin1(kService));
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerObject(QString::fromLatin1(kPath),
                            this,
                            QDBusConnection::ExportAllSlots |
                                QDBusConnection::ExportAllSignals |
                                QDBusConnection::ExportAllProperties)) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void GlobalMenuService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for GlobalMenuService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.GlobalMenu"), QStringLiteral("ActivateMenu"), Slm::Permissions::Capability::GlobalMenuInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.GlobalMenu"), QStringLiteral("ActivateMenuItem"), Slm::Permissions::Capability::GlobalMenuInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.GlobalMenu"), QStringLiteral("SetTopLevelMenus"), Slm::Permissions::Capability::GlobalMenuConfigure);
}

bool GlobalMenuService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Medium"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (d.type == Slm::Permissions::DecisionType::Allow) {
        return true;
    }

    sendErrorReply(QStringLiteral("org.slm.Desktop.Error.PermissionDenied"),
                   QStringLiteral("Permission denied for method %1: %2")
                       .arg(methodName)
                       .arg(d.reason));
    return false;
}
