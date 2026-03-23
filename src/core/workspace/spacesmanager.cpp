#include "spacesmanager.h"

#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
constexpr const char kSpaceActiveKey[] = "spaces/activeSpace";
constexpr const char kSpaceCountKey[] = "spaces/spaceCount";
constexpr const char kAssignmentsKey[] = "spaces/windowAssignments";
constexpr int kMinSpaces = 1;
constexpr int kMaxSpaces = 24;
}

int WindowWorkspaceBinding::spaceFor(const QString &viewId, int defaultSpace) const
{
    const QString key = viewId.trimmed();
    if (key.isEmpty()) {
        return defaultSpace;
    }
    return m_viewToSpace.value(key, defaultSpace);
}

bool WindowWorkspaceBinding::assign(const QString &viewId, int space, int defaultSpace)
{
    const QString key = viewId.trimmed();
    if (key.isEmpty()) {
        return false;
    }
    const bool existed = m_viewToSpace.contains(key);
    if (existed && m_viewToSpace.value(key, defaultSpace) == space) {
        return false;
    }
    m_viewToSpace.insert(key, space);
    return true;
}

bool WindowWorkspaceBinding::contains(const QString &viewId) const
{
    return m_viewToSpace.contains(viewId.trimmed());
}

bool WindowWorkspaceBinding::remove(const QString &viewId)
{
    return m_viewToSpace.remove(viewId.trimmed()) > 0;
}

void WindowWorkspaceBinding::remap(const QHash<int, int> &spaceRemap)
{
    if (spaceRemap.isEmpty()) {
        return;
    }
    for (auto it = m_viewToSpace.begin(); it != m_viewToSpace.end(); ++it) {
        const int current = it.value();
        if (spaceRemap.contains(current)) {
            it.value() = spaceRemap.value(current);
        }
    }
}

QList<QString> WindowWorkspaceBinding::viewIds() const
{
    return m_viewToSpace.keys();
}

