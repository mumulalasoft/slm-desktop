#include "globalprogresscenter.h"

#include <QDateTime>
#include <limits>

GlobalProgressCenter::GlobalProgressCenter(QObject *parent)
    : QObject(parent)
{
}

bool GlobalProgressCenter::visible() const
{
    return !m_activeSource.isEmpty() && m_entries.contains(m_activeSource);
}

QString GlobalProgressCenter::source() const
{
    return m_activeSource;
}

QString GlobalProgressCenter::title() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? QString() : it->title;
}

QString GlobalProgressCenter::detail() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? QString() : it->detail;
}

double GlobalProgressCenter::progress() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? 0.0 : it->progress;
}

bool GlobalProgressCenter::indeterminate() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? false : it->indeterminate;
}

bool GlobalProgressCenter::paused() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? false : it->paused;
}

bool GlobalProgressCenter::canPause() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? false : it->canPause;
}

bool GlobalProgressCenter::canCancel() const
{
    const auto it = m_entries.constFind(m_activeSource);
    return it == m_entries.constEnd() ? false : it->canCancel;
}

void GlobalProgressCenter::showProgress(const QString &source,
                                        const QString &title,
                                        const QString &detail,
                                        double progress,
                                        bool indeterminate,
                                        bool canPause,
                                        bool paused,
                                        bool canCancel)
{
    const QString key = source.trimmed().isEmpty() ? QStringLiteral("default") : source.trimmed();
    Entry e;
    e.source = key;
    e.title = title;
    e.detail = detail;
    e.progress = qBound(0.0, progress, 1.0);
    e.indeterminate = indeterminate;
    e.paused = paused;
    e.canPause = canPause;
    e.canCancel = canCancel;
    e.updatedAtMs = QDateTime::currentMSecsSinceEpoch();

    m_entries.insert(key, e);
    m_activeSource = key;
    emit changed();
}

void GlobalProgressCenter::hideProgress(const QString &source)
{
    const QString key = source.trimmed().isEmpty() ? QStringLiteral("default") : source.trimmed();
    if (!m_entries.contains(key)) {
        return;
    }
    m_entries.remove(key);
    refreshActiveSource();
    emit changed();
}

void GlobalProgressCenter::requestPauseResume()
{
    if (!visible() || !canPause()) {
        return;
    }
    if (paused()) {
        emit resumeRequested(m_activeSource);
        return;
    }
    emit pauseRequested(m_activeSource);
}

void GlobalProgressCenter::requestCancel()
{
    if (!visible() || !canCancel()) {
        return;
    }
    emit cancelRequested(m_activeSource);
}

void GlobalProgressCenter::refreshActiveSource()
{
    if (m_entries.isEmpty()) {
        m_activeSource.clear();
        return;
    }
    qint64 newest = std::numeric_limits<qint64>::min();
    QString chosen;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it->updatedAtMs >= newest) {
            newest = it->updatedAtMs;
            chosen = it.key();
        }
    }
    m_activeSource = chosen;
}
