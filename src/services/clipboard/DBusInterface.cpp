#include "DBusInterface.h"

#include "ClipboardService.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace Slm::Clipboard {
namespace {
constexpr const char kService[] = "org.desktop.Clipboard";
constexpr const char kPath[] = "/org/desktop/Clipboard";
}

DBusInterface::DBusInterface(ClipboardService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    if (m_service) {
        connect(m_service, SIGNAL(ClipboardChanged(QVariantMap)),
                this, SIGNAL(ClipboardChanged(QVariantMap)));
        connect(m_service, SIGNAL(HistoryUpdated()),
                this, SIGNAL(HistoryUpdated()));
    }
}

DBusInterface::~DBusInterface()
{
    if (!m_registered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

void DBusInterface::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[slm-clipboardd] failed to open permission store";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("GetHistory"), Slm::Permissions::Capability::ClipboardReadHistoryPreview);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("Search"), Slm::Permissions::Capability::ClipboardReadHistoryPreview);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("PasteItem"), Slm::Permissions::Capability::ClipboardReadHistoryContent);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("DeleteItem"), Slm::Permissions::Capability::ClipboardDeleteHistory);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("PinItem"), Slm::Permissions::Capability::ClipboardPinItem);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.desktop.Clipboard"), QStringLiteral("ClearHistory"), Slm::Permissions::Capability::ClipboardClearHistory);
}

bool DBusInterface::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    if (capability == Slm::Permissions::Capability::ClipboardReadHistoryContent) {
        ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("High"));
    } else {
        ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Medium"));
    }

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (!d.isAllowed()) {
        sendErrorReply(QStringLiteral("org.slm.PermissionDenied"), d.reason);
        return false;
    }
    return true;
}

bool DBusInterface::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!bus.isConnected() || !iface) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots
                                           | QDBusConnection::ExportAllSignals
                                           | QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }

    m_registered = true;
    emit serviceRegisteredChanged();
    return true;
}

bool DBusInterface::serviceRegistered() const
{
    return m_registered;
}

QString DBusInterface::BackendMode() const
{
    return m_service ? m_service->backendMode() : QStringLiteral("unknown");
}

QVariantList DBusInterface::GetHistory(int limit)
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardReadHistoryPreview, QStringLiteral("GetHistory"))) {
        return {};
    }
    return m_service ? m_service->getHistory(limit) : QVariantList{};
}

QVariantList DBusInterface::Search(const QString &query, int limit)
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardReadHistoryPreview, QStringLiteral("Search"))) {
        return {};
    }
    return m_service ? m_service->search(query, limit) : QVariantList{};
}

bool DBusInterface::PasteItem(qlonglong id)
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardReadHistoryContent, QStringLiteral("PasteItem"))) {
        return false;
    }
    return m_service && m_service->pasteItem(id);
}

bool DBusInterface::DeleteItem(qlonglong id)
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardDeleteHistory, QStringLiteral("DeleteItem"))) {
        return false;
    }
    return m_service && m_service->deleteItem(id);
}

bool DBusInterface::PinItem(qlonglong id, bool pinned)
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardPinItem, QStringLiteral("PinItem"))) {
        return false;
    }
    return m_service && m_service->pinItem(id, pinned);
}

bool DBusInterface::ClearHistory()
{
    if (!checkPermission(Slm::Permissions::Capability::ClipboardClearHistory, QStringLiteral("ClearHistory"))) {
        return false;
    }
    return m_service && m_service->clearHistory();
}

} // namespace Slm::Clipboard
