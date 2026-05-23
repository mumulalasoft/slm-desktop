#pragma once

#include "SlmInstallerDiskModel.h"

#include <QObject>
#include <QString>

namespace Slm::Installer {

// The QML scene's `config` context property. Owns the disk model and the
// commit slot that writes the user's choice to Calamares GlobalStorage.
// Kept thin on purpose — the ViewStep does the Calamares-flavoured glue,
// the screen sees only this object.
class DiskSelectConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Slm::Installer::SlmInstallerDiskModel *diskModel READ diskModel CONSTANT)
    Q_PROPERTY(QString selectedPath READ selectedPath WRITE setSelectedPath
                   NOTIFY selectedPathChanged)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)

public:
    explicit DiskSelectConfig(QObject *parent = nullptr);

    SlmInstallerDiskModel *diskModel() { return &m_diskModel; }

    QString selectedPath() const { return m_selectedPath; }
    void setSelectedPath(const QString &path);

    bool isRefreshing() const { return m_isRefreshing; }

public slots:
    // Called from QML when the user confirms a disk. Writes
    // slm.target.disk to Calamares GlobalStorage and emits commitRequested
    // so the ViewStep can advance.
    void commit(const QString &path);

    // Called from QML when the user clicks Refresh on the empty state.
    void refresh();

    // Called from QML when the user clicks the secondary Quit action.
    void quit();

signals:
    void selectedPathChanged();
    void isRefreshingChanged();
    void commitRequested(const QString &path);
    void quitRequested();

private:
    void setIsRefreshing(bool refreshing);

    SlmInstallerDiskModel m_diskModel;
    QString m_selectedPath;
    bool m_isRefreshing = false;
};

} // namespace Slm::Installer
