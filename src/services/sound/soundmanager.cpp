#include "soundmanager.h"

#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QVector>
#include <algorithm>

namespace {

bool hasExecutable(const QString &name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

QString runCommand(const QString &program, const QStringList &args, int timeoutMs = 1200)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs) || proc.exitCode() != 0) {
        return QString();
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QString normalizeWpctlStatusLine(QString line)
{
    // Strip box-drawing characters used by modern wpctl status output.
    line.remove(QRegularExpression(QStringLiteral("[\\x{2500}-\\x{257F}]")));
    return line.trimmed();
}

} // namespace

SoundManager::SoundManager(QObject *parent)
    : QObject(parent)
{
    m_hasWpctl = hasExecutable(QStringLiteral("wpctl"));
    m_hasPactl = hasExecutable(QStringLiteral("pactl"));
    m_timer = new QTimer(this);
    m_timer->setInterval(4000);
    connect(m_timer, &QTimer::timeout, this, &SoundManager::refresh);
    m_timer->start();
    refresh();
}

bool SoundManager::available() const
{
    return m_available;
}

bool SoundManager::muted() const
{
    return m_muted;
}

int SoundManager::volume() const
{
    return m_volume;
}

QString SoundManager::iconName() const
{
    return m_iconName;
}

QString SoundManager::statusText() const
{
    return m_statusText;
}

QString SoundManager::currentSink() const
{
    return m_currentSink;
}

QVariantList SoundManager::sinks() const
{
    return m_sinks;
}

QVariantList SoundManager::streams() const
{
    return m_streams;
}

bool SoundManager::hasWpctl() const
{
    return m_hasWpctl;
}

QString SoundManager::runWpctl(const QStringList &args, int timeoutMs) const
{
    if (!m_hasWpctl) {
        return QString();
    }
    return runCommand(QStringLiteral("wpctl"), args, timeoutMs);
}

QVariantMap SoundManager::inspectNode(const QString &id) const
{
    QVariantMap out;
    const QString target = id.trimmed();
    if (target.isEmpty() || !hasWpctl()) {
        return out;
    }
    const QString volText = runWpctl({QStringLiteral("get-volume"), target});
    const QString muteText = runWpctl({QStringLiteral("get-volume"), target});
    if (!volText.isEmpty()) {
        out.insert(QStringLiteral("volume"), qBound(0, parseVolumePercent(volText), 150));
        out.insert(QStringLiteral("muted"), parseMuted(muteText));
        return out;
    }
    const QString inspectText = runWpctl({QStringLiteral("inspect"), target});
    if (inspectText.isEmpty()) {
        return out;
    }
    out.insert(QStringLiteral("volume"), qBound(0, parseVolumePercent(inspectText), 150));
    out.insert(QStringLiteral("muted"), parseMuted(inspectText));
    return out;
}

QString SoundManager::detectDefaultSink() const
{
    if (hasWpctl()) {
        const QString status = runWpctl({QStringLiteral("status"), QStringLiteral("-n")});
        if (status.isEmpty()) {
            return QString();
        }
        const QStringList lines = status.split('\n');
        static const QRegularExpression sinkRe(QStringLiteral("^\\*?\\s*(\\d+)\\.\\s+(.+)$"));
        bool inSinks = false;
        for (const QString &raw : lines) {
            const QString trimmed = normalizeWpctlStatusLine(raw);
            if (trimmed.startsWith(QStringLiteral("Sinks:"), Qt::CaseInsensitive)) {
                inSinks = true;
                continue;
            }
            if (inSinks && !trimmed.isEmpty() &&
                !trimmed.startsWith('*') &&
                !trimmed.at(0).isDigit()) {
                inSinks = false;
            }
            if (!inSinks || trimmed.isEmpty() || !trimmed.contains('*')) {
                continue;
            }
            const QRegularExpressionMatch m = sinkRe.match(trimmed);
            if (m.hasMatch()) {
                return m.captured(1).trimmed();
            }
        }
        // Fallback token works with wpctl get/set-volume on PipeWire.
        return QStringLiteral("@DEFAULT_AUDIO_SINK@");
    }

    if (!m_hasPactl) {
        return QString();
    }

    const QString direct = runCommand(QStringLiteral("pactl"),
                                      {QStringLiteral("get-default-sink")}, 1000).trimmed();
    if (!direct.isEmpty()) {
        return direct;
    }

    const QString info = runCommand(QStringLiteral("pactl"), {QStringLiteral("info")}, 1000);
    const QStringList lines = info.split('\n');
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("Default Sink:"), Qt::CaseInsensitive)) {
            const QString detected = line.section(':', 1).trimmed();
            if (!detected.isEmpty()) {
                return detected;
            }
        }
    }
    return QStringLiteral("@DEFAULT_SINK@");
}

