#include "Config.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

namespace Slm::Installer {

DiskSelectConfig::DiskSelectConfig(QObject *parent)
    : QObject(parent)
    , m_diskModel(this)
{
}

void DiskSelectConfig::setSelectedPath(const QString &path)
{
    if (path == m_selectedPath) {
        return;
    }
    m_selectedPath = path;
    emit selectedPathChanged();
}

void DiskSelectConfig::commit(const QString &path)
{
    if (path.isEmpty()) {
        cWarning() << "slm-disk-select: commit() called with empty path";
        return;
    }
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (gs) {
        gs->insert(QStringLiteral("slm.target.disk"), path);
    } else {
        cWarning() << "slm-disk-select: GlobalStorage unavailable; "
                   << "slm.target.disk not written";
    }
    setSelectedPath(path);
    emit commitRequested(path);
}

void DiskSelectConfig::refresh()
{
    setIsRefreshing(true);
    // Synchronous for now — refresh() reads /sys/block which is fast.
    // If an async SMART probe lands, that worker will set isRefreshing
    // around its own lifecycle.
    m_diskModel.refresh();
    setIsRefreshing(false);
}

void DiskSelectConfig::quit()
{
    emit quitRequested();
}

void DiskSelectConfig::setIsRefreshing(bool refreshing)
{
    if (refreshing == m_isRefreshing) {
        return;
    }
    m_isRefreshing = refreshing;
    emit isRefreshingChanged();
}

} // namespace Slm::Installer
