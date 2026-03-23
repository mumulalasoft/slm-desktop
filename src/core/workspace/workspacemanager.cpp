#include "workspacemanager.h"

#include "spacesmanager.h"
#include "windowingbackendmanager.h"

#include <QMetaObject>

namespace {
bool isShellWindow(const QVariantMap &w)
{
    const QString app = w.value(QStringLiteral("appId")).toString().trimmed().toLower();
    const QString title = w.value(QStringLiteral("title")).toString().trimmed().toLower();
    return app == QStringLiteral("appdesktop_shell")
           || app == QStringLiteral("desktop_shell")
           || app == QStringLiteral("desktopshell")
           || app.contains(QStringLiteral("desktop_shell"))
           || title == QStringLiteral("desktop shell");
}
}

WorkspaceManager::WorkspaceManager(WindowingBackendManager *windowingBackend,
                                   SpacesManager *spacesManager,
                                   QObject *compositorStateModel,
                                   QObject *parent)
    : QObject(parent)
    , m_windowingBackend(windowingBackend)
    , m_spacesManager(spacesManager)
    , m_compositorStateModel(compositorStateModel)
{
    if (m_spacesManager) {
        connect(m_spacesManager, &SpacesManager::activeSpaceChanged,
                this, &WorkspaceManager::WorkspaceChanged);
        connect(m_spacesManager, &SpacesManager::spaceCountChanged,
                this, &WorkspaceManager::WorkspaceChanged);
        connect(m_spacesManager, &SpacesManager::assignmentsChanged,
                this, &WorkspaceManager::WorkspaceChanged);
        connect(m_spacesManager, &SpacesManager::workspacesChanged,
                this, &WorkspaceManager::WorkspaceChanged);
    }
    if (m_windowingBackend) {
        connect(m_windowingBackend, &WindowingBackendManager::eventReceived,
                this, [this](const QString &event, const QVariantMap &payload) {
            const QString eventName = event.trimmed().toLower();
            const QString payloadEvent = payload.value(QStringLiteral("event")).toString().trimmed().toLower();
            const QString key = !payloadEvent.isEmpty() ? payloadEvent : eventName;

            if (key == QStringLiteral("overview-open")
                    || key == QStringLiteral("overview-toggle")
                    || key == QStringLiteral("workspace-open")
                    || key == QStringLiteral("workspace-toggle")) {
                setWorkspaceVisible(true);
                emit OverviewShown();
                emit WorkspaceShown();
            } else if (key == QStringLiteral("overview-close")
                       || key == QStringLiteral("workspace-close")) {
                setWorkspaceVisible(false);
            }
            if (key == QStringLiteral("workspace-changed") || key == QStringLiteral("space-changed")) {
                emit WorkspaceChanged();
            }
            if (key.startsWith(QStringLiteral("window-"))
                    || key == QStringLiteral("view-added")
                    || key == QStringLiteral("view-removed")
                    || key == QStringLiteral("workspace-changed")
                    || key == QStringLiteral("space-changed")) {
                reconcileWindowWorkspaceAssignments();
            }
            if (key == QStringLiteral("window-attention")
                    || key == QStringLiteral("window-urgent")
                    || key == QStringLiteral("window-demands-attention")) {
                emit WindowAttention(payload);
            }
        });
    }

    reconcileWindowWorkspaceAssignments();
}

void WorkspaceManager::ShowOverview()
{
    ShowWorkspace();
}

void WorkspaceManager::HideOverview()
{
    HideWorkspace();
}

void WorkspaceManager::ToggleOverview()
{
    ToggleWorkspace();
}

void WorkspaceManager::ShowWorkspace()
{
    if (sendCommand(QStringLiteral("workspace on"))
            || sendCommand(QStringLiteral("overview on"))) {
        setWorkspaceVisible(true);
    }
    emit OverviewShown();
    emit WorkspaceShown();
}

