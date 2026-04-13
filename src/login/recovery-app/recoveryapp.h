#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace Slm::Login {

class RecoveryApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString  recoveryReason    READ recoveryReason    CONSTANT)
    Q_PROPERTY(QString  lastBootStatus    READ lastBootStatus    CONSTANT)
    Q_PROPERTY(int      crashCount        READ crashCount        CONSTANT)
    Q_PROPERTY(bool     hasPreviousConfig READ hasPreviousConfig CONSTANT)
    Q_PROPERTY(QStringList availableSnapshots READ availableSnapshots CONSTANT)

public:
    explicit RecoveryApp(QObject *parent = nullptr);

    QString    recoveryReason()    const;
    QString    lastBootStatus()    const;
    int        crashCount()        const;
    bool       hasPreviousConfig() const;
    QStringList availableSnapshots() const;

    Q_INVOKABLE bool resetToSafeDefaults();
    Q_INVOKABLE bool rollbackToPrevious();
    Q_INVOKABLE bool restoreSnapshot(const QString &snapshotId);
    Q_INVOKABLE bool factoryReset();
    Q_INVOKABLE bool restartDesktop();
    Q_INVOKABLE bool disableExtensions();
    Q_INVOKABLE bool resetGraphicsStack();
    Q_INVOKABLE bool openTerminal();
    Q_INVOKABLE bool networkRepair();
    Q_INVOKABLE QVariantMap previewSnapshotDiff(const QString &snapshotId) const;
    Q_INVOKABLE QString logSummary() const;
    Q_INVOKABLE QVariantMap daemonHealthSnapshot() const;
    Q_INVOKABLE void    exitToDesktop();

private:
    bool writeFlagFile(const QString &name, const QByteArray &value = QByteArray("1\n")) const;
    QString     m_recoveryReason;
    QString     m_lastBootStatus;
    int         m_crashCount      = 0;
    bool        m_hasPreviousConfig = false;
    QStringList m_snapshots;
};

} // namespace Slm::Login
