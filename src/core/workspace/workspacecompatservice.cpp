#include "workspacecompatservice.h"

#include "spacesmanager.h"
#include "windowingbackendmanager.h"
#include "workspacemanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace {
constexpr const char kLegacyService[] = "org.desktop_shell.WorkspaceManager";
constexpr const char kLegacyPath[] = "/org/desktop_shell/WorkspaceManager";
}

WorkspaceCompatService::WorkspaceCompatService(WorkspaceManager *workspaceManager,
                                               WindowingBackendManager *windowingBackend,
                                               SpacesManager *spacesManager,
                                               QObject *parent)
    : QObject(parent)
    , m_workspaceManager(workspaceManager)
    , m_windowingBackend(windowingBackend)
    , m_spacesManager(spacesManager)
{
    registerDbusService();

    if (m_workspaceManager) {
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceShown,
                this, &WorkspaceCompatService::WorkspaceShown);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceVisibilityChanged,
                this, &WorkspaceCompatService::WorkspaceVisibilityChanged);
        // Canonical workspace signals are bridged to legacy overview aliases.
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceShown,
                this, &WorkspaceCompatService::OverviewShown);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceVisibilityChanged,
                this, &WorkspaceCompatService::OverviewVisibilityChanged);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceChanged,
                this, &WorkspaceCompatService::WorkspaceChanged);
        connect(m_workspaceManager, &WorkspaceManager::WindowAttention,
                this, &WorkspaceCompatService::WindowAttention);
    }
}

WorkspaceCompatService::~WorkspaceCompatService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kLegacyPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kLegacyService));
}

bool WorkspaceCompatService::serviceRegistered() const
{
    return m_serviceRegistered;
}

void WorkspaceCompatService::ShowOverview()
{
    if (m_workspaceManager) {
        m_workspaceManager->ShowWorkspace();
    }
}

void WorkspaceCompatService::HideOverview()
{
    if (m_workspaceManager) {
        m_workspaceManager->HideWorkspace();
    }
}

void WorkspaceCompatService::ToggleOverview()
{
    if (m_workspaceManager) {
        m_workspaceManager->ToggleWorkspace();
    }
}

void WorkspaceCompatService::ShowWorkspace()
{
    if (m_workspaceManager) {
        m_workspaceManager->ShowWorkspace();
    }
}

void WorkspaceCompatService::HideWorkspace()
{
    if (m_workspaceManager) {
        m_workspaceManager->HideWorkspace();
    }
}

void WorkspaceCompatService::ToggleWorkspace()
{
    if (m_workspaceManager) {
        m_workspaceManager->ToggleWorkspace();
    }
}

void WorkspaceCompatService::ShowAppGrid()
{
    if (m_workspaceManager) {
        m_workspaceManager->ShowAppGrid();
    }
}

void WorkspaceCompatService::ShowDesktop()
{
    if (m_workspaceManager) {
        m_workspaceManager->ShowDesktop();
    }
}

bool WorkspaceCompatService::PresentWindow(const QString &app_id)
{
    return m_workspaceManager && m_workspaceManager->PresentWindow(app_id);
}

bool WorkspaceCompatService::PresentView(const QString &view_id)
{
    return m_workspaceManager && m_workspaceManager->PresentView(view_id);
}

bool WorkspaceCompatService::CloseView(const QString &view_id)
{
    return m_workspaceManager && m_workspaceManager->CloseView(view_id);
}

void WorkspaceCompatService::SwitchWorkspace(int index)
{
    if (m_workspaceManager) {
        m_workspaceManager->SwitchWorkspace(index);
    }
}

bool WorkspaceCompatService::MoveWindowToWorkspace(const QVariant &window, int index)
{
    return m_workspaceManager && m_workspaceManager->MoveWindowToWorkspace(window, index);
}

QVariantMap WorkspaceCompatService::DiagnosticSnapshot() const
{
    QVariantMap out;
    out.insert(QStringLiteral("service"), QString::fromLatin1(kLegacyService));
    out.insert(QStringLiteral("path"), QString::fromLatin1(kLegacyPath));
    out.insert(QStringLiteral("serviceRegistered"), m_serviceRegistered);
    out.insert(QStringLiteral("backend"),
               m_windowingBackend ? m_windowingBackend->backend() : QStringLiteral("unknown"));
    out.insert(QStringLiteral("connected"),
               m_windowingBackend && m_windowingBackend->connected());
    if (m_spacesManager) {
        out.insert(QStringLiteral("activeSpace"), m_spacesManager->activeSpace());
        out.insert(QStringLiteral("spaceCount"), m_spacesManager->spaceCount());
    } else {
        out.insert(QStringLiteral("activeSpace"), 1);
        out.insert(QStringLiteral("spaceCount"), 1);
    }
    out.insert(QStringLiteral("compatAlias"), true);
    return out;
}

void WorkspaceCompatService::registerDbusService()
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

    if (iface->isServiceRegistered(QString::fromLatin1(kLegacyService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kLegacyService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kLegacyPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kLegacyService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}
