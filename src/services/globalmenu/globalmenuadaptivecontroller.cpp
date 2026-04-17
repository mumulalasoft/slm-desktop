#include "globalmenuadaptivecontroller.h"

GlobalMenuAdaptiveController::GlobalMenuAdaptiveController(QObject *parent)
    : QObject(parent)
{}

QString GlobalMenuAdaptiveController::mode() const
{
    if (!m_userOverride.isEmpty())
        return m_userOverride;
    return m_computedMode;
}

QString GlobalMenuAdaptiveController::userOverride() const
{
    return m_userOverride;
}

void GlobalMenuAdaptiveController::setUserOverride(const QString &override)
{
    const QString v = override.trimmed().toLower();
    const bool hadOverride = !m_userOverride.isEmpty();
    if (v == m_userOverride)
        return;
    m_userOverride = (v == QLatin1String("full")
                      || v == QLatin1String("compact")
                      || v == QLatin1String("focus"))
                         ? v
                         : QString{};
    if (hadOverride && m_userOverride.isEmpty()) {
        recompute();
    }
    emit modeChanged();
}

void GlobalMenuAdaptiveController::setAvailableWidth(int width)
{
    if (m_availableWidth == width)
        return;
    m_availableWidth = width;
    recompute();
}

void GlobalMenuAdaptiveController::setWindowFullscreen(bool fullscreen)
{
    if (m_windowFullscreen == fullscreen)
        return;
    m_windowFullscreen = fullscreen;
    recompute();
}

void GlobalMenuAdaptiveController::recompute()
{
    if (!m_userOverride.isEmpty())
        return; // user override is active — don't recompute

    QString next;
    if (m_windowFullscreen) {
        next = QStringLiteral("focus");
    } else if (m_availableWidth < kCompactThreshold) {
        next = QStringLiteral("compact");
    } else {
        next = QStringLiteral("full");
    }

    if (next == m_computedMode)
        return;
    m_computedMode = next;
    emit modeChanged();
}
