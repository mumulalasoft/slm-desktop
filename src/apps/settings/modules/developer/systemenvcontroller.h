#pragma once

#include "envserviceclient.h"

#include <QObject>
#include <QString>
#include <QVariantList>

class QProcess;

// SystemEnvController — QML-facing bridge for the system-scope section of
// Settings > Developer > Environment Variables.
//
// Provides:
//   • polkit authorization flow (requestAuth → pkcheck → authorized)
//   • read/write/delete of system-scope vars via EnvServiceClient
//
// Exposed to QML as the "SystemEnvController" context property.
// Gated in UI by the "env-system-scope" FeatureFlag.
//
class SystemEnvController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool         authorized  READ authorized  NOTIFY authorizedChanged)
    Q_PROPERTY(bool         authPending READ authPending NOTIFY authPendingChanged)
    Q_PROPERTY(QVariantList vars        READ vars        NOTIFY varsChanged)
    Q_PROPERTY(QString      lastError   READ lastError   NOTIFY lastErrorChanged)

public:
    explicit SystemEnvController(EnvServiceClient *client, QObject *parent = nullptr);

    bool         authorized()  const { return m_authorized;  }
    bool         authPending() const { return m_authPending; }
    QVariantList vars()        const { return m_vars;        }
    QString      lastError()   const { return m_lastError;   }

    // Launch pkcheck for org.slm.environment.write-system.
    Q_INVOKABLE void requestAuth();

    // Requires authorized() == true. Calls EnvServiceClient::writeSystemVar().
    Q_INVOKABLE bool writeVar(const QString &key, const QString &value,
                              const QString &comment, const QString &mergeMode,
                              bool enabled = true);

    // Requires authorized() == true.
    Q_INVOKABLE bool deleteVar(const QString &key);

    // Reload var list from the service.
    Q_INVOKABLE void reload();

signals:
    void authorizedChanged();
    void authPendingChanged();
    void varsChanged();
    void lastErrorChanged();

private:
    void setLastError(const QString &msg);
    void loadVars();

    EnvServiceClient *m_client;
    bool              m_authorized  = false;
    bool              m_authPending = false;
    QVariantList      m_vars;
    QString           m_lastError;
    QProcess         *m_pkcheckProc = nullptr;
};
