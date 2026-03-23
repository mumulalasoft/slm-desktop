#pragma once

#include <QObject>
#include <QVariantList>

class SpacesManager;
class WorkspaceManager;

class WorkspaceStripModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int activeWorkspace READ activeWorkspace NOTIFY activeWorkspaceChanged)
    Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY workspaceCountChanged)
    Q_PROPERTY(int trailingPlaceholderWorkspace READ trailingPlaceholderWorkspace NOTIFY rowsChanged)
    Q_PROPERTY(bool hasTrailingPlaceholder READ hasTrailingPlaceholder NOTIFY rowsChanged)

public:
    explicit WorkspaceStripModel(SpacesManager *spacesManager,
                                 WorkspaceManager *workspaceManager,
                                 QObject *parent = nullptr);

    QVariantList rows() const;
    int activeWorkspace() const;
    int workspaceCount() const;
    int trailingPlaceholderWorkspace() const;
    bool hasTrailingPlaceholder() const;

    Q_INVOKABLE bool switchToWorkspace(int workspaceId);

signals:
    void rowsChanged();
    void activeWorkspaceChanged();
    void workspaceCountChanged();

private:
    void refreshRows();

    SpacesManager *m_spacesManager = nullptr;
    WorkspaceManager *m_workspaceManager = nullptr;
    QVariantList m_rows;
    int m_activeWorkspace = 1;
    int m_workspaceCount = 1;
};