int SoundManager::parseVolumePercent(const QString &text) const
{
    static const QRegularExpression wpFloatRe(QStringLiteral("(?:Volume:|\\[vol:)\\s*([0-9]*\\.?[0-9]+)"));
    QRegularExpressionMatch match = wpFloatRe.match(text);
    if (match.hasMatch()) {
        const double value = match.captured(1).toDouble();
        if (value >= 0.0) {
            return qRound(value * 100.0);
        }
    }
    static const QRegularExpression wpRe(QStringLiteral("\\[(\\d+)%\\]"));
    match = wpRe.match(text);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    static const QRegularExpression genericRe(QStringLiteral("(\\d+)%"));
    match = genericRe.match(text);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    return 0;
}

bool SoundManager::parseMuted(const QString &text) const
{
    return text.contains(QStringLiteral("Muted: true"), Qt::CaseInsensitive) ||
           text.contains(QStringLiteral("mute: true"), Qt::CaseInsensitive) ||
           text.contains(QStringLiteral("[MUTED]"), Qt::CaseInsensitive) ||
           text.contains(QStringLiteral(" yes"), Qt::CaseInsensitive);
}

QString SoundManager::computeIconName() const
{
    if (m_muted || m_volume <= 0) {
        return QStringLiteral("audio-volume-muted-symbolic");
    }
    if (m_volume < 35) {
        return QStringLiteral("audio-volume-low-symbolic");
    }
    if (m_volume < 70) {
        return QStringLiteral("audio-volume-medium-symbolic");
    }
    return QStringLiteral("audio-volume-high-symbolic");
}

QVariantList SoundManager::querySinks() const
{
    QVariantList out;

    if (hasWpctl()) {
        const QString status = runWpctl({QStringLiteral("status"), QStringLiteral("-n")});
        if (status.isEmpty()) {
            return out;
        }
        const QStringList lines = status.split('\n');
        static const QRegularExpression sinkRe(QStringLiteral("^\\*?\\s*(\\d+)\\.\\s+(.+)$"));
        bool inSinks = false;
        for (const QString &rawLine : lines) {
            const QString trimmed = normalizeWpctlStatusLine(rawLine);
            if (trimmed.startsWith(QStringLiteral("Sinks:"), Qt::CaseInsensitive)) {
                inSinks = true;
                continue;
            }
            if (inSinks && !trimmed.isEmpty() &&
                !trimmed.startsWith('*') &&
                !trimmed.at(0).isDigit()) {
                inSinks = false;
            }
            if (!inSinks || trimmed.isEmpty()) {
                continue;
            }
            const QRegularExpressionMatch m = sinkRe.match(trimmed);
            if (!m.hasMatch()) {
                continue;
            }
            const QString id = m.captured(1).trimmed();
            QString label = m.captured(2).trimmed();
            const int volPos = label.indexOf(QStringLiteral("[vol:"), 0, Qt::CaseInsensitive);
            if (volPos >= 0) {
                label = label.left(volPos).trimmed();
            }
            if (label.isEmpty()) {
                label = QStringLiteral("Sink %1").arg(id);
            }
            QVariantMap item;
            item.insert(QStringLiteral("name"), id);
            item.insert(QStringLiteral("description"), label);
            out.push_back(item);
        }
        return out;
    }

    if (!m_hasPactl) {
        return out;
    }

    const QString text = runCommand(QStringLiteral("pactl"),
                                    {QStringLiteral("list"), QStringLiteral("sinks")});
    if (text.isEmpty()) {
        return out;
    }
    const QStringList lines = text.split('\n');
    QString currentName;
    QString currentDescription;
    const auto flush = [&]() {
        if (currentName.isEmpty()) {
            return;
        }
        QVariantMap item;
        item.insert(QStringLiteral("name"), currentName);
        item.insert(QStringLiteral("description"),
                    currentDescription.isEmpty() ? currentName : currentDescription);
        out << item;
    };
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("Sink #"))) {
            flush();
            currentName.clear();
            currentDescription.clear();
            continue;
        }
        if (line.startsWith(QStringLiteral("Name:"))) {
            currentName = line.mid(5).trimmed();
            continue;
        }
        if (line.startsWith(QStringLiteral("Description:"))) {
            currentDescription = line.mid(12).trimmed();
            continue;
        }
    }
    flush();
    return out;
}

