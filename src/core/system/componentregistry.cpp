#include "componentregistry.h"

#include <utility>

namespace Slm::System {

namespace {

QMap<QString, QString> distroPackages(const QString &fallback,
                                      std::initializer_list<std::pair<const char *, const char *>> entries)
{
    QMap<QString, QString> out;
    if (!fallback.trimmed().isEmpty()) {
        out.insert(QStringLiteral("*"), fallback.trimmed());
    }
    for (const auto &entry : entries) {
        out.insert(QString::fromLatin1(entry.first).toLower(),
                   QString::fromLatin1(entry.second).trimmed());
    }
    return out;
}

QList<ComponentRequirement> componentList()
{
    return {
        ComponentRequirement{
            QStringLiteral("samba"),
            QStringLiteral("Samba"),
            QStringLiteral("Komponen berbagi folder jaringan (SMB usershare)."),
            QStringLiteral("samba"),
            QStringList{QStringLiteral("net")},
            {},
            true,
            QStringLiteral("Pasang paket samba agar fitur folder sharing aktif."),
            QStringLiteral("required"),
            distroPackages(QStringLiteral("samba"),
                           {{"debian", "samba"},
                            {"ubuntu", "samba"},
                            {"fedora", "samba"},
                            {"arch", "samba"},
                            {"opensuse", "samba"}})
        },
        ComponentRequirement{
            QStringLiteral("gvfs"),
            QStringLiteral("GVFS Daemons"),
            QStringLiteral("Diperlukan untuk integrasi mount backend pada file manager."),
            QStringLiteral("gvfs-daemons"),
            {},
            QStringList{
                QStringLiteral("/usr/libexec/gvfsd|/usr/lib/gvfs/gvfsd|/usr/lib/x86_64-linux-gnu/gvfs/gvfsd|/usr/lib64/gvfs/gvfsd")
            },
            true,
            QStringLiteral("Pasang gvfs daemons agar file manager dapat berinteraksi dengan storage virtual."),
            QStringLiteral("required"),
            distroPackages(QStringLiteral("gvfs-daemons"),
                           {{"debian", "gvfs-daemons"},
                            {"ubuntu", "gvfs-daemons"},
                            {"fedora", "gvfs-daemon"},
                            {"arch", "gvfs"},
                            {"opensuse", "gvfs"}})
        },
        ComponentRequirement{
            QStringLiteral("gvfs-smb"),
            QStringLiteral("GVFS SMB Backend"),
            QStringLiteral("Backend SMB untuk membuka share jaringan dari file manager."),
            QStringLiteral("gvfs-backends"),
            {},
            QStringList{
                QStringLiteral("/usr/libexec/gvfsd-smb|/usr/lib/gvfs/gvfsd-smb|/usr/lib/x86_64-linux-gnu/gvfs/gvfsd-smb|/usr/lib64/gvfs/gvfsd-smb")
            },
            true,
            QStringLiteral("Pasang backend GVFS SMB agar browsing SMB share berfungsi."),
            QStringLiteral("recommended"),
            distroPackages(QStringLiteral("gvfs-backends"),
                           {{"debian", "gvfs-backends"},
                            {"ubuntu", "gvfs-backends"},
                            {"fedora", "gvfs-smb"},
                            {"arch", "gvfs-smb"},
                            {"opensuse", "gvfs-backends"}})
        },
        ComponentRequirement{
            QStringLiteral("pkexec"),
            QStringLiteral("PolicyKit"),
            QStringLiteral("Diperlukan untuk aksi instalasi/perbaikan berizin administrator."),
            QStringLiteral("policykit-1"),
            QStringList{QStringLiteral("pkexec")},
            {},
            true,
            QStringLiteral("Pasang policykit-1 agar aksi perbaikan otomatis bisa berjalan."),
            QStringLiteral("required"),
            distroPackages(QStringLiteral("policykit-1"),
                           {{"fedora", "polkit"},
                            {"arch", "polkit"},
                            {"opensuse", "polkit"}})
        },
        ComponentRequirement{
            QStringLiteral("journalctl"),
            QStringLiteral("Journalctl"),
            QStringLiteral("Diperlukan untuk melihat ringkasan log recovery."),
            QStringLiteral("systemd"),
            QStringList{QStringLiteral("journalctl")},
            {},
            true,
            QStringLiteral("Pasang systemd utils agar log diagnosis tersedia."),
            QStringLiteral("recommended"),
            {}
        },
        ComponentRequirement{
            QStringLiteral("slm-watchdog"),
            QStringLiteral("SLM Watchdog"),
            QStringLiteral("Komponen pemantau stabilitas sesi desktop."),
            QStringLiteral("slm-desktop"),
            QStringList{QStringLiteral("slm-watchdog")},
            {},
            false,
            QStringLiteral("Pasang ulang paket slm-desktop untuk memulihkan slm-watchdog."),
            QStringLiteral("required"),
            {}
        },
        ComponentRequirement{
            QStringLiteral("slm-recovery-app"),
            QStringLiteral("SLM Recovery App"),
            QStringLiteral("Aplikasi pemulihan saat sesi mengalami crash berulang."),
            QStringLiteral("slm-desktop"),
            QStringList{QStringLiteral("slm-recovery-app")},
            {},
            false,
            QStringLiteral("Pasang ulang paket slm-desktop untuk memulihkan aplikasi recovery."),
            QStringLiteral("required"),
            {}
        },
        ComponentRequirement{
            QStringLiteral("gio"),
            QStringLiteral("GIO"),
            QStringLiteral("Diperlukan oleh portal manager untuk integrasi file URI."),
            QStringLiteral("libglib2.0-bin"),
            QStringList{QStringLiteral("gio")},
            {},
            true,
            QStringLiteral("Pasang utilitas GIO agar portal integration berjalan."),
            QStringLiteral("recommended"),
            distroPackages(QStringLiteral("libglib2.0-bin"),
                           {{"fedora", "glib2"},
                            {"arch", "glib2"},
                            {"opensuse", "glib2-tools"}})
        },
        ComponentRequirement{
            QStringLiteral("iproute2"),
            QStringLiteral("IPRoute2"),
            QStringLiteral("Diperlukan untuk deteksi network route/default gateway."),
            QStringLiteral("iproute2"),
            QStringList{QStringLiteral("ip")},
            {},
            true,
            QStringLiteral("Pasang iproute2 agar layanan jaringan dapat membaca status route."),
            QStringLiteral("required"),
            distroPackages(QStringLiteral("iproute2"),
                           {{"fedora", "iproute"},
                            {"arch", "iproute2"},
                            {"opensuse", "iproute2"}})
        },
        ComponentRequirement{
            QStringLiteral("bluez"),
            QStringLiteral("BlueZ Tools"),
            QStringLiteral("Diperlukan untuk manajemen perangkat bluetooth."),
            QStringLiteral("bluez"),
            QStringList{QStringLiteral("bluetoothctl")},
            {},
            true,
            QStringLiteral("Pasang bluez agar modul bluetooth berfungsi."),
            QStringLiteral("required"),
            distroPackages(QStringLiteral("bluez"),
                           {{"fedora", "bluez"},
                            {"arch", "bluez"},
                            {"opensuse", "bluez"}})
        },
        ComponentRequirement{
            QStringLiteral("cups-client"),
            QStringLiteral("CUPS Client"),
            QStringLiteral("Diperlukan untuk status printer dan operasi print."),
            QStringLiteral("cups-client"),
            QStringList{QStringLiteral("lpstat")},
            {},
            true,
            QStringLiteral("Pasang cups-client agar fitur printing aktif."),
            QStringLiteral("recommended"),
            distroPackages(QStringLiteral("cups-client"),
                           {{"fedora", "cups-client"},
                            {"arch", "cups"},
                            {"opensuse", "cups-client"}})
        },
    };
}

bool belongsToDomain(const QString &id, const QString &domain)
{
    const QString d = domain.trimmed().toLower();
    if (d == QStringLiteral("filemanager")) {
        return id == QStringLiteral("samba")
            || id == QStringLiteral("gvfs")
            || id == QStringLiteral("gvfs-smb");
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

QVariantList ComponentRegistry::missingRequiredForDomain(const QString &domain)
{
    QVariantList out;
    const QVariantList allMissing = missingForDomain(domain);
    for (const QVariant &row : allMissing) {
        const QVariantMap map = row.toMap();
        const QString severity = map.value(QStringLiteral("severity"))
                .toString()
                .trimmed()
                .toLower();
        if (severity.isEmpty() || severity == QStringLiteral("required")) {
            out.push_back(map);
        }
    }
    return out;
}

bool ComponentRegistry::hasBlockingMissingForDomain(const QString &domain)
{
    return !missingRequiredForDomain(domain).isEmpty();
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
