#include "shellpolicycontroller.h"

#include "shellstatecontroller.h"
#include "../workspace/windowingbackendmanager.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <QVariant>

Q_LOGGING_CATEGORY(slmShellPolicy, "slm.shell.policy")

namespace {
QString normalizedAppId(QString value)
{
    value = value.trimmed().toLower();
    if (value.endsWith(QStringLiteral(".desktop"))) {
        value.chop(8);
    }
    return value;
}

bool boolValue(const QVariantMap &map, const QString &key, bool fallback = false)
{
    if (!map.contains(key)) {
        return fallback;
    }
    const QVariant value = map.value(key);
    if (value.typeId() == QMetaType::Bool) {
        return value.toBool();
    }
    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1")
        || text == QStringLiteral("yes") || text == QStringLiteral("on")) {
        return true;
    }
    if (text == QStringLiteral("false") || text == QStringLiteral("0")
        || text == QStringLiteral("no") || text == QStringLiteral("off")) {
        return false;
    }
    return fallback;
}

QRect windowGeometry(const QVariantMap &window)
{
    return QRect(window.value(QStringLiteral("x")).toInt(),
                 window.value(QStringLiteral("y")).toInt(),
                 qMax(0, window.value(QStringLiteral("width")).toInt()),
                 qMax(0, window.value(QStringLiteral("height")).toInt()));
}
}

ShellPolicyController::ShellPolicyController(QObject *parent)
    : QObject(parent)
{
    m_revealTimer.setSingleShot(true);
    m_revealTimer.setInterval(m_revealTimeoutMs);
    connect(&m_revealTimer, &QTimer::timeout, this, [this]() {
        clearReveal(QStringLiteral("reveal-timeout"));
    });

    const auto refreshForScreens = [this]() {
        if (!m_windows.isEmpty()) {
            applyEvaluation(evaluateWindows(m_windows), QStringLiteral("screen-geometry-changed"));
        }
    };
    connect(qGuiApp, &QGuiApplication::screenAdded, this, refreshForScreens);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, refreshForScreens);
}

ShellPolicyController::ShellPolicyController(ShellStateController *stateController,
                                             WindowingBackendManager *windowingBackend,
                                             QObject *parent)
    : ShellPolicyController(parent)
{
    setStateController(stateController);
    setWindowingBackend(windowingBackend);
}

void ShellPolicyController::setStateController(ShellStateController *stateController)
{
    if (m_stateController == stateController) {
        return;
    }
    if (m_stateController) {
        disconnect(m_stateController, nullptr, this, nullptr);
    }
    m_stateController = stateController;
    if (m_stateController) {
        connect(m_stateController, &ShellStateController::lockScreenActiveChanged,
                this, [this](bool) {
            applyEvaluation(evaluateWindows(m_windows), QStringLiteral("lockscreen"));
        });
    }
    applyEvaluation(evaluateWindows(m_windows), QStringLiteral("state-controller"));
}

void ShellPolicyController::setWindowingBackend(WindowingBackendManager *windowingBackend)
{
    if (m_windowingBackend == windowingBackend) {
        return;
    }
    if (m_windowingBackend) {
        disconnect(m_windowingBackend, nullptr, this, nullptr);
    }
    if (m_compositorState) {
        disconnect(m_compositorState, nullptr, this, nullptr);
        m_compositorState.clear();
    }

    m_windowingBackend = windowingBackend;
    if (!m_windowingBackend) {
        setWindowSnapshot({}, QStringLiteral("windowing-backend-cleared"));
        return;
    }

    connect(m_windowingBackend, &WindowingBackendManager::eventReceived,
            this, [this](const QString &event, const QVariantMap &payload) {
        Q_UNUSED(payload)
        refreshFromCompositorState();
        qCInfo(slmShellPolicy).noquote()
            << "[FULLSCREEN]"
            << "event=" + event
            << "policy=" + visibilityPolicyName();
    });

    m_compositorState = m_windowingBackend->compositorStateObject();
    if (m_compositorState) {
        connect(m_compositorState, SIGNAL(lastEventChanged()),
                this, SLOT(refreshFromCompositorState()));
    }
    refreshFromCompositorState();
}

ShellPolicyController::VisibilityPolicy ShellPolicyController::visibilityPolicy() const
{
    return m_policy;
}

QString ShellPolicyController::visibilityPolicyName() const
{
    return policyName(m_policy);
}

