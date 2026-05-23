#include "Config.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

namespace Slm::Installer {

SummaryConfig::SummaryConfig(QObject *parent)
    : QObject(parent)
    , m_language(tr("English (United States)"))
    , m_timezone(tr("Auto-detected from network (can be changed after install)"))
{
}

void SummaryConfig::reload()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        cWarning() << "slm-summary: GlobalStorage unavailable on reload()";
        emit changed();
        return;
    }

    // Calamares::GlobalStorage::value() returns an invalid QVariant when
    // the key is missing — fall back per-field rather than relying on a
    // (key, default) overload that doesn't exist in 3.3.14.
    const auto readInt = [gs](const QString &key, int fallback) {
        const QVariant v = gs->value(key);
        return v.isValid() ? v.toInt() : fallback;
    };
    const auto readBool = [gs](const QString &key, bool fallback) {
        const QVariant v = gs->value(key);
        return v.isValid() ? v.toBool() : fallback;
    };

    m_diskPath = gs->value(QStringLiteral("slm.target.disk")).toString();
    m_diskName = gs->value(QStringLiteral("slm.target.disk_name")).toString();
    m_espSizeMb = readInt(QStringLiteral("slm.target.esp_size_mb"), 1024);
    m_recoverySizeMb = readInt(QStringLiteral("slm.target.recovery_size_mb"), 8192);
    m_subvolumes = gs->value(QStringLiteral("slm.btrfs.subvolumes")).toStringList();
    m_warnings = gs->value(QStringLiteral("slm.hw.warnings")).toStringList();

    // Factory snapshot defaults true — slm-snapshot decides at runtime
    // whether to actually run based on §4.4 disk-space policy. The summary
    // shows the *intent* here; a future "skip factory snapshot" advanced
    // toggle would set this key explicitly to false.
    m_factorySnapshotEnabled = readBool(QStringLiteral("slm.snapshot.enabled"), true);

    emit changed();
}

void SummaryConfig::commit()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        gs->insert(QStringLiteral("slm.target.confirmed"), true);
    } else {
        cWarning() << "slm-summary: GlobalStorage unavailable; "
                   << "slm.target.confirmed not written";
    }
    emit commitRequested();
}

void SummaryConfig::back()
{
    emit backRequested();
}

} // namespace Slm::Installer
