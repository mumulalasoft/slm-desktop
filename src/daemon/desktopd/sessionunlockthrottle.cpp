#include "sessionunlockthrottle.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Slm::Desktopd {

int SessionUnlockThrottle::lockoutSecondsForLevel(int level)
{
    if (level <= 1) return 10;
    if (level == 2) return 30;
    if (level == 3) return 60;
    if (level == 4) return 120;
    return 300;
}

int SessionUnlockThrottle::decayWindowSeconds()
{
    return 15 * 60;
}

bool SessionUnlockThrottle::applyDecay(SessionUnlockThrottleState &state, qint64 nowMs)
{
    if (state.level <= 0 || state.lastFailureMs <= 0) {
        return false;
    }
    const qint64 elapsedMs = nowMs - state.lastFailureMs;
    if (elapsedMs <= 0) {
        return false;
    }
    const qint64 windowMs = qint64(decayWindowSeconds()) * 1000;
    if (windowMs <= 0) {
        return false;
    }
    const int decaySteps = int(elapsedMs / windowMs);
    if (decaySteps <= 0) {
        return false;
    }
    state.level = qMax(0, state.level - decaySteps);
    if (state.level == 0) {
        state.failStreak = 0;
        state.lockoutUntilMs = 0;
        state.lastFailureMs = 0;
    } else {
        state.lastFailureMs += qint64(decaySteps) * windowMs;
    }
    return true;
}

void SessionUnlockThrottle::recordFailure(SessionUnlockThrottleState &state, qint64 nowMs)
{
    applyDecay(state, nowMs);
    state.failStreak += 1;
    state.lastFailureMs = nowMs;
    if (state.failStreak >= 5) {
        state.failStreak = 0;
        state.level = qMin(state.level + 1, 5);
        const int seconds = lockoutSecondsForLevel(state.level);
        state.lockoutUntilMs = nowMs + qint64(seconds) * 1000;
    }
}

void SessionUnlockThrottle::recordSuccess(SessionUnlockThrottleState &state)
{
    state.failStreak = 0;
    state.level = 0;
    state.lockoutUntilMs = 0;
    state.lastFailureMs = 0;
}

bool SessionUnlockThrottle::load(const QString &path,
                                 QHash<QString, SessionUnlockThrottleState> &states,
                                 qint64 nowMs,
                                 bool *changed)
{
    if (changed) {
        *changed = false;
    }
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const QJsonObject obj = it.value().toObject();
        SessionUnlockThrottleState state;
        state.failStreak = obj.value(QStringLiteral("failStreak")).toInt(0);
        state.level = obj.value(QStringLiteral("level")).toInt(0);
        state.lockoutUntilMs = qint64(obj.value(QStringLiteral("lockoutUntilMs")).toDouble(0));
        state.lastFailureMs = qint64(obj.value(QStringLiteral("lastFailureMs")).toDouble(0));
        if (applyDecay(state, nowMs) && changed) {
            *changed = true;
        }
        if (state.failStreak <= 0 && state.level <= 0 && state.lockoutUntilMs <= nowMs) {
            continue;
        }
        states.insert(it.key(), state);
    }
    return true;
}

bool SessionUnlockThrottle::save(const QString &path,
                                 const QHash<QString, SessionUnlockThrottleState> &states)
{
    QFileInfo fi(path);
    QDir dir(fi.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonObject root;
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        const SessionUnlockThrottleState &state = it.value();
        QJsonObject obj;
        obj.insert(QStringLiteral("failStreak"), state.failStreak);
        obj.insert(QStringLiteral("level"), state.level);
        obj.insert(QStringLiteral("lockoutUntilMs"), double(state.lockoutUntilMs));
        obj.insert(QStringLiteral("lastFailureMs"), double(state.lastFailureMs));
        root.insert(it.key(), obj);
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return out.commit();
}

} // namespace Slm::Desktopd

