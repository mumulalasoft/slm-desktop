#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Slm::Installer {

// The QML scene's `config` context property for the §2.5 Summary screen.
// Reads everything the screen needs from Calamares GlobalStorage on
// reload(), exposes it as bindable Q_PROPERTYs, and writes
// slm.target.confirmed=true when the user commits.
//
// Reading from GS is one-shot per onActivate() — the screen does not
// observe GS changes during its lifetime because the prior view steps
// have already settled by the time we land here.
class SummaryConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString diskName READ diskName NOTIFY changed)
    Q_PROPERTY(QString diskPath READ diskPath NOTIFY changed)
    Q_PROPERTY(int espSizeMb READ espSizeMb NOTIFY changed)
    Q_PROPERTY(int recoverySizeMb READ recoverySizeMb NOTIFY changed)
    Q_PROPERTY(QStringList subvolumes READ subvolumes NOTIFY changed)
    Q_PROPERTY(bool factorySnapshotEnabled READ factorySnapshotEnabled NOTIFY changed)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QString timezone READ timezone NOTIFY changed)

public:
    explicit SummaryConfig(QObject *parent = nullptr);

    QString     diskName() const               { return m_diskName; }
    QString     diskPath() const               { return m_diskPath; }
    int         espSizeMb() const              { return m_espSizeMb; }
    int         recoverySizeMb() const         { return m_recoverySizeMb; }
    QStringList subvolumes() const             { return m_subvolumes; }
    bool        factorySnapshotEnabled() const { return m_factorySnapshotEnabled; }
    QStringList warnings() const               { return m_warnings; }
    QString     language() const               { return m_language; }
    QString     timezone() const               { return m_timezone; }

    // Refresh all properties from Calamares GlobalStorage. Called by the
    // ViewStep's onActivate so values reflect the latest pipeline state
    // whenever the user lands on (or returns to) the summary screen.
    void reload();

public slots:
    // Writes slm.target.confirmed=true to GlobalStorage and emits
    // commitRequested so the ViewStep can advance Calamares forward.
    void commit();

    // Emits backRequested so the ViewStep can pop back to disk-select.
    void back();

signals:
    void changed();
    void commitRequested();
    void backRequested();

private:
    QString     m_diskName;
    QString     m_diskPath;
    int         m_espSizeMb = 1024;
    int         m_recoverySizeMb = 8192;
    QStringList m_subvolumes;
    bool        m_factorySnapshotEnabled = true;
    QStringList m_warnings;
    QString     m_language;
    QString     m_timezone;
};

} // namespace Slm::Installer
