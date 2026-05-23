#include "SlmInstallerDiskModel.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>

namespace Slm::Installer {

namespace {

constexpr qint64 kSectorBytes = 512;

// SI units — matches what storage vendors print on the label.
constexpr qint64 kKB = 1000LL;
constexpr qint64 kMB = kKB * 1000;
constexpr qint64 kGB = kMB * 1000;
constexpr qint64 kTB = kGB * 1000;

// §2 sizing — must stay in sync with slm-partition's constants.
constexpr qint64 kEspBytes = 1LL * 1024 * 1024 * 1024;       // 1 GiB
constexpr qint64 kRecoveryBytes = 8LL * 1024 * 1024 * 1024;  // 8 GiB

// §2 minimum total disk per slm-partition (ESP + Recovery + ~24 GiB root).
constexpr qint64 kMinDiskBytes = 32LL * 1024 * 1024 * 1024;

QString readSysfs(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLatin1(f.readAll()).trimmed();
}

bool shouldSkip(const QString &basename)
{
    static const QStringList kSkipPrefixes = {
        QStringLiteral("loop"), QStringLiteral("ram"),  QStringLiteral("dm-"),
        QStringLiteral("sr"),   QStringLiteral("zram"), QStringLiteral("md"),
    };
    for (const QString &p : kSkipPrefixes) {
        if (basename.startsWith(p)) {
            return true;
        }
    }
    return false;
}

QString formatSize(qint64 bytes)
{
    if (bytes >= kTB) {
        return QString::number(double(bytes) / double(kTB), 'f', 1)
               + QStringLiteral(" TB");
    }
    if (bytes >= kGB) {
        return QString::number(qRound(double(bytes) / double(kGB)))
               + QStringLiteral(" GB");
    }
    if (bytes >= kMB) {
        return QString::number(qRound(double(bytes) / double(kMB)))
               + QStringLiteral(" MB");
    }
    return QString::number(bytes) + QStringLiteral(" B");
}

QString classifyKind(const QString &basename, bool rotational, bool removable)
{
    if (basename.startsWith(QStringLiteral("nvme"))) {
        return QStringLiteral("NVMe");
    }
    if (basename.startsWith(QStringLiteral("mmcblk"))) {
        return QStringLiteral("MMC");
    }
    if (basename.startsWith(QStringLiteral("vd"))) {
        return QStringLiteral("Virtual");
    }
    if (removable) {
        // sd* + removable means a USB/Thunderbolt enclosure on modern Linux.
        // Distinguishing USB from eSATA from hot-pluggable SATA is messy and
        // adds little user value here — "USB" reads correctly for the common
        // case.
        return QStringLiteral("USB");
    }
    return rotational ? QStringLiteral("HDD") : QStringLiteral("SATA SSD");
}

QString deriveDisplayName(const QString &vendor, const QString &model,
                          const QString &sizeDisplay, const QString &basename)
{
    QString combined;
    const QString v = vendor.trimmed();
    const QString m = model.trimmed();
    if (!v.isEmpty() && v != QLatin1String("ATA")) {
        combined = v;
    }
    if (!m.isEmpty()) {
        if (!combined.isEmpty()) {
            combined += QLatin1Char(' ');
        }
        combined += m;
    }
    if (combined.isEmpty()) {
        combined = basename;
    }
    return combined + QStringLiteral(" — ") + sizeDisplay;
}

} // namespace

SlmInstallerDiskModel::SlmInstallerDiskModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();
}

int SlmInstallerDiskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_disks.size());
}

QVariant SlmInstallerDiskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_disks.size())) {
        return {};
    }
    const Disk &d = m_disks[size_t(index.row())];
    switch (role) {
    case PathRole:           return d.path;
    case NameRole:           return d.name;
    case SizeRole:           return d.sizeDisplay;
    case KindRole:           return d.kind;
    case HealthRole:         return d.health;
    case RootBytesRole:      return QVariant::fromValue(d.rootBytes);
    case EfiBytesRole:       return QVariant::fromValue(d.efiBytes);
    case RecoveryBytesRole:  return QVariant::fromValue(d.recoveryBytes);
    case RemovableRole:      return d.removable;
    case SizeBytesRole:      return QVariant::fromValue(d.sizeBytes);
    default:                 return {};
    }
}

QHash<int, QByteArray> SlmInstallerDiskModel::roleNames() const
{
    return {
        { PathRole,           "path" },
        { NameRole,           "name" },
        { SizeRole,           "size" },
        { KindRole,           "kind" },
        { HealthRole,         "health" },
        { RootBytesRole,      "rootBytes" },
        { EfiBytesRole,       "efiBytes" },
        { RecoveryBytesRole,  "recoveryBytes" },
        { RemovableRole,      "removable" },
        { SizeBytesRole,      "sizeBytes" },
    };
}

void SlmInstallerDiskModel::refresh()
{
    enumerateFrom(QStringLiteral("/sys/block"));
}

QVariantMap SlmInstallerDiskModel::get(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= static_cast<int>(m_disks.size())) {
        return out;
    }
    const auto roles = roleNames();
    const QModelIndex idx = index(row);
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        out.insert(QString::fromLatin1(it.value()), data(idx, it.key()));
    }
    return out;
}

void SlmInstallerDiskModel::enumerateFrom(const QString &sysfsBlockRoot)
{
    beginResetModel();
    m_disks.clear();

    QDir root(sysfsBlockRoot);
    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                     QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (shouldSkip(name)) {
            continue;
        }
        const QString basePath = entry.absoluteFilePath();

        const qint64 sectors = readSysfs(basePath + QStringLiteral("/size")).toLongLong();
        const qint64 sizeBytes = sectors * kSectorBytes;
        if (sizeBytes < kMinDiskBytes) {
            // Too small to host the SLM partition layout.
            continue;
        }

        const bool removable = readSysfs(basePath + QStringLiteral("/removable")) == QLatin1String("1");
        const bool rotational = readSysfs(basePath + QStringLiteral("/queue/rotational")) == QLatin1String("1");
        const QString vendor = readSysfs(basePath + QStringLiteral("/device/vendor"));
        const QString model = readSysfs(basePath + QStringLiteral("/device/model"));

        Disk d;
        d.path = QStringLiteral("/dev/") + name;
        d.sizeBytes = sizeBytes;
        d.sizeDisplay = formatSize(sizeBytes);
        d.kind = classifyKind(name, rotational, removable);
        d.name = deriveDisplayName(vendor, model, d.sizeDisplay, name);
        d.health = QStringLiteral("healthy");  // populated later by async probe
        d.removable = removable;
        d.efiBytes = kEspBytes;
        d.recoveryBytes = kRecoveryBytes;
        d.rootBytes = sizeBytes - kEspBytes - kRecoveryBytes;

        m_disks.push_back(std::move(d));
    }

    endResetModel();
    emit countChanged();
}

} // namespace Slm::Installer
