#pragma once

#include "logtypes.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// LogService — in-process log aggregator for slm-loggerd.
//
// Sources:
//   1. journald — via QProcess running `journalctl --follow --output=json`
//      filtered to slm-* units.
//   2. Direct submissions — other D-Bus components call Submit() to push
//      a structured entry.
//
// Storage: in-memory ring buffer (kMaxEntries most recent entries).
// Subscriptions: callers register a filter and receive live LogEntry signals.
//
class LogService : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxEntries = 5000;

    explicit LogService(QObject *parent = nullptr);
    ~LogService() override;

    bool start();
    void stop();

    // ── Query ──────────────────────────────────────────────────────────────
    QVariantList getLogs(const QVariantMap &filter) const;
    QStringList  getSources() const;

    // ── Subscriptions ──────────────────────────────────────────────────────
    quint32 subscribe(const QString &source, const QString &level);
    void    unsubscribe(quint32 subscriptionId);

    // ── Export ─────────────────────────────────────────────────────────────
    QString exportBundle(const QVariantMap &filter) const;

    // ── Direct submission (from other D-Bus services) ──────────────────────
    void submit(const QString &source, const QString &level, const QString &message,
                const QVariantMap &fields = {});

    QString lastError() const { return m_lastError; }

signals:
    // Emitted for every new entry that matches a subscriber's filter.
    void logEntryForSubscription(quint32 subscriptionId, const QVariantMap &entry);

    // Emitted for every new entry regardless of subscriptions (for internal use).
    void newEntry(const LogEntry &entry);

private slots:
    void onJournalReadyRead();
    void onJournalError();

private:
    void appendEntry(LogEntry entry);
    void dispatchToSubscribers(const LogEntry &entry);
    bool matchesFilter(const LogEntry &entry, const QVariantMap &filter) const;

    struct Subscription {
        quint32 id     = 0;
        QString source;  // empty = all
        QString level;   // empty = all, otherwise minimum level name
    };

    QList<LogEntry>         m_buffer;
    quint64                 m_nextId   = 1;
    QProcess               *m_journal  = nullptr;
    QByteArray              m_lineBuffer;
    QList<Subscription>     m_subs;
    quint32                 m_nextSubId = 1;
    QString                 m_lastError;
};