bool ShellPolicyController::fullscreenActive() const
{
    return m_fullscreenActive;
}

bool ShellPolicyController::immersiveFullscreen() const
{
    return m_policy == ImmersiveFullscreen;
}

bool ShellPolicyController::presentationActive() const
{
    return m_policy == Presentation;
}

bool ShellPolicyController::locked() const
{
    return m_policy == Locked;
}

QString ShellPolicyController::focusedAppId() const
{
    return m_focusedAppId;
}

QString ShellPolicyController::focusedWindowTitle() const
{
    return m_focusedWindowTitle;
}

QVariantMap ShellPolicyController::focusedWindow() const
{
    return m_focusedWindow;
}

bool ShellPolicyController::crownSurfaceVisible() const
{
    return m_policy == Normal || m_policy == AppFullscreen;
}

bool ShellPolicyController::crownContentVisible() const
{
    return m_policy == Normal || (m_policy == AppFullscreen && m_topEdgeRevealActive);
}

bool ShellPolicyController::appDeckSurfaceVisible() const
{
    return m_policy == Normal || m_policy == AppFullscreen;
}

bool ShellPolicyController::appDeckContentVisible() const
{
    return m_policy == Normal || (m_policy == AppFullscreen && m_bottomEdgeRevealActive);
}

bool ShellPolicyController::notificationsAllowed() const
{
    return m_policy == Normal || m_policy == AppFullscreen;
}

bool ShellPolicyController::notificationsSuppressed() const
{
    return !notificationsAllowed();
}

bool ShellPolicyController::compactNotifications() const
{
    return m_policy == AppFullscreen;
}

bool ShellPolicyController::edgeRevealEnabled() const
{
    return m_policy == AppFullscreen;
}

bool ShellPolicyController::topEdgeRevealActive() const
{
    return m_topEdgeRevealActive;
}

bool ShellPolicyController::bottomEdgeRevealActive() const
{
    return m_bottomEdgeRevealActive;
}

int ShellPolicyController::edgeRevealSize() const
{
    return m_edgeRevealSize;
}

int ShellPolicyController::normalPolicy() const
{
    return Normal;
}

int ShellPolicyController::appFullscreenPolicy() const
{
    return AppFullscreen;
}

int ShellPolicyController::immersiveFullscreenPolicy() const
{
    return ImmersiveFullscreen;
}

int ShellPolicyController::presentationPolicy() const
{
    return Presentation;
}

int ShellPolicyController::lockedPolicy() const
{
    return Locked;
}

void ShellPolicyController::setEdgeRevealSize(int size)
{
    const int next = qBound(1, size, 24);
    if (m_edgeRevealSize == next) {
        return;
    }
    m_edgeRevealSize = next;
    qCInfo(slmShellPolicy).noquote()
        << "[REVEAL]"
        << "edgeRevealSize=" + QString::number(m_edgeRevealSize);
    emit edgeRevealSizeChanged();
    emit surfacePolicyChanged();
}

QString ShellPolicyController::policyName(int policy) const
{
    switch (static_cast<VisibilityPolicy>(policy)) {
    case Normal:
        return QStringLiteral("Normal");
    case AppFullscreen:
        return QStringLiteral("AppFullscreen");
    case ImmersiveFullscreen:
        return QStringLiteral("ImmersiveFullscreen");
    case Presentation:
        return QStringLiteral("Presentation");
    case Locked:
        return QStringLiteral("Locked");
    }
    return QStringLiteral("Normal");
}

void ShellPolicyController::setWindowSnapshot(const QVariantList &windows, const QString &reason)
{
    m_windows = windows;
    applyEvaluation(evaluateWindows(m_windows), reason.isEmpty() ? QStringLiteral("snapshot") : reason);
}

void ShellPolicyController::requestTopEdgeReveal(const QString &reason)
{
    if (!edgeRevealEnabled()) {
        qCInfo(slmShellPolicy).noquote()
            << "[REVEAL]"
            << "edge=top"
            << "action=suppressed"
            << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason)
            << "policy=" + visibilityPolicyName();
        return;
    }
    const bool changed = !m_topEdgeRevealActive || m_bottomEdgeRevealActive;
    m_topEdgeRevealActive = true;
    m_bottomEdgeRevealActive = false;
    restartRevealTimeout();
    qCInfo(slmShellPolicy).noquote()
        << "[REVEAL]"
        << "edge=top"
        << "action=activate"
        << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason)
        << "policy=" + visibilityPolicyName();
    if (changed) {
        emit surfacePolicyChanged();
        logLayerState(QStringLiteral("top-reveal"));
    }
}

