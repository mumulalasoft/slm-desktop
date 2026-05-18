#include "appdeckcompositorevents.h"

#include "src/core/shell/shellpolicycontroller.h"
#include "src/core/shell/shellstatecontroller.h"

#include <QGuiApplication>
#include <QScreen>

AppDeckCompositorEvents::AppDeckCompositorEvents(ShellPolicyController *policy,
                                                 ShellStateController *state,
                                                 QObject *parent)
    : QObject(parent)
    , m_policy(policy)
    , m_state(state)
{
    if (m_policy) {
        connect(m_policy, &ShellPolicyController::policyStateChanged,
                this, &AppDeckCompositorEvents::onPolicyChanged);
        // Seed initial fullscreen state without emitting (avoid a spurious
        // false→false signal on startup).
        m_fullscreenActive = m_policy->fullscreenActive();
        m_lastAppId = m_policy->focusedAppId();
    }

    if (QGuiApplication *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(app, &QGuiApplication::primaryScreenChanged,
                this, &AppDeckCompositorEvents::onPrimaryScreenChanged);
        wireScreen(app->primaryScreen());
    }
}

void AppDeckCompositorEvents::wireScreen(QScreen *screen)
{
    if (m_currentScreen) {
        disconnect(m_currentScreen, nullptr, this, nullptr);
    }
    m_currentScreen = screen;
    if (!screen) {
        return;
    }
    connect(screen, &QScreen::geometryChanged,
            this, &AppDeckCompositorEvents::onPrimaryScreenGeometryChanged);
    connect(screen, &QScreen::logicalDotsPerInchChanged,
            this, &AppDeckCompositorEvents::onLogicalDotsPerInchChanged);

    const QRect g = screen->geometry();
    if (g != m_screenGeometry) {
        m_screenGeometry = g;
        emit screenGeometryChanged(m_screenGeometry);
    }
    const qreal s = screen->devicePixelRatio();
    if (!qFuzzyCompare(s, m_scaleFactor)) {
        m_scaleFactor = s;
        emit scaleFactorChanged(m_scaleFactor);
    }
}

void AppDeckCompositorEvents::onPolicyChanged()
{
    if (!m_policy) {
        return;
    }
    const bool nowFullscreen = m_policy->fullscreenActive();
    if (nowFullscreen != m_fullscreenActive) {
        m_fullscreenActive = nowFullscreen;
        emit fullscreenStateChanged(m_fullscreenActive);
    }
    const QString nowAppId = m_policy->focusedAppId();
    if (nowAppId != m_lastAppId) {
        m_lastAppId = nowAppId;
        if (!nowAppId.isEmpty()) {
            emit appActivated(nowAppId);
        }
    }
}

void AppDeckCompositorEvents::onPrimaryScreenChanged(QScreen *screen)
{
    wireScreen(screen);
}

void AppDeckCompositorEvents::onPrimaryScreenGeometryChanged(const QRect &geometry)
{
    if (geometry != m_screenGeometry) {
        m_screenGeometry = geometry;
        emit screenGeometryChanged(m_screenGeometry);
    }
}

void AppDeckCompositorEvents::onLogicalDotsPerInchChanged(qreal dpi)
{
    Q_UNUSED(dpi)
    if (!m_currentScreen) {
        return;
    }
    const qreal s = m_currentScreen->devicePixelRatio();
    if (!qFuzzyCompare(s, m_scaleFactor)) {
        m_scaleFactor = s;
        emit scaleFactorChanged(m_scaleFactor);
    }
}

void AppDeckCompositorEvents::notifyOutsidePointer(const QPointF &globalPos)
{
    emit outsidePointerPressed(globalPos);
}

void AppDeckCompositorEvents::notifyAppActivated(const QString &appId)
{
    if (appId == m_lastAppId) {
        return;
    }
    m_lastAppId = appId;
    if (!appId.isEmpty()) {
        emit appActivated(appId);
    }
}

void AppDeckCompositorEvents::notifyWorkspaceFocused(int index)
{
    if (index == m_lastWorkspaceIndex) {
        return;
    }
    m_lastWorkspaceIndex = index;
    emit workspaceFocused(index);
}
