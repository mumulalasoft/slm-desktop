#include "SlmProtectedPackagesJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmProtectedPackagesJobFactory, registerPlugin<SlmProtectedPackagesJob>();)

namespace {

constexpr const char *kSchemaVersion = "1.0";
constexpr const char *kInstallerVersion = "1.0.0";
constexpr const char *kTargetPath = "usr/share/slm/protected-packages.json";
constexpr const char *kRecoveryPath = "var/lib/slm/recovery-metadata/protected-packages.json";

// §6 curated category list. The §12 open question on hardcode-vs-template
// ownership remains open; until then this is the source of truth.
struct PackageEntry
{
    const char *name;
    const char *minVersion;  // nullptr means "no minimum"
};

struct Category
{
    const char *key;
    const char *description;
    QList<PackageEntry> packages;
};

const QList<Category> kCategories = {
    { "bootloader", "Boot and EFI components", {
        { "systemd-boot-unsigned", "255.0" },
        { "efibootmgr", "18" },
    }},
    { "kernel", "Linux kernel and modules", {
        { "linux-image-generic", nullptr },
        { "linux-modules-extra-generic", nullptr },
    }},
    { "filesystem", "Root filesystem tools", {
        { "btrfs-progs", "6.0" },
    }},
    { "display", "Wayland and GPU stack", {
        { "libwayland-client0", nullptr },
        { "mesa-vulkan-drivers", nullptr },
    }},
    { "shell", "SLM Desktop Shell core", {
        { "slm-desktop", nullptr },
        { "slm-greeter", nullptr },
        { "greetd", nullptr },
    }},
};

QString resolveRootMountPoint(Calamares::GlobalStorage *gs)
{
    const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
    return fromGs.isEmpty() ? QStringLiteral("/mnt/slm-install-root") : fromGs;
}

QJsonObject buildSchema()
{
    QJsonObject categories;
    for (const Category &cat : kCategories) {
        QJsonArray packages;
        for (const PackageEntry &pkg : cat.packages) {
            QJsonObject entry;
            entry.insert(QStringLiteral("name"), QLatin1String(pkg.name));
            if (pkg.minVersion) {
                entry.insert(QStringLiteral("min_version"), QLatin1String(pkg.minVersion));
            } else {
                entry.insert(QStringLiteral("min_version"), QJsonValue::Null);
            }
            packages.append(entry);
        }
        QJsonObject category;
        category.insert(QStringLiteral("description"), QLatin1String(cat.description));
        category.insert(QStringLiteral("packages"), packages);
        categories.insert(QLatin1String(cat.key), category);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("schema_version"), QLatin1String(kSchemaVersion));
    doc.insert(QStringLiteral("generated_at"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    doc.insert(QStringLiteral("installer_version"), QLatin1String(kInstallerVersion));
    doc.insert(QStringLiteral("categories"), categories);
    return doc;
}

bool writeJsonFile(const QString &path, const QByteArray &content, QString *err)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        *err = QStringLiteral("mkpath failed for %1").arg(info.absolutePath());
        return false;
    }
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *err = QStringLiteral("open(%1) failed: %2").arg(path, out.errorString());
        return false;
    }
    if (out.write(content) != content.size()) {
        *err = QStringLiteral("short write to %1: %2").arg(path, out.errorString());
        out.close();
        return false;
    }
    out.close();
    return true;
}

} // namespace

SlmProtectedPackagesJob::SlmProtectedPackagesJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmProtectedPackagesJob::~SlmProtectedPackagesJob() = default;

QString SlmProtectedPackagesJob::prettyName() const
{
    return tr("Record protected packages");
}

Calamares::JobResult SlmProtectedPackagesJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("PROT_001: GlobalStorage missing"));
    }

    const QString rootMount = resolveRootMountPoint(gs);
    const QString primaryPath = rootMount + QLatin1Char('/') + QLatin1String(kTargetPath);
    const QString recoveryPath = rootMount + QLatin1Char('/') + QLatin1String(kRecoveryPath);

    const QJsonObject schema = buildSchema();
    const QByteArray content = QJsonDocument(schema).toJson(QJsonDocument::Indented);

    int packageCount = 0;
    for (const Category &cat : kCategories) {
        packageCount += cat.packages.size();
    }

    gs->insert(QStringLiteral("slm.protected.path"), primaryPath);
    gs->insert(QStringLiteral("slm.protected.recovery_path"), recoveryPath);
    gs->insert(QStringLiteral("slm.protected.package_count"), packageCount);
    gs->insert(QStringLiteral("slm.protected.preview"), QString::fromUtf8(content));

    cDebug() << "slm-protected-packages:"
             << "primary=" << primaryPath
             << "recovery=" << recoveryPath
             << "package_count=" << packageCount
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-protected-packages: DRY RUN — would write:";
        cDebug() << "  " << primaryPath << "(" << content.size() << "bytes)";
        cDebug() << "  " << recoveryPath << "(" << content.size() << "bytes)";
        gs->insert(QStringLiteral("slm.protected.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    QString err;
    if (!writeJsonFile(primaryPath, content, &err)) {
        return Calamares::JobResult::error(
            tr("Unable to write protected packages manifest."),
            QStringLiteral("PROT_002: %1").arg(err));
    }
    if (!writeJsonFile(recoveryPath, content, &err)) {
        return Calamares::JobResult::error(
            tr("Unable to write recovery copy of protected packages manifest."),
            QStringLiteral("PROT_003: %1").arg(err));
    }

    return Calamares::JobResult::ok();
}

void SlmProtectedPackagesJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