void ShellPolicyController::requestBottomEdgeReveal(const QString &reason)
{
    if (!edgeRevealEnabled()) {
        qCInfo(slmShellPolicy).noquote()
            << "[REVEAL]"
            << "edge=bottom"
            << "action=suppressed"
            << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason)
            << "policy=" + visibilityPolicyName();
        return;
    }
    const bool changed = !m_bottomEdgeRevealActive || m_topEdgeRevealActive;
    m_bottomEdgeRevealActive = true;
    m_topEdgeRevealActive = false;
    restartRevealTimeout();
    qCInfo(slmShellPolicy).noquote()
        << "[REVEAL]"
        << "edge=bottom"
        << "action=activate"
        << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason)
        << "policy=" + visibilityPolicyName();
    if (changed) {
        emit surfacePolicyChanged();
        logLayerState(QStringLiteral("bottom-reveal"));
    }
}

void ShellPolicyController::clearReveal(const QString &reason)
{
    clearRevealInternal(reason.isEmpty() ? QStringLiteral("clear") : reason, true);
}

void ShellPolicyController::refreshFromCompositorState()
{
    if (!m_compositorState) {
        return;
    }

    int count = 0;
    QMetaObject::invokeMethod(m_compositorState, "windowCount",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, count));
    QVariantList windows;
    windows.reserve(qMax(0, count));
    for (int i = 0; i < count; ++i) {
        QVariantMap window;
        QMetaObject::invokeMethod(m_compositorState, "windowAt",
                                  Qt::DirectConnection,
                                  Q_RETURN_ARG(QVariantMap, window),
                                  Q_ARG(int, i));
        if (!window.isEmpty()) {
            windows.push_back(window);
        }
    }

    QString reason = QStringLiteral("compositor-state");
    const QVariant lastEvent = m_compositorState->property("lastEvent");
    if (lastEvent.canConvert<QVariantMap>()) {
        const QVariantMap event = lastEvent.toMap();
        const QString eventName = event.value(QStringLiteral("event")).toString().trimmed();
        if (!eventName.isEmpty()) {
            reason = eventName;
        }
    }
    setWindowSnapshot(windows, reason);
}

ShellPolicyController::Evaluation ShellPolicyController::evaluateWindows(const QVariantList &windows) const
{
    Evaluation evaluation;
    if (m_stateController && m_stateController->lockScreenActive()) {
        evaluation.policy = Locked;
        return evaluation;
    }

    QVariantMap focused;
    QVariantMap fullscreenFallback;
    for (const QVariant &entry : windows) {
        const QVariantMap window = entry.toMap();
        if (!isMappedCandidate(window) || isShellWindow(window)) {
            continue;
        }
        if (boolValue(window, QStringLiteral("focused"))) {
            focused = window;
            break;
        }
        if (fullscreenFallback.isEmpty() && isFullscreenWindow(window)) {
            fullscreenFallback = window;
        }
    }
    if (focused.isEmpty()) {
        focused = fullscreenFallback;
    }
    if (focused.isEmpty()) {
        return evaluation;
    }

    evaluation.focusedWindow = focused;
    evaluation.appId = focused.value(QStringLiteral("appId")).toString().trimmed();
    evaluation.title = focused.value(QStringLiteral("title")).toString().trimmed();
    evaluation.fullscreen = isFullscreenWindow(focused);
    evaluation.policy = evaluation.fullscreen ? classifyFullscreenIntent(focused) : Normal;
    return evaluation;
}

