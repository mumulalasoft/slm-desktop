#include "appsandboxcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

static constexpr int kFlatpakListTimeoutMs = 5000;

AppSandboxController::AppSandboxController(QObject *parent)
    : QObject(parent)
{
    detectTools();
    if (m_flatpakAvailable)
        loadApps();
}

void AppSandboxController::refresh()
{
    detectTools();
    emit flatpakAvailableChanged();
    emit bubblewrapAvailableChanged();

    m_apps.clear();
    m_error.clear();
    emit errorChanged();

    if (m_flatpakAvailable) {
        loadApps();
    } else {
        emit appsChanged();
    }
}

// ── Tool detection ─────────────────────────────────────────────────────────

void AppSandboxController::detectTools()
{
    m_flatpakAvailable    = !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();
    m_bubblewrapAvailable = !QStandardPaths::findExecutable(QStringLiteral("bwrap")).isEmpty();
}

// ── App loading ────────────────────────────────────────────────────────────

void AppSandboxController::loadApps()
{
    m_loading = true;
    emit loadingChanged();

    QProcess proc;
    proc.start(QStringLiteral("flatpak"),
               { QStringLiteral("list"),
                 QStringLiteral("--app"),
                 QStringLiteral("--columns=application,name,version,origin") });

    if (!proc.waitForFinished(kFlatpakListTimeoutMs)) {
        m_error   = QStringLiteral("flatpak list timed out");
        m_loading = false;
        emit errorChanged();
        emit loadingChanged();
        return;
    }

    if (proc.exitCode() != 0) {
        m_error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (m_error.isEmpty())
            m_error = QStringLiteral("flatpak list failed (exit %1)").arg(proc.exitCode());
        m_loading = false;
        emit errorChanged();
        emit loadingChanged();
        return;
    }

    QVariantList result;
    const QString     output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines  = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QStringList cols = line.split(QLatin1Char('\t'));
        if (cols.size() < 2)
            continue;

        const QString id      = cols.value(0).trimmed();
        const QString name    = cols.value(1).trimmed();
        const QString version = cols.value(2).trimmed();
        const QString origin  = cols.value(3).trimmed();

        if (id.isEmpty())
            continue;

        QVariantMap app = parseMetadata(id);
        app.insert(QStringLiteral("id"),      id);
        app.insert(QStringLiteral("name"),    name.isEmpty() ? id : name);
        app.insert(QStringLiteral("version"), version);
        app.insert(QStringLiteral("origin"),  origin);
        result.append(app);
    }

    m_apps    = result;
    m_error.clear();
    m_loading = false;
    emit appsChanged();
    emit errorChanged();
    emit loadingChanged();
}

// ── Metadata parsing ───────────────────────────────────────────────────────

QVariantMap AppSandboxController::parseMetadata(const QString &appId) const
{
    // Prefer user install; fall back to system-wide install.
    const QString userPath =
        QDir::homePath()
        + QStringLiteral("/.local/share/flatpak/app/")
        + appId
        + QStringLiteral("/current/active/metadata");
    const QString systemPath =
        QStringLiteral("/var/lib/flatpak/app/")
        + appId
        + QStringLiteral("/current/active/metadata");

    QString metaPath;
    if (QFileInfo::exists(userPath))
        metaPath = userPath;
    else if (QFileInfo::exists(systemPath))
        metaPath = systemPath;
    else
        return {};

    QSettings meta(metaPath, QSettings::IniFormat);

    // [Context]
    meta.beginGroup(QStringLiteral("Context"));
    const QStringList shared = meta.value(QStringLiteral("shared")).toString()
                                   .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QStringList sockets = meta.value(QStringLiteral("sockets")).toString()
                                    .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QStringList devices = meta.value(QStringLiteral("devices")).toString()
                                    .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QStringList filesystems = meta.value(QStringLiteral("filesystems")).toString()
                                        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    meta.endGroup();

    // [Session Bus Policy] / [System Bus Policy] — just collect the service names.
    meta.beginGroup(QStringLiteral("Session Bus Policy"));
    const QStringList dbusSession = meta.childKeys();
    meta.endGroup();

    meta.beginGroup(QStringLiteral("System Bus Policy"));
    const QStringList dbusSystem = meta.childKeys();
    meta.endGroup();

    QVariantMap perms;
    perms.insert(QStringLiteral("network"),     shared.contains(QStringLiteral("network")));
    perms.insert(QStringLiteral("filesystem"),  QVariant(filesystems));
    perms.insert(QStringLiteral("sockets"),     QVariant(sockets));
    perms.insert(QStringLiteral("devices"),     QVariant(devices));
    perms.insert(QStringLiteral("dbusSession"), QVariant(dbusSession));
    perms.insert(QStringLiteral("dbusSystem"),  QVariant(dbusSystem));

    QVariantMap result;
    result.insert(QStringLiteral("permissions"),  perms);
    result.insert(QStringLiteral("sandboxLevel"), sandboxLevel(filesystems, devices));
    return result;
}

// ── Sandbox level ──────────────────────────────────────────────────────────

QString AppSandboxController::sandboxLevel(const QStringList &filesystems,
                                            const QStringList &devices)
{
    // "open" — host filesystem access or unrestricted device access.
    for (const QString &fs : filesystems) {
        if (fs == QLatin1String("host")
                || fs == QLatin1String("host-os")
                || fs == QLatin1String("host-etc"))
            return QStringLiteral("open");
    }
    if (devices.contains(QStringLiteral("all")))
        return QStringLiteral("open");

    // "partial" — home directory or other broad path access.
    for (const QString &fs : filesystems) {
        if (fs == QLatin1String("home") || fs.startsWith(QLatin1Char('~')))
            return QStringLiteral("partial");
    }
    if (!filesystems.isEmpty())
        return QStringLiteral("partial");

    return QStringLiteral("full");
}
