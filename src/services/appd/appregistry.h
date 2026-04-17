#pragma once

#include "appentry.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace Slm::Appd {

// AppRegistry — authoritative in-memory store for AppEntry objects.
//
// Thread-safety: single-threaded (Qt main thread). All mutations
// must happen on the object's thread.
class AppRegistry : public QObject
{
    Q_OBJECT
public:
    explicit AppRegistry(QObject *parent = nullptr);

    // ── Query ────────────────────────────────────────────────────────────────

    bool contains(const QString &appId) const;
    AppEntry *find(const QString &appId);
    const AppEntry *find(const QString &appId) const;

    // Find by PID (returns nullptr if not found).
    AppEntry *findByPid(qint64 pid);

    // Find by window id.
    AppEntry *findByWindowId(const QString &windowId);

    QList<AppEntry *> all();
    QList<const AppEntry *> all() const;

    // ── Mutation ─────────────────────────────────────────────────────────────

    // Insert or update — takes ownership of data, keyed by appId.
    // Returns pointer to stored entry.
    AppEntry *upsert(AppEntry entry);

    // Remove permanently (e.g. after TERMINATED).
    void remove(const QString &appId);

    // Convenience: attach a PID to an existing entry.
    bool attachPid(const QString &appId, qint64 pid);

    // Detach a PID; returns true if the entry still has other PIDs.
    bool detachPid(qint64 pid);

    // Attach/detach window info.
    bool attachWindow(const QString &appId, const WindowInfo &window);
    bool detachWindow(const QString &appId, const QString &windowId);
    bool updateWindow(const QString &appId, const WindowInfo &window);

signals:
    void entryAdded(const QString &appId);
    void entryRemoved(const QString &appId);
    void entryStateChanged(const QString &appId, AppState newState);
    void entryWindowsChanged(const QString &appId);
    void entryFocusChanged(const QString &appId, bool focused);
    void entryUpdated(const QString &appId);

private:
    QHash<QString, AppEntry> m_entries;
};

} // namespace Slm::Appd
