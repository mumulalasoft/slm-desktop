#pragma once

#include <QHash>
#include <QString>

class NotificationLifecycleEngine
{
public:
    enum class State {
        Queued,
        Appearing,
        Visible,
        Grouped,
        Expanded,
        Collapsing,
        Archived
    };

    void onQueued(uint id);
    void onDisplayed(uint id, bool grouped);
    void onExpanded(uint id);
    void onCollapsed(uint id);
    void onArchived(uint id);
    void clear();

    State state(uint id) const;
    static QString stateName(State state);

private:
    void setState(uint id, State state);

    QHash<uint, State> m_stateById;
};
