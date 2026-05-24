#include "SlmFirstbootJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmFirstbootJobFactory, registerPlugin<SlmFirstbootJob>();)

namespace {

// Spec §7 requires this path; do not relocate without updating
// slm-session-broker (src/login/session-broker/sessionbroker.cpp).
constexpr const char *kMarkerRelativePath = "var/lib/slm/firstboot_pending";

constexpr const char *kInstallerVersion = "1.0.0";

QString resolveRootMountPoint()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
        if (!fromGs.isEmpty()) {
            return fromGs;
        }
    }
    // Fallback to the staging path documented in §2.3. Hitting this branch in
    // production means a prior module failed to set rootMountPoint, which is a
    // real bug worth seeing in the logs.
    return QStringLiteral("/mnt/slm-install-root");
}

} // namespace

SlmFirstbootJob::SlmFirstbootJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmFirstbootJob::~SlmFirstbootJob() = default;

QString SlmFirstbootJob::prettyName() const
{
    return tr("Prepare first boot");
}

Calamares::JobResult SlmFirstbootJob::exec()
{
    const QString rootMount = resolveRootMountPoint();
    const QString markerPath = rootMount + QLatin1Char('/') + QLatin1String(kMarkerRelativePath);

    const QFileInfo info(markerPath);
    if (!QDir().mkpath(info.absolutePath())) {
        cWarning() << "slm-firstboot: cannot create" << info.absolutePath();
        return Calamares::JobResult::error(
            tr("Unable to prepare first-boot marker directory."),
            QStringLiteral("FB_002: mkpath failed for %1").arg(info.absolutePath()));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("created_at"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    payload.insert(QStringLiteral("installer_version"), QLatin1String(kInstallerVersion));
    payload.insert(QStringLiteral("install_id"),
                   QUuid::createUuid().toString(QUuid::WithoutBraces));
    payload.insert(QStringLiteral("oobe_required"), true);

    const QByteArray serialised = QJsonDocument(payload).toJson(QJsonDocument::Indented);

    QFile out(markerPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cWarning() << "slm-firstboot: cannot open" << markerPath << "for write";
        return Calamares::JobResult::error(
            tr("Unable to write first-boot marker."),
            QStringLiteral("FB_001: open(%1) failed: %2")
                .arg(markerPath, out.errorString()));
    }
    if (out.write(serialised) != serialised.size()) {
        const QString err = out.errorString();
        out.close();
        return Calamares::JobResult::error(
            tr("Unable to write first-boot marker."),
            QStringLiteral("FB_001: short write to %1: %2").arg(markerPath, err));
    }
    out.close();

    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        gs->insert(QStringLiteral("slm.firstboot.marker_path"), markerPath);
        gs->insert(QStringLiteral("slm.firstboot.install_id"),
                   payload.value(QStringLiteral("install_id")).toString());
    }

    cDebug() << "slm-firstboot: wrote marker" << markerPath;
    return Calamares::JobResult::ok();
}

void SlmFirstbootJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
