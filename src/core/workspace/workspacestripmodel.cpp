#include "workspacestripmodel.h"

#include "spacesmanager.h"
#include "workspacemanager.h"

WorkspaceStripModel::WorkspaceStripModel(SpacesManager *spacesManager,
                                         WorkspaceManager *workspaceManager,
                                         QObject *parent)
    : QObject(parent)
    , m_spacesManager(spacesManager)
    , m_workspaceManager(workspaceManager)
{
    if (m_spacesManager) {
        m_activeWorkspace = m_spacesManager->activeSpace();
        m_workspaceCount = m_spacesManager->spaceCount();
        m_rows = m_spacesManager->workspaceModel();

        connect(m_spacesManager, &SpacesManager::activeSpaceChanged, this, [this]() {
            const int next = m_spacesManager ? m_spacesManager->activeSpace() : 1;
            if (next != m_activeWorkspace) {
                m_activeWorkspace = next;
                emit activeWorkspaceChanged();
            }
            refreshRows();
        });
        connect(m_spacesManager, &SpacesManager::spaceCountChanged, this, [this]() {
            const int next = m_spacesManager ? m_spacesManager->spaceCount() : 1;
            if (next != m_workspaceCount) {
                m_workspaceCount = next;
                emit workspaceCountChanged();
            }
            refreshRows();
        });
        connect(m_spacesManager, &SpacesManager::workspacesChanged,
                this, &WorkspaceStripModel::refreshRows);
        connect(m_spacesManager, &SpacesManager::assignmentsChanged,
                this, &WorkspaceStripModel::refreshRows);
    }
}

QVariantList WorkspaceStripModel::rows() const
{
    return m_rows;
}

int WorkspaceStripModel::activeWorkspace() const
{
    return m_activeWorkspace;
}

int WorkspaceStripModel::workspaceCount() const
{
    return m_workspaceCount;
}

int WorkspaceStripModel::trailingPlaceholderWorkspace() const
{
    for (int i = m_rows.size() - 1; i >= 0; --i) {
        const QVariantMap row = m_rows.at(i).toMap();
        if (row.value(QStringLiteral("isTrailingEmpty")).toBool()) {
            return row.value(QStringLiteral("id"),
                             row.value(QStringLiteral("index"), 0)).toInt();
        }
    }
    return 0;
}

bool WorkspaceStripModel::hasTrailingPlaceholder() const
{
    return trailingPlaceholderWorkspace() > 0;
}

bool WorkspaceStripModel::switchToWorkspace(int workspaceId)
{
    if (workspaceId <= 0) {
        return false;
    }
    if (m_workspaceManager) {
        m_workspaceManager->SwitchWorkspace(workspaceId);
        return true;
    }
    if (m_spacesManager) {
        m_spacesManager->setActiveSpace(workspaceId);
        return true;
    }
    return false;
}

void WorkspaceStripModel::refreshRows()
{
    const QVariantList next = m_spacesManager ? m_spacesManager->workspaceModel() : QVariantList{};
    if (next != m_rows) {
        m_rows = next;
        emit rowsChanged();
    }
}
