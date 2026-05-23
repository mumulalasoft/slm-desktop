#include "SlmUefiCheckJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVariantMap>

#include <optional>

#include <unistd.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmUefiCheckJobFactory, registerPlugin<SlmUefiCheckJob>();)

namespace {

constexpr qint64 kRamBlockBytes = 512LL * 1024 * 1024;
constexpr qint64 kRamWarnBytes = 2LL * 1024 * 1024 * 1024;

// EFI Global Variable GUID per UEFI Specification (Appendix A).
constexpr const char *kSecureBootVarName = "SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c";

bool pathAccess(const QString &path, int mode)
{
    return ::access(path.toLocal8Bit().constData(), mode) == 0;
}

std::optional<qint64> readMemTotalBytes(const QString &meminfoPath)
{
    QFile f(meminfoPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (!line.startsWith("MemTotal:")) {
            continue;
        }
        // Format: "MemTotal:       16264452 kB"
        const QList<QByteArray> parts = line.split(' ');
        for (const QByteArray &part : parts) {
            bool ok = false;
            const qulonglong kb = part.trimmed().toULongLong(&ok);
            if (ok && kb > 0) {
                return static_cast<qint64>(kb) * 1024;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<bool> readSecureBoot(const QString &efivarsDir)
{
    const QString varPath = efivarsDir + QLatin1Char('/') + QLatin1String(kSecureBootVarName);
    QFile f(varPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    // efivars layout: 4 bytes EFI attributes followed by the value (1 byte for SecureBoot).
    const QByteArray blob = f.read(5);
    if (blob.size() < 5) {
        return std::nullopt;
    }
    return blob.at(4) != 0;
}

} // namespace

SlmUefiCheckJob::SlmUefiCheckJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmUefiCheckJob::~SlmUefiCheckJob() = default;

QString SlmUefiCheckJob::prettyName() const
{
    return tr("Validate UEFI firmware");
}

Calamares::JobResult SlmUefiCheckJob::exec()
{
    const QString efiRoot = QStringLiteral("/sys/firmware/efi");
    const QString efivarsDir = efiRoot + QStringLiteral("/efivars");
    const QString meminfoPath = QStringLiteral("/proc/meminfo");

    const bool firmwarePresent = pathAccess(efiRoot, F_OK);
    const bool efivarsWritable = firmwarePresent && pathAccess(efivarsDir, W_OK);
    const std::optional<bool> secureBoot = firmwarePresent ? readSecureBoot(efivarsDir) : std::nullopt;
    const std::optional<qint64> ramBytes = readMemTotalBytes(meminfoPath);

    QStringList blocks;
    QStringList warnings;

    if (!firmwarePresent) {
        blocks << QStringLiteral("UEFI_001");
    }
    if (firmwarePresent && !efivarsWritable) {
        blocks << QStringLiteral("UEFI_002");
    }
    if (ramBytes.has_value() && ramBytes.value() < kRamBlockBytes) {
        blocks << QStringLiteral("UEFI_003");
    }
    if (ramBytes.has_value() && ramBytes.value() >= kRamBlockBytes
        && ramBytes.value() < kRamWarnBytes) {
        warnings << QStringLiteral("UEFI_W002");
    }
    if (secureBoot.has_value() && secureBoot.value()) {
        warnings << QStringLiteral("UEFI_W001");
    }

    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        gs->insert(QStringLiteral("slm.hw.uefi.firmware_present"), firmwarePresent);
        gs->insert(QStringLiteral("slm.hw.uefi.efivars_writable"), efivarsWritable);
        if (secureBoot.has_value()) {
            gs->insert(QStringLiteral("slm.hw.uefi.secure_boot"), secureBoot.value());
        }
        if (ramBytes.has_value()) {
            gs->insert(QStringLiteral("slm.hw.ram.total_bytes"),
                       QVariant::fromValue(ramBytes.value()));
        }
        // Merge with any blocks/warnings already collected by earlier jobs.
        QStringList allBlocks = gs->value(QStringLiteral("slm.hw.blocks")).toStringList();
        allBlocks << blocks;
        gs->insert(QStringLiteral("slm.hw.blocks"), allBlocks);
        QStringList allWarnings = gs->value(QStringLiteral("slm.hw.warnings")).toStringList();
        allWarnings << warnings;
        gs->insert(QStringLiteral("slm.hw.warnings"), allWarnings);
    } else {
        cWarning() << "slm-uefi-check: JobQueue GlobalStorage unavailable";
    }

    cDebug() << "slm-uefi-check:"
             << "firmware=" << firmwarePresent
             << "efivars_writable=" << efivarsWritable
             << "secure_boot=" << (secureBoot.has_value() ? (secureBoot.value() ? "on" : "off") : "unknown")
             << "ram_bytes=" << (ramBytes.has_value() ? ramBytes.value() : -1)
             << "blocks=" << blocks.join(QLatin1Char(','))
             << "warnings=" << warnings.join(QLatin1Char(','));

    if (!blocks.isEmpty()) {
        return Calamares::JobResult::error(
            tr("SLM Desktop requires modern UEFI firmware."),
            tr("Failed pre-install checks: %1").arg(blocks.join(QStringLiteral(", "))));
    }
    return Calamares::JobResult::ok();
}

void SlmUefiCheckJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
