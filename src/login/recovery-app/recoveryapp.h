#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

namespace Slm::Login {

class RecoveryApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString  recoveryReason    READ recoveryReason    CONSTANT)
    Q_PROPERTY(QString  lastBootStatus    READ lastBootStatus    CONSTANT)
    Q_PROPERTY(int      crashCount        READ crashCount        CONSTANT)
    Q_PROPERTY(bool     hasPreviousConfig READ hasPreviousConfig CONSTANT)
    Q_PROPERTY(QStringList availableSnapshots READ availableSnapshots CONSTANT)
    Q_PROPERTY(QVariantList missingComponents READ missingComponents NOTIFY missingComponentsChanged)

public:
    explicit RecoveryApp(QObject *parent = nullptr);

    QString    recoveryReason()    const;
    QString    lastBootStatus()    const;
    int        crashCount()        const;
    bool       hasPreviousConfig() const;
    QStringList availableSnapshots() const;
    QVariantList missingComponents() const;

    Q_INVOKABLE bool resetToSafeDefaults();
    Q_INVOKABLE bool rollbackToPrevious();
    Q_INVOKABLE bool restoreSnapshot(const QString &snapshotId);
    Q_INVOKABLE bool factoryReset();
    Q_INVOKABLE QString logSummary() const;
    Q_INVOKABLE void    exitToDesktop();
    Q_INVOKABLE QVariantMap installMissingComponent(const QString &componentId);
    Q_INVOKABLE QVariantList refreshMissingComponents();

signals:
    void missingComponentsChanged();

private:
    QVariantList checkRequiredComponents() const;
    QString     m_recoveryReason;
    QString     m_lastBootStatus;
    int         m_crashCount      = 0;
    bool        m_hasPreviousConfig = false;
    QStringList m_snapshots;
    QVariantList m_missingComponents;
};

} // namespace Slm::Login
