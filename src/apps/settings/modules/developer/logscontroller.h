#pragma once

#include "logserviceclient.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// LogsController — QML-facing controller for the Logs & Events page.
//
// Wraps LogServiceClient, manages the live model and filter state.
// Exposed to QML as the "LogsController" context property.
//
class LogsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool        serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(QVariantList entries         READ entries          NOTIFY entriesChanged)
    Q_PROPERTY(QStringList  sources         READ sources          NOTIFY sourcesChanged)
    Q_PROPERTY(QString      filterSource    READ filterSource     WRITE  setFilterSource
               NOTIFY filterSourceChanged)
    Q_PROPERTY(QString      filterLevel     READ filterLevel      WRITE  setFilterLevel
               NOTIFY filterLevelChanged)
    Q_PROPERTY(bool         streaming       READ streaming        NOTIFY streamingChanged)

public:
    explicit LogsController(LogServiceClient *client, QObject *parent = nullptr);
    ~LogsController() override;

    bool         serviceAvailable() const;
    QVariantList entries()          const { return m_entries;      }
    QStringList  sources()          const { return m_sources;      }
    QString      filterSource()     const { return m_filterSource; }
    QString      filterLevel()      const { return m_filterLevel;  }
    bool         streaming()        const { return m_subId != 0;   }

    void setFilterSource(const QString &source);
    void setFilterLevel(const QString &level);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startStream();
    Q_INVOKABLE void stopStream();
    Q_INVOKABLE QString exportBundle();

signals:
    void serviceAvailableChanged();
    void entriesChanged();
    void sourcesChanged();
    void filterSourceChanged();
    void filterLevelChanged();
    void streamingChanged();

private slots:
    void onServiceAvailableChanged();
    void onLogEntry(quint32 subId, const QVariantMap &entry);

private:
    void reloadSources();

    LogServiceClient *m_client       = nullptr;
    QVariantList      m_entries;
    QStringList       m_sources;
    QString           m_filterSource;
    QString           m_filterLevel;
    quint32           m_subId        = 0;
};
