#include "componentregistry.h"

namespace Slm::System {

namespace {

QList<ComponentRequirement> componentList()
{
    return {
        ComponentRequirement{
            QStringLiteral("samba"),
            QStringLiteral("Samba"),
            QStringLiteral("Komponen berbagi folder jaringan (SMB usershare)."),
            QStringLiteral("samba"),
            QStringList{QStringLiteral("net")},
            true,
            QStringLiteral("Pasang paket samba agar fitur folder sharing aktif.")
        },
        ComponentRequirement{
            QStringLiteral("pkexec"),
            QStringLiteral("PolicyKit"),
            QStringLiteral("Diperlukan untuk aksi instalasi/perbaikan berizin administrator."),
            QStringLiteral("policykit-1"),
            QStringList{QStringLiteral("pkexec")},
            true,
            QStringLiteral("Pasang policykit-1 agar aksi perbaikan otomatis bisa berjalan.")
        },
        ComponentRequirement{
            QStringLiteral("journalctl"),
            QStringLiteral("Journalctl"),
            QStringLiteral("Diperlukan untuk melihat ringkasan log recovery."),
            QStringLiteral("systemd"),
            QStringList{QStringLiteral("journalctl")},
            true,
            QStringLiteral("Pasang systemd utils agar log diagnosis tersedia.")
        },
        ComponentRequirement{
            QStringLiteral("slm-watchdog"),
            QStringLiteral("SLM Watchdog"),
            QStringLiteral("Komponen pemantau stabilitas sesi desktop."),
            QStringLiteral("slm-desktop"),
            QStringList{QStringLiteral("slm-watchdog")},
            false,
            QStringLiteral("Pasang ulang paket slm-desktop untuk memulihkan slm-watchdog.")
        },
        ComponentRequirement{
            QStringLiteral("slm-recovery-app"),
            QStringLiteral("SLM Recovery App"),
            QStringLiteral("Aplikasi pemulihan saat sesi mengalami crash berulang."),
            QStringLiteral("slm-desktop"),
            QStringList{QStringLiteral("slm-recovery-app")},
            false,
            QStringLiteral("Pasang ulang paket slm-desktop untuk memulihkan aplikasi recovery.")
        },
        ComponentRequirement{
            QStringLiteral("gio"),
            QStringLiteral("GIO"),
            QStringLiteral("Diperlukan oleh portal manager untuk integrasi file URI."),
            QStringLiteral("libglib2.0-bin"),
            QStringList{QStringLiteral("gio")},
            true,
            QStringLiteral("Pasang utilitas GIO agar portal integration berjalan.")
        },
        ComponentRequirement{
            QStringLiteral("iproute2"),
            QStringLiteral("IPRoute2"),
            QStringLiteral("Diperlukan untuk deteksi network route/default gateway."),
            QStringLiteral("iproute2"),
            QStringList{QStringLiteral("ip")},
            true,
            QStringLiteral("Pasang iproute2 agar layanan jaringan dapat membaca status route.")
        },
        ComponentRequirement{
            QStringLiteral("bluez"),
            QStringLiteral("BlueZ Tools"),
            QStringLiteral("Diperlukan untuk manajemen perangkat bluetooth."),
            QStringLiteral("bluez"),
            QStringList{QStringLiteral("bluetoothctl")},
            true,
            QStringLiteral("Pasang bluez agar modul bluetooth berfungsi.")
        },
        ComponentRequirement{
            QStringLiteral("cups-client"),
            QStringLiteral("CUPS Client"),
            QStringLiteral("Diperlukan untuk status printer dan operasi print."),
            QStringLiteral("cups-client"),
            QStringList{QStringLiteral("lpstat")},
            true,
            QStringLiteral("Pasang cups-client agar fitur printing aktif.")
        },
    };
}

bool belongsToDomain(const QString &id, const QString &domain)
{
    const QString d = domain.trimmed().toLower();
    if (d == QStringLiteral("filemanager")) {
        return id == QStringLiteral("samba");
    }
    if (d == QStringLiteral("recovery")) {
        return id == QStringLiteral("pkexec")
            || id == QStringLiteral("journalctl");
    }
    if (d == QStringLiteral("greeter")) {
        return id == QStringLiteral("pkexec")
            || id == QStringLiteral("slm-watchdog")
            || id == QStringLiteral("slm-recovery-app");
    }
    if (d == QStringLiteral("printing")) {
        return id == QStringLiteral("cups-client");
    }
    if (d == QStringLiteral("network")) {
        return id == QStringLiteral("iproute2");
    }
    if (d == QStringLiteral("bluetooth")) {
        return id == QStringLiteral("bluez");
    }
    if (d == QStringLiteral("portal")) {
        return id == QStringLiteral("gio");
    }
    return true;
}

} // namespace

QList<ComponentRequirement> ComponentRegistry::all()
{
    return componentList();
}

QList<ComponentRequirement> ComponentRegistry::forDomain(const QString &domain)
{
    QList<ComponentRequirement> out;
    const QList<ComponentRequirement> allComponents = componentList();
    for (const ComponentRequirement &req : allComponents) {
        if (belongsToDomain(req.id, domain)) {
            out.push_back(req);
        }
    }
    return out;
}

QVariantList ComponentRegistry::missingForDomain(const QString &domain)
{
    QVariantList out;
    const QList<ComponentRequirement> components = forDomain(domain);
    for (const ComponentRequirement &req : components) {
        const QVariantMap result = checkComponent(req);
        if (!result.value(QStringLiteral("ready")).toBool()) {
            out.push_back(result);
        }
    }
    return out;
}

bool ComponentRegistry::findById(const QString &componentId, ComponentRequirement *out)
{
    const QString target = componentId.trimmed().toLower();
    const QList<ComponentRequirement> allComponents = componentList();
    for (const ComponentRequirement &req : allComponents) {
        if (req.id == target) {
            if (out) {
                *out = req;
            }
            return true;
        }
    }
    return false;
}

} // namespace Slm::System