void WorkspaceManager::HideWorkspace()
{
    if (sendCommand(QStringLiteral("workspace off"))
            || sendCommand(QStringLiteral("overview off"))) {
        setWorkspaceVisible(false);
    }
}

void WorkspaceManager::ToggleWorkspace()
{
    const bool nextVisible = !m_workspaceVisible;
    if (sendCommand(QStringLiteral("workspace toggle"))
            || sendCommand(QStringLiteral("overview toggle"))) {
        setWorkspaceVisible(nextVisible);
        if (nextVisible) {
            emit OverviewShown();
            emit WorkspaceShown();
        }
    }
}

void WorkspaceManager::ShowAppGrid()
{
    sendCommand(QStringLiteral("launchpad on"));
}

void WorkspaceManager::ShowDesktop()
{
    sendCommand(QStringLiteral("show-desktop"));
}

bool WorkspaceManager::PresentWindow(const QString &app_id)
{
    const QVariantMap w = findWindowByAppId(app_id);
    const QString viewId = w.value(QStringLiteral("viewId")).toString().trimmed();
    if (viewId.isEmpty()) {
        return false;
    }

    const int space = w.value(QStringLiteral("space")).toInt();
    if (m_spacesManager && space > 0) {
        m_spacesManager->setActiveSpace(space);
    }
    return focusWindowByViewId(viewId);
}

bool WorkspaceManager::PresentView(const QString &view_id)
{
    const QString viewId = view_id.trimmed();
    if (viewId.isEmpty()) {
        return false;
    }

    const int n = windowCount();
    for (int i = 0; i < n; ++i) {
        const QVariantMap w = windowAt(i);
        const QString candidate = w.value(QStringLiteral("viewId")).toString().trimmed();
        if (candidate != viewId) {
            continue;
        }
        const int space = w.value(QStringLiteral("space")).toInt();
        if (m_spacesManager && space > 0) {
            m_spacesManager->setActiveSpace(space);
        }
        return focusWindowByViewId(viewId);
    }
    return focusWindowByViewId(viewId);
}

bool WorkspaceManager::CloseView(const QString &view_id)
{
    const QString viewId = view_id.trimmed();
    if (viewId.isEmpty()) {
        return false;
    }
    return sendCommand(QStringLiteral("close-view %1").arg(viewId));
}

void WorkspaceManager::SwitchWorkspace(int index)
{
    if (!m_spacesManager) {
        return;
    }
    m_spacesManager->setActiveSpace(index);
}

void WorkspaceManager::SwitchWorkspaceByDelta(int delta)
{
    if (!m_spacesManager || delta == 0) {
        return;
    }
    const int active = qMax(1, m_spacesManager->activeSpace());
    const int count = qMax(1, m_spacesManager->spaceCount());
    const int target = qBound(1, active + delta, count);
    if (target == active) {
        return;
    }
    SwitchWorkspace(target);
}

bool WorkspaceManager::MoveWindowToWorkspace(const QVariant &window, int index)
{
    if (!m_spacesManager) {
        return false;
    }

    const QString viewId = extractViewId(window);
    if (viewId.isEmpty()) {
        return false;
    }
    // Mapping-layer strategy:
    // keep stable window->workspace assignments in SpacesManager even when
    // compositor backend has no direct move-window-to-desktop command.
    m_spacesManager->assignWindowToSpace(viewId, index);
    return true;
}

bool WorkspaceManager::MoveFocusedWindowByDelta(int delta)
{
    if (!m_spacesManager || delta == 0) {
        return false;
    }

    const QString viewId = focusedUserViewId();
    if (viewId.isEmpty()) {
        return false;
    }

    int current = m_spacesManager->windowSpace(viewId);
    if (current <= 0) {
        current = qMax(1, m_spacesManager->activeSpace());
    }
    const int count = qMax(1, m_spacesManager->spaceCount());
    const int target = qBound(1, current + delta, count);
    if (target == current) {
        return false;
    }
    if (!MoveWindowToWorkspace(viewId, target)) {
        return false;
    }

    // Follow moved focused window so keyboard move feels deterministic.
    SwitchWorkspace(target);
    return true;
}

