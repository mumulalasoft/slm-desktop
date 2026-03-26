#pragma once

#include "logservice.h"

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// LogDbusInterface — hand-written D-Bus adaptor for org.slm.Logger1.
//
// Service name : org.slm.Logger1
// Object path  : /org/slm/Logger
//
class LogDbusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Logger1")

public:
    explicit LogDbusInterface(LogService *service, QObject *parent = nullptr);

    bool registerOn(QDBusConnection &bus);

public slots:
    // Returns log entries matching filter map.
    // filter keys: source (string), level (string), sinceId (uint64), limit (int)
    QVariantList GetLogs(const QVariantMap &filter);

    // Returns list of known source names.
    QStringList GetSources();

    // Subscribe to live log stream. Returns subscription ID.
    // filter keys: source (string), level (string)
    quint32 Subscribe(const QVariantMap &filter);

    // Cancel a subscription.
    void Unsubscribe(quint32 subscriptionId);

    // Export log bundle to a temp file. Returns file path.
    QString ExportBundle(const QVariantMap &filter);

    // Direct log submission (for shell components that want structured logging).
    void Submit(const QString &source, const QString &level,
                const QString &message, const QVariantMap &fields);

    // Service metadata.
    QString ServiceVersion();

signals:
    void LogEntry(quint32 subscriptionId, const QVariantMap &entry);

private:
    LogService *m_service;
};
