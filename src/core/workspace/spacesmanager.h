#pragma once

#include <QObject>
#include <QHash>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>

class WindowWorkspaceBinding
{
public:
    int spaceFor(const QString &viewId, int defaultSpace) const;
    bool assign(const QString &viewId, int space, int defaultSpace);
    bool contains(const QString &viewId) const;
    bool remove(const QString &viewId);
    void remap(const QHash<int, int> &spaceRemap);
    QList<QString> viewIds() const;
    QVariantMap snapshot() const;
    int size() const;
    int maxAssignedSpace() const;
    const QHash<QString, int> &raw() const;
    void clear();
    void load(const QHash<QString, int> &map);

private:
    QHash<QString, int> m_viewToSpace;
};

class SpacesManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeSpace READ activeSpace WRITE setActiveSpace NOTIFY activeSpaceChanged)
    Q_PROPERTY(int spaceCount READ spaceCount WRITE setSpaceCount NOTIFY spaceCountChanged)
    Q_PROPERTY(QVariantList workspaceModel READ workspaceModel NOTIFY workspacesChanged)

public:
    explicit SpacesManager(QObject *parent = nullptr);

    int activeSpace() const;
    int spaceCount() const;
    void setActiveSpace(int space);
    void setSpaceCount(int count);

    Q_INVOKABLE int windowSpace(const QString &viewId) const;
    Q_INVOKABLE bool windowBelongsToActive(const QString &viewId) const;
    Q_INVOKABLE void assignWindowToSpace(const QString &viewId, int space);
    Q_INVOKABLE void cycleWindowSpace(const QString &viewId, int delta);
    Q_INVOKABLE void clearMissingAssignments(const QVariantList &activeViewIds);
    Q_INVOKABLE QVariantList spaces() const;
    Q_INVOKABLE QVariantList workspaceModel() const;
    Q_INVOKABLE QVariantMap assignmentSnapshot() const;
    Q_INVOKABLE bool hasTrailingEmptyWorkspace() const;
    Q_INVOKABLE bool isActiveSpaceValid() const;
    Q_INVOKABLE bool enforceInvariants();
    Q_INVOKABLE QVariantMap invariantState() const;

signals:
    void activeSpaceChanged();
    void spaceCountChanged();
    void assignmentsChanged();
    void workspacesChanged();
    void windowWorkspaceChanged(const QString &viewId, int workspaceId);
    void windowWorkspaceMoved(const QString &viewId, int fromWorkspaceId, int toWorkspaceId);

private:
    struct Workspace {
        int id = 1;
        bool isActive = false;
        bool isEmpty = true;
        bool isTrailingEmpty = false;
        int windowCount = 0;
    };

    Workspace workspaceAt(int id) const;
    QHash<int, int> occupancyBySpace() const;
    void emitWorkspacesChanged();
    void load();
    void save();
    int clampSpace(int space) const;
    bool normalizeDynamicSpaces();
    bool ensureTrailingEmptyWorkspace();

    QSettings m_settings;
    int m_activeSpace = 1;
    int m_spaceCount = 4;
    WindowWorkspaceBinding m_binding;
};