QVariantMap WindowWorkspaceBinding::snapshot() const
{
    QVariantMap out;
    for (auto it = m_viewToSpace.constBegin(); it != m_viewToSpace.constEnd(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

int WindowWorkspaceBinding::size() const
{
    return m_viewToSpace.size();
}

int WindowWorkspaceBinding::maxAssignedSpace() const
{
    int maxSpace = 0;
    for (auto it = m_viewToSpace.constBegin(); it != m_viewToSpace.constEnd(); ++it) {
        maxSpace = qMax(maxSpace, it.value());
    }
    return maxSpace;
}

const QHash<QString, int> &WindowWorkspaceBinding::raw() const
{
    return m_viewToSpace;
}

void WindowWorkspaceBinding::clear()
{
    m_viewToSpace.clear();
}

void WindowWorkspaceBinding::load(const QHash<QString, int> &map)
{
    m_viewToSpace = map;
}

SpacesManager::SpacesManager(QObject *parent)
    : QObject(parent)
    , m_settings()
{
    load();
}

int SpacesManager::activeSpace() const
{
    return m_activeSpace;
}

int SpacesManager::spaceCount() const
{
    return m_spaceCount;
}

void SpacesManager::setActiveSpace(int space)
{
    const int next = clampSpace(space);
    if (next == m_activeSpace) {
        return;
    }
    m_activeSpace = next;
    m_settings.setValue(QString::fromLatin1(kSpaceActiveKey), m_activeSpace);
    emit activeSpaceChanged();
    emitWorkspacesChanged();
}

void SpacesManager::setSpaceCount(int count)
{
    const int next = qBound(kMinSpaces, count, kMaxSpaces);
    if (next == m_spaceCount) {
        return;
    }
    m_spaceCount = next;
    if (m_activeSpace > m_spaceCount) {
        m_activeSpace = m_spaceCount;
        emit activeSpaceChanged();
    }

    bool changed = false;
    QHash<QString, int> patched = m_binding.raw();
    for (auto it = patched.begin(); it != patched.end(); ++it) {
        const int clamped = clampSpace(it.value());
        if (clamped != it.value()) {
            it.value() = clamped;
            changed = true;
        }
    }
    m_binding.load(patched);
    changed = normalizeDynamicSpaces() || changed;

    m_settings.setValue(QString::fromLatin1(kSpaceCountKey), m_spaceCount);
    save();
    emit spaceCountChanged();
    if (changed) {
        emit assignmentsChanged();
    }
    emitWorkspacesChanged();
}

int SpacesManager::windowSpace(const QString &viewId) const
{
    const int stored = m_binding.spaceFor(viewId, m_activeSpace);
    return clampSpace(stored);
}

bool SpacesManager::windowBelongsToActive(const QString &viewId) const
{
    return windowSpace(viewId) == m_activeSpace;
}

void SpacesManager::assignWindowToSpace(const QString &viewId, int space)
{
    const QString key = viewId.trimmed();
    if (key.isEmpty()) {
        return;
    }
    const int fromSpace = windowSpace(key);
    const int next = clampSpace(space);
    const bool assignmentChanged = m_binding.assign(key, next, m_activeSpace);
    const bool normalized = normalizeDynamicSpaces();
    if (!assignmentChanged && !normalized) {
        return;
    }
    save();
    emit assignmentsChanged();
    if (normalized) {
        emit spaceCountChanged();
    }
    const int toSpace = windowSpace(key);
    emit windowWorkspaceChanged(key, toSpace);
    if (fromSpace != toSpace) {
        emit windowWorkspaceMoved(key, fromSpace, toSpace);
    }
    emitWorkspacesChanged();
}

void SpacesManager::cycleWindowSpace(const QString &viewId, int delta)
{
    const QString key = viewId.trimmed();
    if (key.isEmpty() || delta == 0) {
        return;
    }
    int current = windowSpace(key);
    int next = current + delta;
    while (next < 1) {
        next += m_spaceCount;
    }
    while (next > m_spaceCount) {
        next -= m_spaceCount;
    }
    assignWindowToSpace(key, next);
}

void SpacesManager::clearMissingAssignments(const QVariantList &activeViewIds)
{
    QSet<QString> active;
    active.reserve(activeViewIds.size());
    for (const QVariant &value : activeViewIds) {
        const QString viewId = value.toString().trimmed();
        if (!viewId.isEmpty()) {
            active.insert(viewId);
        }
    }

    bool changed = false;
    for (const QString &viewId : active) {
        changed = m_binding.assign(viewId, m_activeSpace, m_activeSpace) || changed;
    }

    const QList<QString> known = m_binding.viewIds();
    for (const QString &viewId : known) {
        if (!active.contains(viewId)) {
            changed = m_binding.remove(viewId) || changed;
        }
    }
    const bool normalized = normalizeDynamicSpaces();
    changed = changed || normalized;
    if (!changed) {
        return;
    }
    save();
    if (normalized) {
        emit spaceCountChanged();
    }
    emit assignmentsChanged();
    emitWorkspacesChanged();
}

QVariantList SpacesManager::spaces() const
{
    QVariantList out;
    out.reserve(m_spaceCount);
    for (int i = 1; i <= m_spaceCount; ++i) {
        out.push_back(i);
    }
    return out;
}

QVariantList SpacesManager::workspaceModel() const
{
    QVariantList out;
    out.reserve(m_spaceCount);
    const QHash<int, int> occupancy = occupancyBySpace();
    for (int i = 1; i <= m_spaceCount; ++i) {
        Workspace w;
        w.id = i;
        w.isActive = (i == m_activeSpace);
        w.windowCount = occupancy.value(i, 0);
        w.isEmpty = (w.windowCount <= 0);
        w.isTrailingEmpty = (i == m_spaceCount && w.isEmpty);
        QVariantMap row;
        row.insert(QStringLiteral("id"), w.id);
        row.insert(QStringLiteral("index"), w.id);
        row.insert(QStringLiteral("isActive"), w.isActive);
        row.insert(QStringLiteral("isEmpty"), w.isEmpty);
        row.insert(QStringLiteral("isTrailingEmpty"), w.isTrailingEmpty);
        row.insert(QStringLiteral("windowCount"), w.windowCount);
        out.push_back(row);
    }
    return out;
}

QVariantMap SpacesManager::assignmentSnapshot() const
{
    return m_binding.snapshot();
}

bool SpacesManager::hasTrailingEmptyWorkspace() const
{
    if (m_spaceCount <= 0) {
        return false;
    }
    const QHash<int, int> occupancy = occupancyBySpace();
    return occupancy.value(m_spaceCount, 0) == 0;
}

bool SpacesManager::isActiveSpaceValid() const
{
    return m_activeSpace >= kMinSpaces && m_activeSpace <= qMax(kMinSpaces, m_spaceCount);
}

bool SpacesManager::enforceInvariants()
{
    const bool changed = normalizeDynamicSpaces();
    if (!changed) {
        return false;
    }
    save();
    emit spaceCountChanged();
    emit assignmentsChanged();
    emitWorkspacesChanged();
    return true;
}

QVariantMap SpacesManager::invariantState() const
{
    const QHash<int, int> occupancy = occupancyBySpace();
    bool hasEmptyNonLast = false;
    for (int i = 1; i < m_spaceCount; ++i) {
        if (occupancy.value(i, 0) == 0) {
            hasEmptyNonLast = true;
            break;
        }
    }

    QVariantMap out;
    out.insert(QStringLiteral("activeSpace"), m_activeSpace);
    out.insert(QStringLiteral("spaceCount"), m_spaceCount);
    out.insert(QStringLiteral("hasTrailingEmptyWorkspace"), hasTrailingEmptyWorkspace());
    out.insert(QStringLiteral("hasEmptyNonLastWorkspace"), hasEmptyNonLast);
    out.insert(QStringLiteral("validActiveSpace"), isActiveSpaceValid());
    out.insert(QStringLiteral("assignments"), assignmentSnapshot());
    return out;
}

void SpacesManager::load()
{
    m_spaceCount = qBound(kMinSpaces,
                          m_settings.value(QString::fromLatin1(kSpaceCountKey), 4).toInt(),
                          kMaxSpaces);
    m_activeSpace = qBound(kMinSpaces,
                           m_settings.value(QString::fromLatin1(kSpaceActiveKey), 1).toInt(),
                           m_spaceCount);
    m_binding.clear();

    const QByteArray raw = m_settings.value(QString::fromLatin1(kAssignmentsKey)).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        normalizeDynamicSpaces();
        return;
    }
    const QJsonObject obj = doc.object();
    QHash<QString, int> loaded;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString key = it.key().trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const int space = clampSpace(it.value().toInt(m_activeSpace));
        loaded.insert(key, space);
    }
    m_binding.load(loaded);
    normalizeDynamicSpaces();
}

void SpacesManager::save()
{
    QJsonObject obj;
    for (auto it = m_binding.raw().constBegin(); it != m_binding.raw().constEnd(); ++it) {
        obj.insert(it.key(), it.value());
    }
    m_settings.setValue(QString::fromLatin1(kAssignmentsKey), QJsonDocument(obj).toJson(QJsonDocument::Compact));
    m_settings.setValue(QString::fromLatin1(kSpaceCountKey), m_spaceCount);
    m_settings.setValue(QString::fromLatin1(kSpaceActiveKey), m_activeSpace);
}

int SpacesManager::clampSpace(int space) const
{
    return qBound(kMinSpaces, space, m_spaceCount);
}

bool SpacesManager::normalizeDynamicSpaces()
{
    // Workspace invariant strategy:
    // 1) compress occupied spaces so there are no empty non-last spaces,
    // 2) keep exactly one trailing empty workspace when possible.
    const QHash<QString, int> raw = m_binding.raw();
    QSet<int> used;
    used.reserve(raw.size());
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        used.insert(qBound(kMinSpaces, it.value(), m_spaceCount));
    }

    if (used.isEmpty()) {
        bool changed = false;
        if (m_spaceCount != 1) {
            m_spaceCount = 1;
            changed = true;
        }
        if (m_activeSpace != 1) {
            m_activeSpace = 1;
            emit activeSpaceChanged();
            changed = true;
        }
        return changed;
    }

    QList<int> ordered = used.values();
    std::sort(ordered.begin(), ordered.end());

    QHash<int, int> remap;
    remap.reserve(ordered.size());
    int next = 1;
    for (int oldSpace : ordered) {
        remap.insert(oldSpace, next++);
    }

    bool changed = false;
    m_binding.remap(remap);
    const int oldCount = m_spaceCount;
    const int desiredCount = qBound(kMinSpaces, ordered.size() + 1, kMaxSpaces);
    if (desiredCount != m_spaceCount) {
        m_spaceCount = desiredCount;
        changed = true;
    }

    if (remap.contains(m_activeSpace)) {
        const int remappedActive = remap.value(m_activeSpace);
        if (remappedActive != m_activeSpace) {
            m_activeSpace = remappedActive;
            emit activeSpaceChanged();
            changed = true;
        }
    } else if (m_activeSpace == oldCount) {
        if (m_activeSpace != m_spaceCount) {
            m_activeSpace = m_spaceCount;
            emit activeSpaceChanged();
            changed = true;
        }
    } else {
        const int fallbackActive = qBound(kMinSpaces, m_activeSpace, qMax(kMinSpaces, m_spaceCount - 1));
        if (fallbackActive != m_activeSpace) {
            m_activeSpace = fallbackActive;
            emit activeSpaceChanged();
            changed = true;
        }
    }

    return ensureTrailingEmptyWorkspace() || changed;
}