ShellPolicyController::VisibilityPolicy
ShellPolicyController::classifyFullscreenIntent(const QVariantMap &window) const
{
    const QString appId = normalizedAppId(window.value(QStringLiteral("appId")).toString());
    if (appId.isEmpty()) {
        return AppFullscreen;
    }

    if (appIdContainsAny(appId, {
            QStringLiteral("impress"),
            QStringLiteral("powerpnt"),
            QStringLiteral("powerpoint"),
            QStringLiteral("keynote"),
            QStringLiteral("presenter"),
            QStringLiteral("slides"),
        })) {
        return Presentation;
    }

    if (appIdContainsAny(appId, {
            QStringLiteral("steam"),
            QStringLiteral("lutris"),
            QStringLiteral("heroic"),
            QStringLiteral("gamescope"),
            QStringLiteral("wine"),
            QStringLiteral("proton"),
            QStringLiteral("minecraft"),
            QStringLiteral("prismlauncher"),
            QStringLiteral("mpv"),
            QStringLiteral("vlc"),
            QStringLiteral("kodi"),
            QStringLiteral("plex"),
            QStringLiteral("jellyfin"),
            QStringLiteral("remmina"),
            QStringLiteral("virt-manager"),
            QStringLiteral("virt-viewer"),
            QStringLiteral("virtualbox"),
            QStringLiteral("qemu"),
            QStringLiteral("gnome-boxes"),
            QStringLiteral("parsec"),
            QStringLiteral("moonlight"),
        })) {
        return ImmersiveFullscreen;
    }

    return AppFullscreen;
}

bool ShellPolicyController::isFullscreenWindow(const QVariantMap &window) const
{
    if (boolValue(window, QStringLiteral("fullscreen"))
        || boolValue(window, QStringLiteral("fullScreen"))
        || boolValue(window, QStringLiteral("full-screen"))) {
        return true;
    }

    const QRect geom = windowGeometry(window);
    if (geom.width() <= 0 || geom.height() <= 0) {
        return false;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (!screen) {
            continue;
        }
        const QRect screenGeom = screen->geometry();
        if (screenGeom.width() <= 0 || screenGeom.height() <= 0) {
            continue;
        }
        const QRect overlap = geom.intersected(screenGeom);
        const qint64 screenArea = qint64(screenGeom.width()) * qint64(screenGeom.height());
        const qint64 overlapArea = qint64(overlap.width()) * qint64(overlap.height());
        if (screenArea > 0 && double(overlapArea) / double(screenArea) >= 0.94) {
            return true;
        }
    }
    return false;
}

bool ShellPolicyController::isMappedCandidate(const QVariantMap &window) const
{
    const bool mapped = boolValue(window, QStringLiteral("mapped"), true);
    const bool minimized = boolValue(window, QStringLiteral("minimized"), false);
    return mapped && !minimized;
}

bool ShellPolicyController::isShellWindow(const QVariantMap &window) const
{
    const QString appId = normalizedAppId(window.value(QStringLiteral("appId")).toString());
    const QString appKey = appId;
    const QString title = window.value(QStringLiteral("title")).toString().trimmed().toLower();
    const QString resourceClass = window.value(QStringLiteral("resourceClass")).toString().trimmed().toLower();
    const QString windowClass = window.value(QStringLiteral("class")).toString().trimmed().toLower();
    const QString scope = window.value(QStringLiteral("scope")).toString().trimmed().toLower();

    // Layer-shell surfaces may be reported without an appId. Do not let shell
    // chrome (AppDeck/Crown/etc.) classify itself as the focused fullscreen app,
    // otherwise the policy hides the AppDeck content while it is opening.
    if (title.startsWith(QStringLiteral("slm "))
            || title.contains(QStringLiteral("appdeck"))
            || title.contains(QStringLiteral("desktop shell"))
            || scope.startsWith(QStringLiteral("slm-"))
            || scope.contains(QStringLiteral("appdeck"))
            || resourceClass.contains(QStringLiteral("slm"))
            || resourceClass.contains(QStringLiteral("desktop_shell"))
            || windowClass.contains(QStringLiteral("slm"))
            || windowClass.contains(QStringLiteral("desktop_shell"))) {
        return true;
    }

    if (appKey.isEmpty()) {
        return false;
    }
    return appKey == QStringLiteral("slm-shell")
            || appKey == QStringLiteral("slm-desktop")
            || appKey == QStringLiteral("slm_desktop")
            || appKey == QStringLiteral("desktop-shell")
            || appKey == QStringLiteral("desktop_shell")
            || appId.startsWith(QStringLiteral("org.slm."))
            || appId.startsWith(QStringLiteral("io.slm."));
}

bool ShellPolicyController::appIdContainsAny(const QString &appId, const QStringList &tokens) const
{
    for (const QString &token : tokens) {
        if (!token.isEmpty() && appId.contains(token)) {
            return true;
        }
    }
    return false;
}