QVariantList WorkspaceManager::activeViewIds() const
{
    QVariantList out;
    const int n = windowCount();
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QVariantMap w = windowAt(i);
        if (!w.value(QStringLiteral("mapped"), true).toBool()) {
            continue;
        }
        const QString viewId = w.value(QStringLiteral("viewId")).toString().trimmed();
        if (!viewId.isEmpty()) {
            out.push_back(viewId);
        }
    }
    return out;
}

QString WorkspaceManager::focusedUserViewId() const
{
    const int n = windowCount();
    for (int i = 0; i < n; ++i) {
        const QVariantMap w = windowAt(i);
        if (!w.value(QStringLiteral("focused"), false).toBool()) {
            continue;
        }
        if (isShellWindow(w)) {
            continue;
        }
        const QString viewId = w.value(QStringLiteral("viewId")).toString().trimmed();
        if (!viewId.isEmpty()) {
            return viewId;
        }
    }
    return QString();
}

void WorkspaceManager::reconcileWindowWorkspaceAssignments()
{
    if (!m_spacesManager) {
        return;
    }
    m_spacesManager->clearMissingAssignments(activeViewIds());
}

QVariantMap WorkspaceManager::windowAt(int index) const
{
    QVariantMap out;
    if (!m_compositorStateModel) {
        return out;
    }
    QMetaObject::invokeMethod(m_compositorStateModel, "windowAt",
                              Q_RETURN_ARG(QVariantMap, out),
                              Q_ARG(int, index));
    return out;
}

int WorkspaceManager::windowCount() const
{
    if (!m_compositorStateModel) {
        return 0;
    }
    int count = 0;
    QMetaObject::invokeMethod(m_compositorStateModel, "windowCount",
                              Q_RETURN_ARG(int, count));
    return count;
}

QString WorkspaceManager::extractViewId(const QVariant &window) const
{
    if (window.canConvert<QVariantMap>()) {
        return window.toMap().value(QStringLiteral("viewId")).toString().trimmed();
    }

    const QString asString = window.toString().trimmed();
    if (asString.isEmpty()) {
        return QString();
    }

    if (asString.startsWith(QStringLiteral("kwin:"), Qt::CaseInsensitive)) {
        return asString;
    }
    const QVariantMap byApp = findWindowByAppId(asString);
    return byApp.value(QStringLiteral("viewId")).toString().trimmed();
}

QVariantMap WorkspaceManager::findWindowByAppId(const QString &appId) const
{
    QVariantMap out;
    const QString needle = appId.trimmed().toLower();
    if (needle.isEmpty()) {
        return out;
    }

    const int n = windowCount();
    for (int i = 0; i < n; ++i) {
        const QVariantMap w = windowAt(i);
        const QString candidate = w.value(QStringLiteral("appId")).toString().trimmed().toLower();
        if (candidate == needle) {
            return w;
        }
        if (!candidate.isEmpty() && (candidate.contains(needle) || needle.contains(candidate))) {
            out = w;
        }
    }
    return out;
}

bool WorkspaceManager::focusWindowByViewId(const QString &viewId)
{
    const QString id = viewId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    return sendCommand(QStringLiteral("focus-view %1").arg(id));
}

bool WorkspaceManager::sendCommand(const QString &command)
{
    return m_windowingBackend && m_windowingBackend->sendCommand(command);
}

void WorkspaceManager::setWorkspaceVisible(bool visible)
{
    if (m_workspaceVisible == visible) {
        return;
    }
    m_workspaceVisible = visible;
    emit OverviewVisibilityChanged(m_workspaceVisible);
    emit WorkspaceVisibilityChanged(m_workspaceVisible);
}

void WorkspaceManager::setOverviewVisible(bool visible)
{
    setWorkspaceVisible(visible);
}
