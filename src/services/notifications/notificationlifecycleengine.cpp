#include "notificationlifecycleengine.h"

void NotificationLifecycleEngine::onQueued(uint id)
{
    setState(id, State::Queued);
}

void NotificationLifecycleEngine::onDisplayed(uint id, bool grouped)
{
    setState(id, grouped ? State::Grouped : State::Visible);
}

void NotificationLifecycleEngine::onExpanded(uint id)
{
    setState(id, State::Expanded);
}

void NotificationLifecycleEngine::onCollapsed(uint id)
{
    setState(id, State::Collapsing);
}

void NotificationLifecycleEngine::onArchived(uint id)
{
    setState(id, State::Archived);
}

void NotificationLifecycleEngine::clear()
{
    m_stateById.clear();
}

NotificationLifecycleEngine::State NotificationLifecycleEngine::state(uint id) const
{
    return m_stateById.value(id, State::Archived);
}

QString NotificationLifecycleEngine::stateName(State state)
{
    switch (state) {
    case State::Queued:
        return QStringLiteral("Queued");
    case State::Appearing:
        return QStringLiteral("Appearing");
    case State::Visible:
        return QStringLiteral("Visible");
    case State::Grouped:
        return QStringLiteral("Grouped");
    case State::Expanded:
        return QStringLiteral("Expanded");
    case State::Collapsing:
        return QStringLiteral("Collapsing");
    case State::Archived:
    default:
        return QStringLiteral("Archived");
    }
}

void NotificationLifecycleEngine::setState(uint id, State state)
{
    if (id == 0) {
        return;
    }
    m_stateById.insert(id, state);
}