bool SpacesManager::ensureTrailingEmptyWorkspace()
{
    if (m_binding.size() <= 0) {
        return false;
    }
    const int highestUsed = m_binding.maxAssignedSpace();
    const int desiredCount = qBound(kMinSpaces, highestUsed + 1, kMaxSpaces);
    if (desiredCount == m_spaceCount) {
        return false;
    }
    m_spaceCount = desiredCount;
    if (m_activeSpace > m_spaceCount) {
        m_activeSpace = m_spaceCount;
        emit activeSpaceChanged();
    }
    return true;
}

SpacesManager::Workspace SpacesManager::workspaceAt(int id) const
{
    Workspace w;
    w.id = id;
    w.isActive = (id == m_activeSpace);
    const QHash<int, int> occupancy = occupancyBySpace();
    w.windowCount = occupancy.value(id, 0);
    w.isEmpty = (w.windowCount <= 0);
    w.isTrailingEmpty = (id == m_spaceCount && w.isEmpty);
    return w;
}

QHash<int, int> SpacesManager::occupancyBySpace() const
{
    QHash<int, int> occupancy;
    for (int i = 1; i <= m_spaceCount; ++i) {
        occupancy.insert(i, 0);
    }
    for (auto it = m_binding.raw().constBegin(); it != m_binding.raw().constEnd(); ++it) {
        const int space = qBound(kMinSpaces, it.value(), m_spaceCount);
        occupancy[space] = occupancy.value(space, 0) + 1;
    }
    return occupancy;
}

void SpacesManager::emitWorkspacesChanged()
{
    emit workspacesChanged();
}