QVariantList SoundManager::queryStreams()
{
    QVariantList out;
    QVector<QVariantMap> parsed;

    if (hasWpctl()) {
        const QString status = runWpctl({QStringLiteral("status"), QStringLiteral("-n")});
        if (status.isEmpty()) {
            return out;
        }
        const QStringList lines = status.split('\n');
        static const QRegularExpression nodeRe(QStringLiteral("^\\*?\\s*(\\d+)\\.\\s+(.+)$"));
        bool inStreams = false;
        for (const QString &rawLine : lines) {
            const QString trimmed = normalizeWpctlStatusLine(rawLine);
            if (trimmed.startsWith(QStringLiteral("Streams:"), Qt::CaseInsensitive)) {
                inStreams = true;
                continue;
            }
            if (inStreams && !trimmed.isEmpty() &&
                !trimmed.startsWith('*') &&
                !trimmed.at(0).isDigit()) {
                inStreams = false;
            }
            if (!inStreams || trimmed.isEmpty()) {
                continue;
            }
            const QRegularExpressionMatch m = nodeRe.match(trimmed);
            if (!m.hasMatch()) {
                continue;
            }
            const uint id = m.captured(1).toUInt();
            QString label = m.captured(2).trimmed();
            const int volPos = label.indexOf(QStringLiteral("[vol:"), 0, Qt::CaseInsensitive);
            if (volPos >= 0) {
                label = label.left(volPos).trimmed();
            }
            if (label.isEmpty()) {
                label = QStringLiteral("Application %1").arg(id);
            }
            const int streamVolume = qBound(0, parseVolumePercent(trimmed), 150);
            const bool streamMuted = parseMuted(trimmed);
            const bool activeNow = !streamMuted;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (activeNow) {
                m_streamLastActiveMs.insert(id, nowMs);
            } else if (!m_streamLastActiveMs.contains(id)) {
                m_streamLastActiveMs.insert(id, 0);
            }
            QVariantMap item;
            item.insert(QStringLiteral("id"), id);
            item.insert(QStringLiteral("name"), label);
            item.insert(QStringLiteral("volume"), streamVolume);
            item.insert(QStringLiteral("muted"), streamMuted);
            item.insert(QStringLiteral("active"), activeNow);
            item.insert(QStringLiteral("_lastActive"), m_streamLastActiveMs.value(id));
            parsed.push_back(item);
        }
    } else {
        if (!m_hasPactl) {
            return out;
        }
        const QString text = runCommand(QStringLiteral("pactl"),
                                        {QStringLiteral("list"), QStringLiteral("sink-inputs")});
        if (text.isEmpty()) {
            return out;
        }
        const QStringList lines = text.split('\n');
        uint currentId = 0;
        QString applicationName;
        QString mediaName;
        QString processBinary;
        int volume = 0;
        bool muted = false;
        bool running = false;
        bool corked = false;
        QSet<uint> seenIds;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        const auto parseQuotedValue = [](const QString &line) -> QString {
            const int firstQuote = line.indexOf('"');
            const int lastQuote = line.lastIndexOf('"');
            if (firstQuote < 0 || lastQuote <= firstQuote) {
                return QString();
            }
            return line.mid(firstQuote + 1, lastQuote - firstQuote - 1).trimmed();
        };

        const auto flush = [&]() {
            if (currentId == 0) {
                return;
            }
            QString label = mediaName.trimmed();
            if (label.isEmpty()) {
                label = applicationName.trimmed();
            }
            if (label.isEmpty()) {
                label = processBinary.trimmed();
            }
            if (label.isEmpty()) {
                label = QStringLiteral("Application %1").arg(currentId);
            }
            QVariantMap item;
            item.insert(QStringLiteral("id"), currentId);
            item.insert(QStringLiteral("name"), label);
            item.insert(QStringLiteral("volume"), qBound(0, volume, 150));
            item.insert(QStringLiteral("muted"), muted);
            const bool activeNow = running || !corked;
            item.insert(QStringLiteral("active"), activeNow);
            if (activeNow) {
                m_streamLastActiveMs.insert(currentId, nowMs);
            } else if (!m_streamLastActiveMs.contains(currentId)) {
                m_streamLastActiveMs.insert(currentId, 0);
            }
            item.insert(QStringLiteral("_lastActive"), m_streamLastActiveMs.value(currentId));
            seenIds.insert(currentId);
            parsed.push_back(item);
        };

        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (line.startsWith(QStringLiteral("Sink Input #"))) {
                flush();
                currentId = line.mid(QStringLiteral("Sink Input #").size()).trimmed().toUInt();
                applicationName.clear();
                mediaName.clear();
                processBinary.clear();
                volume = 0;
                muted = false;
                running = false;
                corked = false;
                continue;
            }
            if (line.startsWith(QStringLiteral("Mute:"))) {
                muted = parseMuted(line);
                continue;
            }
            if (line.startsWith(QStringLiteral("Volume:"))) {
                volume = parseVolumePercent(line);
                continue;
            }
            if (line.startsWith(QStringLiteral("application.name ="))) {
                applicationName = parseQuotedValue(line);
                continue;
            }
            if (line.startsWith(QStringLiteral("media.name ="))) {
                mediaName = parseQuotedValue(line);
                continue;
            }
            if (line.startsWith(QStringLiteral("application.process.binary ="))) {
                processBinary = parseQuotedValue(line);
                continue;
            }
            if (line.startsWith(QStringLiteral("State:"))) {
                running = line.contains(QStringLiteral("RUNNING"), Qt::CaseInsensitive);
                continue;
            }
            if (line.startsWith(QStringLiteral("Corked:"))) {
                corked = parseMuted(line);
                continue;
            }
        }
        flush();

        QList<uint> trackedIds = m_streamLastActiveMs.keys();
        for (const uint trackedId : trackedIds) {
            if (!seenIds.contains(trackedId)) {
                m_streamLastActiveMs.remove(trackedId);
            }
        }
    }

    std::sort(parsed.begin(), parsed.end(), [](const QVariantMap &a, const QVariantMap &b) {
        const qint64 aTs = a.value(QStringLiteral("_lastActive")).toLongLong();
        const qint64 bTs = b.value(QStringLiteral("_lastActive")).toLongLong();
        if (aTs != bTs) {
            return aTs > bTs;
        }
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });

    for (QVariantMap row : std::as_const(parsed)) {
        row.remove(QStringLiteral("_lastActive"));
        out.push_back(row);
    }
    return out;
}

