#include "featureflagscontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

FeatureFlagsController::FeatureFlagsController(QObject *parent)
    : QObject(parent)
{
    m_configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                   + QStringLiteral("/slm/feature-flags.conf");

    initRegistry();
    load();
    rebuild();
}

void FeatureFlagsController::initRegistry()
{
    m_registry = {
        {
            QStringLiteral("qml-hot-reload"),
            QStringLiteral("QML Hot Reload"),
            QStringLiteral("Reload QML files without restarting the shell when source files change on disk."),
            QStringLiteral("preview"),
            QStringLiteral("immediately"),
            false
        },
        {
            QStringLiteral("compositor-vulkan"),
            QStringLiteral("Compositor Vulkan Backend"),
            QStringLiteral("Use Vulkan as the compositor rendering backend instead of OpenGL. Experimental and may cause visual glitches."),
            QStringLiteral("experimental"),
            QStringLiteral("restart-compositor"),
            false
        },
        {
            QStringLiteral("new-launcher-engine"),
            QStringLiteral("New Launcher Engine"),
            QStringLiteral("Next-generation app launcher with improved search ranking and lower memory footprint."),
            QStringLiteral("experimental"),
            QStringLiteral("restart-session"),
            false
        },
        {
            QStringLiteral("portal-request-log"),
            QStringLiteral("Portal Request Log"),
            QStringLiteral("Keep a persistent log of all XDG portal requests across sessions. Stored in ~/.local/share/slm/portal-log.jsonl."),
            QStringLiteral("preview"),
            QStringLiteral("immediately"),
            false
        },
        {
            QStringLiteral("env-system-scope"),
            QStringLiteral("System Scope Environment"),
            QStringLiteral("Enable writing to /etc/slm/environment.d/ via polkit. Requires slm-envd-helper to be installed."),
            QStringLiteral("experimental"),
            QStringLiteral("immediately"),
            false
        },
        {
            QStringLiteral("dbus-inspector-write"),
            QStringLiteral("D-Bus Inspector Write Mode"),
            QStringLiteral("Allow sending test D-Bus method calls from the inspector. Use with caution — calls are executed immediately."),
            QStringLiteral("experimental"),
            QStringLiteral("immediately"),
            false
        },
        {
            QStringLiteral("legacy-env-resolver"),
            QStringLiteral("Legacy Environment Resolver"),
            QStringLiteral("Use the Phase 1 local JSON resolver instead of the slm-envd D-Bus service. Deprecated, will be removed in a future release."),
            QStringLiteral("deprecated"),
            QStringLiteral("next-launch"),
            false
        },
    };
}

void FeatureFlagsController::load()
{
    QSettings s(m_configPath, QSettings::IniFormat);
    for (FlagDef &flag : m_registry) {
        flag.defaultEnabled = s.value(flag.id, flag.defaultEnabled).toBool();
    }
}

void FeatureFlagsController::save()
{
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QSettings s(m_configPath, QSettings::IniFormat);
    for (const FlagDef &flag : std::as_const(m_registry))
        s.setValue(flag.id, flag.defaultEnabled);
    s.sync();
}

void FeatureFlagsController::rebuild()
{
    m_flags.clear();
    for (const FlagDef &flag : std::as_const(m_registry)) {
        m_flags.append(QVariantMap{
            { QStringLiteral("id"),          flag.id              },
            { QStringLiteral("name"),        flag.name            },
            { QStringLiteral("description"), flag.description     },
            { QStringLiteral("badge"),       flag.badge           },
            { QStringLiteral("effectWhen"),  flag.effectWhen      },
            { QStringLiteral("enabled"),     flag.defaultEnabled  },
        });
    }
    emit flagsChanged();
}

bool FeatureFlagsController::isEnabled(const QString &id) const
{
    for (const FlagDef &flag : m_registry) {
        if (flag.id == id)
            return flag.defaultEnabled;
    }
    return false;
}

void FeatureFlagsController::setEnabled(const QString &id, bool enabled)
{
    for (FlagDef &flag : m_registry) {
        if (flag.id == id) {
            if (flag.defaultEnabled == enabled)
                return;
            flag.defaultEnabled = enabled;
            save();
            rebuild();
            return;
        }
    }
}

void FeatureFlagsController::resetAll()
{
    for (FlagDef &flag : m_registry)
        flag.defaultEnabled = false;
    save();
    rebuild();
}