void ShellPolicyController::applyEvaluation(const Evaluation &evaluation, const QString &reason)
{
    setVisibilityPolicy(evaluation.policy,
                        evaluation.fullscreen,
                        evaluation.focusedWindow,
                        evaluation.appId,
                        evaluation.title,
                        reason);
}

void ShellPolicyController::setVisibilityPolicy(VisibilityPolicy policy,
                                                bool fullscreen,
                                                const QVariantMap &focusedWindow,
                                                const QString &appId,
                                                const QString &title,
                                                const QString &reason)
{
    const bool policyChanged = m_policy != policy;
    const bool stateChanged = policyChanged
            || m_fullscreenActive != fullscreen
            || m_focusedAppId != appId
            || m_focusedWindowTitle != title
            || m_focusedWindow != focusedWindow;
    if (!stateChanged) {
        return;
    }

    const QString oldName = visibilityPolicyName();
    m_policy = policy;
    m_fullscreenActive = fullscreen;
    m_focusedWindow = focusedWindow;
    m_focusedAppId = appId;
    m_focusedWindowTitle = title;

    if (!edgeRevealEnabled()) {
        clearRevealInternal(QStringLiteral("policy-transition"), false);
    }

    qCInfo(slmShellPolicy).noquote()
        << "[SHELL_POLICY]"
        << "transition=" + oldName + "->" + visibilityPolicyName()
        << "fullscreen=" + QString::fromLatin1(fullscreen ? "true" : "false")
        << "appId=" + (appId.isEmpty() ? QStringLiteral("<none>") : appId)
        << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason);

    if (fullscreen) {
        const QRect geom = windowGeometry(focusedWindow);
        qCInfo(slmShellPolicy).noquote()
            << "[FULLSCREEN]"
            << "ownerAppId=" + (appId.isEmpty() ? QStringLiteral("<unknown>") : appId)
            << "viewId=" + focusedWindow.value(QStringLiteral("viewId")).toString()
            << "geometry=" + QStringLiteral("%1,%2 %3x%4")
                            .arg(geom.x()).arg(geom.y()).arg(geom.width()).arg(geom.height())
            << "policy=" + visibilityPolicyName();
    }
    if (m_policy == ImmersiveFullscreen) {
        qCInfo(slmShellPolicy).noquote()
            << "[IMMERSIVE]"
            << "appId=" + (appId.isEmpty() ? QStringLiteral("<unknown>") : appId)
            << "edgeReveal=disabled"
            << "notifications=suppressed";
    }

    emit policyStateChanged();
    emit surfacePolicyChanged();
    logLayerState(reason);
}

void ShellPolicyController::clearRevealInternal(const QString &reason, bool emitChanges)
{
    if (!m_topEdgeRevealActive && !m_bottomEdgeRevealActive) {
        m_revealTimer.stop();
        return;
    }
    m_topEdgeRevealActive = false;
    m_bottomEdgeRevealActive = false;
    m_revealTimer.stop();
    qCInfo(slmShellPolicy).noquote()
        << "[REVEAL]"
        << "action=clear"
        << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason)
        << "policy=" + visibilityPolicyName();
    if (emitChanges) {
        emit surfacePolicyChanged();
        logLayerState(QStringLiteral("reveal-clear"));
    }
}

void ShellPolicyController::restartRevealTimeout()
{
    m_revealTimer.setInterval(m_revealTimeoutMs);
    m_revealTimer.start();
}

void ShellPolicyController::logLayerState(const QString &reason) const
{
    qCInfo(slmShellPolicy).noquote()
        << "[LAYER_STATE]"
        << "policy=" + visibilityPolicyName()
        << "crownSurface=" + QString::fromLatin1(crownSurfaceVisible() ? "true" : "false")
        << "crownContent=" + QString::fromLatin1(crownContentVisible() ? "true" : "false")
        << "appDeckSurface=" + QString::fromLatin1(appDeckSurfaceVisible() ? "true" : "false")
        << "appDeckContent=" + QString::fromLatin1(appDeckContentVisible() ? "true" : "false")
        << "notifications=" + QString::fromLatin1(notificationsAllowed() ? "allowed" : "suppressed")
        << "edgeReveal=" + QString::fromLatin1(edgeRevealEnabled() ? "enabled" : "disabled")
        << "reason=" + (reason.isEmpty() ? QStringLiteral("unspecified") : reason);
}
