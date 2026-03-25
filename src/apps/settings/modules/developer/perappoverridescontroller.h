#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class EnvServiceClient;

// PerAppOverridesController — drives the "Per-App Overrides" tab in Settings.
//
// Provides a list of apps that have overrides, the variables for the currently
// selected app, and methods to add/remove per-app variables.
//
class PerAppOverridesController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList  appList       READ appList       NOTIFY appListChanged)
    Q_PROPERTY(QString      selectedAppId READ selectedAppId WRITE setSelectedAppId
                                          NOTIFY selectedAppIdChanged)
    Q_PROPERTY(QVariantList appVars       READ appVars       NOTIFY appVarsChanged)
    Q_PROPERTY(QString      lastError     READ lastError     NOTIFY lastErrorChanged)

public:
    explicit PerAppOverridesController(EnvServiceClient *client, QObject *parent = nullptr);

    QStringList  appList()       const;
    QString      selectedAppId() const;
    QVariantList appVars()       const;
    QString      lastError()     const;

    void setSelectedAppId(const QString &appId);

    Q_INVOKABLE bool addVar(const QString &key, const QString &value,
                            const QString &mergeMode = QStringLiteral("replace"));
    Q_INVOKABLE bool removeVar(const QString &key);
    Q_INVOKABLE void refresh();

signals:
    void appListChanged();
    void selectedAppIdChanged();
    void appVarsChanged();
    void lastErrorChanged();

private:
    void reloadAppList();
    void reloadAppVars();

    EnvServiceClient *m_client;
    QStringList       m_appList;
    QString           m_selectedAppId;
    QVariantList      m_appVars;
    QString           m_lastError;
};
