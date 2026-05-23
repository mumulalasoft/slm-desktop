#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantMap>

#include <vector>

namespace Slm::Installer {

// Enumerates candidate install targets from /sys/block and exposes them
// as a QAbstractListModel for consumption by DiskSelectionScreen.qml.
//
// The model is intentionally synchronous and stateless beyond the row
// vector — refresh() rescans from scratch. SMART health is stubbed
// "healthy" for now; an async health-probe worker will populate it later
// (see project-slm-installer memory).
//
// Role names match the keys the QML delegate expects: `path`, `name`,
// `size`, `kind`, `health`, `rootBytes`, `efiBytes`, `recoveryBytes`,
// `removable`, `sizeBytes`.
class SlmInstallerDiskModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        SizeRole,
        KindRole,
        HealthRole,
        RootBytesRole,
        EfiBytesRole,
        RecoveryBytesRole,
        RemovableRole,
        SizeBytesRole,
    };
    Q_ENUM(Roles)

    explicit SlmInstallerDiskModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Rescan /sys/block. Emits modelReset and kicks off the async SMART probes.
    Q_INVOKABLE void refresh();

    // Convenience for QML — returns the row as a {role: value} map so
    // DiskSelectionScreen can do `model.get(0).health`.
    Q_INVOKABLE QVariantMap get(int row) const;

    // Test seam: rescan from a custom sysfs-block root. Used by
    // disk-model unit tests to point at a fixture directory.
    void enumerateFrom(const QString &sysfsBlockRoot);

    // Disable async health probes for unit tests / no-smartctl harnesses.
    void setHealthProbesEnabled(bool enabled) { m_healthProbesEnabled = enabled; }

signals:
    void countChanged();

private slots:
    // Stale-safe sink for async health probes. Drops results whose
    // generation epoch doesn't match the current model state (i.e. the
    // model was reset between dispatch and completion).
    void applyHealthProbeResult(int row, qint64 generation, const QString &health);

private:
    void startHealthProbes();
    struct Disk
    {
        QString path;          // "/dev/nvme0n1"
        QString name;          // "Samsung SSD 870 — 500 GB"
        QString sizeDisplay;   // "500 GB"
        QString kind;          // "NVMe" | "SATA SSD" | "HDD" | "USB" | "MMC" | "Disk"
        QString health;        // "healthy" | "warning" | "failed"
        qint64  sizeBytes = 0;
        qint64  rootBytes = 0;
        qint64  efiBytes = 0;
        qint64  recoveryBytes = 0;
        bool    removable = false;
    };

    std::vector<Disk> m_disks;

    // Epoch — bumped on every enumerateFrom() so workers spawned by a
    // previous enumeration know to discard their results.
    qint64 m_generation = 0;
    bool m_healthProbesEnabled = true;
};

} // namespace Slm::Installer