void SoundManager::refresh()
{
    const bool oldAvailable = m_available;
    const bool oldMuted = m_muted;
    const int oldVolume = m_volume;
    const QString oldIcon = m_iconName;
    const QString oldStatus = m_statusText;
    const QString oldSink = m_currentSink;
    const QVariantList oldSinks = m_sinks;
    const QVariantList oldStreams = m_streams;

    const bool wp = m_hasWpctl;
    const bool pactl = m_hasPactl;
    if (!wp && !pactl) {
        m_available = false;
        m_muted = false;
        m_volume = 0;
        m_iconName = QStringLiteral("audio-volume-muted-symbolic");
        m_statusText = QStringLiteral("Audio backend unavailable");
        m_currentSink.clear();
        m_sinks.clear();
        m_streams.clear();
    } else {
        m_sinks = querySinks();
        m_streams = queryStreams();
        m_currentSink = detectDefaultSink();
        if (m_currentSink.isEmpty()) {
            m_available = false;
            m_muted = false;
            m_volume = 0;
            m_iconName = QStringLiteral("audio-volume-muted-symbolic");
            m_statusText = QStringLiteral("Sound unavailable");
        } else if (wp) {
            const QVariantMap state = inspectNode(m_currentSink);
            m_volume = qBound(0, state.value(QStringLiteral("volume"), 0).toInt(), 150);
            m_muted = state.value(QStringLiteral("muted"), false).toBool();
            m_available = true;
            m_iconName = computeIconName();
            m_statusText = m_muted ? QStringLiteral("Muted")
                                   : QStringLiteral("%1%").arg(m_volume);
        } else {
            const QString volText = runCommand(QStringLiteral("pactl"),
                                               {QStringLiteral("get-sink-volume"), m_currentSink});
            const QString muteText = runCommand(QStringLiteral("pactl"),
                                                {QStringLiteral("get-sink-mute"), m_currentSink});
            m_volume = qBound(0, parseVolumePercent(volText), 150);
            m_muted = parseMuted(muteText);
            m_available = true;
            m_iconName = computeIconName();
            m_statusText = m_muted ? QStringLiteral("Muted")
                                   : QStringLiteral("%1%").arg(m_volume);
        }
    }

    if (oldAvailable != m_available ||
        oldMuted != m_muted ||
        oldVolume != m_volume ||
        oldIcon != m_iconName ||
        oldStatus != m_statusText ||
        oldSink != m_currentSink ||
        oldSinks != m_sinks ||
        oldStreams != m_streams) {
        emit changed();
    }
}

