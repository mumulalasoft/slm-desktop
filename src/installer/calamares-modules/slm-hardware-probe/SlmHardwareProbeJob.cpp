#include "SlmHardwareProbeJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmHardwareProbeJobFactory, registerPlugin<SlmHardwareProbeJob>();)

namespace {

constexpr const char *kSysPciDevices = "/sys/bus/pci/devices";
constexpr const char *kSysUsbDevices = "/sys/bus/usb/devices";
constexpr const char *kSysDrm = "/sys/class/drm";

// PCI vendor IDs we recognise. Stored as 4-char lowercase hex without "0x".
constexpr const char *kVendorIntel = "8086";
constexpr const char *kVendorNvidia = "10de";
constexpr const char *kVendorAmd = "1002";
constexpr const char *kVendorBroadcomPci = "14e4";
constexpr const char *kVendorBroadcomUsb = "0a5c";

QString readTrimmed(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLatin1(f.readAll()).trimmed();
}

// Normalise "0x10de\n" → "10de". USB writes plain "10de" with no prefix; this
// handles both.
QString normaliseVendor(QString raw)
{
    raw = raw.trimmed().toLower();
    if (raw.startsWith(QLatin1String("0x"))) {
        raw = raw.mid(2);
    }
    return raw;
}

// Returns the highest-priority GPU vendor present across all DRM cards,
// preferring discrete vendors over integrated ones (nvidia > amd > intel).
QString detectGpuVendor(const QString &drmRoot)
{
    QDir drmDir(drmRoot);
    const QStringList cards = drmDir.entryList(QStringList{ QStringLiteral("card[0-9]*") },
                                                QDir::Dirs | QDir::NoDotAndDotDot);
    bool sawNvidia = false;
    bool sawAmd = false;
    bool sawIntel = false;
    bool sawAny = false;
    for (const QString &card : cards) {
        // Filter out connector entries like "card1-HDMI-A-1".
        if (card.contains(QLatin1Char('-'))) {
            continue;
        }
        const QString vendor = normaliseVendor(readTrimmed(drmRoot + QLatin1Char('/') + card
                                                           + QStringLiteral("/device/vendor")));
        if (vendor.isEmpty()) {
            continue;
        }
        sawAny = true;
        if (vendor == QLatin1String(kVendorNvidia)) sawNvidia = true;
        else if (vendor == QLatin1String(kVendorAmd)) sawAmd = true;
        else if (vendor == QLatin1String(kVendorIntel)) sawIntel = true;
    }
    if (sawNvidia) return QStringLiteral("nvidia");
    if (sawAmd) return QStringLiteral("amd");
    if (sawIntel) return QStringLiteral("intel");
    if (sawAny) return QStringLiteral("unknown");
    return QStringLiteral("unknown");
}

// Wayland-capable if at least one DRM card exposes a non-empty /dev major:minor.
// "limited" is reserved for future use (e.g. NVIDIA proprietary without
// nouveau) where we can detect the degradation more precisely.
QString detectWaylandCapability(const QString &drmRoot)
{
    QDir drmDir(drmRoot);
    const QStringList cards = drmDir.entryList(QStringList{ QStringLiteral("card[0-9]*") },
                                                QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cards) {
        if (card.contains(QLatin1Char('-'))) {
            continue;
        }
        const QString dev = readTrimmed(drmRoot + QLatin1Char('/') + card + QStringLiteral("/dev"));
        if (!dev.isEmpty()) {
            return QStringLiteral("capable");
        }
    }
    return QStringLiteral("unsupported");
}

bool detectBroadcomPci(const QString &pciRoot)
{
    QDir d(pciRoot);
    const QStringList devices = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dev : devices) {
        const QString vendor = normaliseVendor(readTrimmed(pciRoot + QLatin1Char('/') + dev
                                                           + QStringLiteral("/vendor")));
        if (vendor == QLatin1String(kVendorBroadcomPci)) {
            return true;
        }
    }
    return false;
}

bool detectBroadcomUsb(const QString &usbRoot)
{
    QDir d(usbRoot);
    const QStringList devices = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dev : devices) {
        const QString vendor = normaliseVendor(readTrimmed(usbRoot + QLatin1Char('/') + dev
                                                           + QStringLiteral("/idVendor")));
        if (vendor == QLatin1String(kVendorBroadcomUsb)) {
            return true;
        }
    }
    return false;
}

} // namespace

SlmHardwareProbeJob::SlmHardwareProbeJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmHardwareProbeJob::~SlmHardwareProbeJob() = default;

QString SlmHardwareProbeJob::prettyName() const
{
    return tr("Probe hardware compatibility");
}

Calamares::JobResult SlmHardwareProbeJob::exec()
{
    const QString drmRoot = QString::fromLatin1(kSysDrm);
    const QString pciRoot = QString::fromLatin1(kSysPciDevices);
    const QString usbRoot = QString::fromLatin1(kSysUsbDevices);

    const QString gpuVendor = detectGpuVendor(drmRoot);
    const QString waylandCapability = detectWaylandCapability(drmRoot);
    const bool broadcom = detectBroadcomPci(pciRoot) || detectBroadcomUsb(usbRoot);

    QStringList warnings;
    if (waylandCapability != QLatin1String("capable")) {
        warnings << QStringLiteral("HW_W001");
    }
    if (broadcom) {
        warnings << QStringLiteral("HW_W002");
    }

    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        gs->insert(QStringLiteral("slm.hw.gpu.vendor"), gpuVendor);
        gs->insert(QStringLiteral("slm.hw.gpu.wayland"), waylandCapability);
        // Proprietary driver availability depends on whether the live ISO
        // shipped /iso/slm-drivers-optional.squashfs. That mount happens later
        // in the pipeline, so this stays false here and a later job may set
        // it true.
        gs->insert(QStringLiteral("slm.hw.gpu.proprietary_available"), false);
        gs->insert(QStringLiteral("slm.hw.broadcom.detected"), broadcom);
        // Merge with any warnings already collected by earlier jobs.
        QStringList allWarnings = gs->value(QStringLiteral("slm.hw.warnings")).toStringList();
        allWarnings << warnings;
        gs->insert(QStringLiteral("slm.hw.warnings"), allWarnings);
        // SMART check deferred to slm-partition (it owns the target disk).
    } else {
        cWarning() << "slm-hardware-probe: JobQueue GlobalStorage unavailable";
    }

    cDebug() << "slm-hardware-probe:"
             << "gpu=" << gpuVendor
             << "wayland=" << waylandCapability
             << "broadcom=" << broadcom
             << "warnings=" << warnings.join(QLatin1Char(','));

    return Calamares::JobResult::ok();
}

void SlmHardwareProbeJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