bool SoundManager::setMuted(bool muted)
{
    const QString sink = detectDefaultSink();
    if (sink.isEmpty()) {
        return false;
    }
    QString out;
    if (hasWpctl()) {
        out = runWpctl({QStringLiteral("set-mute"), sink, muted ? QStringLiteral("1") : QStringLiteral("0")});
    } else {
        out = runCommand(QStringLiteral("pactl"),
                         {QStringLiteral("set-sink-mute"), sink,
                          muted ? QStringLiteral("1") : QStringLiteral("0")});
    }
    if (out.isNull()) {
        return false;
    }
    refresh();
    return true;
}

bool SoundManager::setVolume(int volume)
{
    const QString sink = detectDefaultSink();
    if (sink.isEmpty()) {
        return false;
    }
    const int clamped = qBound(0, volume, 150);
    QString out;
    if (hasWpctl()) {
        out = runWpctl({QStringLiteral("set-volume"), sink, QStringLiteral("%1%").arg(clamped)});
    } else {
        out = runCommand(QStringLiteral("pactl"),
                         {QStringLiteral("set-sink-volume"), sink,
                          QStringLiteral("%1%").arg(clamped)});
    }
    if (out.isNull()) {
        return false;
    }
    refresh();
    return true;
}

bool SoundManager::setDefaultSink(const QString &sinkName)
{
    const QString target = sinkName.trimmed();
    if (target.isEmpty()) {
        return false;
    }
    QString out;
    if (hasWpctl()) {
        out = runWpctl({QStringLiteral("set-default"), target});
    } else {
        out = runCommand(QStringLiteral("pactl"),
                         {QStringLiteral("set-default-sink"), target});
    }
    if (out.isNull()) {
        return false;
    }
    refresh();
    return true;
}

bool SoundManager::setStreamVolume(uint streamId, int volume)
{
    if (streamId == 0) {
        return false;
    }
    const int clamped = qBound(0, volume, 150);
    QString out;
    if (hasWpctl()) {
        out = runWpctl({QStringLiteral("set-volume"),
                        QString::number(streamId),
                        QStringLiteral("%1%").arg(clamped)});
    } else {
        out = runCommand(QStringLiteral("pactl"),
                         {QStringLiteral("set-sink-input-volume"),
                          QString::number(streamId),
                          QStringLiteral("%1%").arg(clamped)});
    }
    if (out.isNull()) {
        return false;
    }
    refresh();
    return true;
}

bool SoundManager::setStreamMuted(uint streamId, bool muted)
{
    if (streamId == 0) {
        return false;
    }
    QString out;
    if (hasWpctl()) {
        out = runWpctl({QStringLiteral("set-mute"),
                        QString::number(streamId),
                        muted ? QStringLiteral("1") : QStringLiteral("0")});
    } else {
        out = runCommand(QStringLiteral("pactl"),
                         {QStringLiteral("set-sink-input-mute"),
                          QString::number(streamId),
                          muted ? QStringLiteral("1") : QStringLiteral("0")});
    }
    if (out.isNull()) {
        return false;
    }
    refresh();
    return true;
}

bool SoundManager::openSoundSettings()
{
    const QList<QStringList> commands = {
        {QStringLiteral("gnome-control-center"), QStringLiteral("sound")},
        {QStringLiteral("kcmshell6"), QStringLiteral("kcm_pulseaudio")},
        {QStringLiteral("pavucontrol")}
    };

    for (const QStringList &cmd : commands) {
        if (cmd.isEmpty()) {
            continue;
        }
        QString program = cmd.first();
        QStringList args = cmd;
        args.removeFirst();
        if (QProcess::startDetached(program, args)) {
            return true;
        }
    }
    return false;
}
