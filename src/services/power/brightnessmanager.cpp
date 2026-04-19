#include "brightnessmanager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

namespace {

struct BrightnessSnapshot {
    bool available = false;
    int brightness = 0;
    QString statusText;
    QString iconName = QStringLiteral("display-brightness-symbolic");
    QString preferredDevice;
};

int parsePercent(const QString &text)
{
    static const QRegularExpression kPercentRe(QStringLiteral(R"((\d{1,3})\s*%)"));
    const QRegularExpressionMatch match = kPercentRe.match(text);
    if (!match.hasMatch()) {
        return -1;
    }
    bool ok = false;
    const int value = match.captured(1).toInt(&ok);
    return ok ? qBound(0, value, 100) : -1;
}

QString runBrightnessProgram(const QString &program, const QStringList &args, int timeoutMs = 1200)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs) || proc.exitCode() != 0) {
        return QString();
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

bool parseBrightnessctlOutput(const QString &output, BrightnessSnapshot &snap)
{
    if (output.isEmpty()) {
        return false;
    }
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return false;
    }
    QStringList chosenParts;
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char(','));
        if (parts.size() < 5) {
            continue;
        }
        if (parts.at(1).trimmed().toLower() == QStringLiteral("backlight")) {
            chosenParts = parts;
            break;
        }
    }
    if (chosenParts.isEmpty()) {
        const QStringList parts = lines.first().split(QLatin1Char(','));
        if (parts.size() < 5) {
            return false;
        }
        chosenParts = parts;
    }
    const int value = parsePercent(chosenParts.at(4));
    if (value < 0) {
        return false;
    }
    snap.preferredDevice = chosenParts.at(0).trimmed();
    snap.available       = true;
    snap.brightness      = value;
    snap.statusText      = QStringLiteral("%1%").arg(value);
    return true;
}

BrightnessSnapshot collectBrightnessSnapshot(bool hasBrightnessctl, bool hasLight)
{
    BrightnessSnapshot snap;
    snap.iconName = QStringLiteral("display-brightness-symbolic");

    if (hasBrightnessctl) {
        QString out = runBrightnessProgram(QStringLiteral("brightnessctl"),
                                           {QStringLiteral("--class=backlight"), QStringLiteral("-m")});
        if (out.isEmpty()) {
            out = runBrightnessProgram(QStringLiteral("brightnessctl"),
                                       {QStringLiteral("-c"), QStringLiteral("backlight"), QStringLiteral("-m")});
        }
        if (out.isEmpty()) {
            out = runBrightnessProgram(QStringLiteral("brightnessctl"), {QStringLiteral("-m")});
        }
        if (parseBrightnessctlOutput(out, snap)) {
            return snap;
        }
    }

    if (hasLight) {
        const QString out = runBrightnessProgram(QStringLiteral("light"), {QStringLiteral("-G")});
        if (!out.isEmpty()) {
            bool ok = false;
            const double raw = out.toDouble(&ok);
            if (ok) {
                snap.available  = true;
                snap.brightness = qBound(0, qRound(raw), 100);
                snap.statusText = QStringLiteral("%1%").arg(snap.brightness);
                return snap;
            }
        }
    }

    snap.statusText = QStringLiteral("Display unavailable");
    return snap;
}

bool applyBrightness(int clamped, bool hasBrightnessctl, bool hasLight,
                     const QString &preferredDevice)
{
    if (hasBrightnessctl) {
        const QString pct = QStringLiteral("%1%").arg(clamped);
        QProcess proc;

        proc.start(QStringLiteral("brightnessctl"),
                   {QStringLiteral("--class=backlight"), QStringLiteral("set"), pct});
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            return true;
        }

        proc.start(QStringLiteral("brightnessctl"),
                   {QStringLiteral("-c"), QStringLiteral("backlight"), QStringLiteral("set"), pct});
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            return true;
        }

        if (!preferredDevice.trimmed().isEmpty()) {
            proc.start(QStringLiteral("brightnessctl"),
                       {QStringLiteral("--device"), preferredDevice, QStringLiteral("set"), pct});
            if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
                return true;
            }
        }

        proc.start(QStringLiteral("brightnessctl"), {QStringLiteral("set"), pct});
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            return true;
        }
    }

    if (hasLight) {
        QProcess proc;
        proc.start(QStringLiteral("light"), {QStringLiteral("-S"), QString::number(clamped)});
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            return true;
        }
    }

    return false;
}

} // namespace

BrightnessManager::BrightnessManager(QObject *parent)
    : QObject(parent)
{
    m_hasBrightnessctl = !QStandardPaths::findExecutable(QStringLiteral("brightnessctl")).isEmpty();
    m_hasLight         = !QStandardPaths::findExecutable(QStringLiteral("light")).isEmpty();

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &BrightnessManager::refresh);
    m_timer->start();

    refresh();
}

bool BrightnessManager::available() const { return m_available; }
int BrightnessManager::brightness() const { return m_brightness; }
QString BrightnessManager::iconName() const { return m_iconName; }
QString BrightnessManager::statusText() const { return m_statusText; }

void BrightnessManager::refresh()
{
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;

    const bool hasBrightnessctl = m_hasBrightnessctl;
    const bool hasLight         = m_hasLight;

    auto *watcher = new QFutureWatcher<BrightnessSnapshot>(this);
    connect(watcher, &QFutureWatcher<BrightnessSnapshot>::finished, this, [this, watcher]() {
        BrightnessSnapshot snap = watcher->result();
        watcher->deleteLater();
        m_refreshPending = false;

        const bool changed =
            m_available  != snap.available  ||
            m_brightness != snap.brightness ||
            m_statusText != snap.statusText ||
            m_iconName   != snap.iconName;

        m_available             = snap.available;
        m_brightness            = snap.brightness;
        m_statusText            = snap.statusText;
        m_iconName              = snap.iconName;
        m_preferredBacklightDevice = snap.preferredDevice;

        if (changed) {
            emit this->changed();
        }
    });

    watcher->setFuture(QtConcurrent::run([hasBrightnessctl, hasLight]() {
        return collectBrightnessSnapshot(hasBrightnessctl, hasLight);
    }));
}

bool BrightnessManager::setBrightness(int brightness)
{
    m_pendingBrightness = qBound(1, brightness, 100);

    // Optimistic UI update so the slider feels instant
    if (m_brightness != m_pendingBrightness) {
        m_brightness = m_pendingBrightness;
        m_statusText = QStringLiteral("%1%").arg(m_brightness);
        emit changed();
    }

    if (!m_setPending) {
        firePendingSetBrightness();
    }
    return true;
}

void BrightnessManager::firePendingSetBrightness()
{
    m_setPending = true;
    const int target          = m_pendingBrightness;
    const bool hasBrightnessctl = m_hasBrightnessctl;
    const bool hasLight         = m_hasLight;
    const QString preferredDevice = m_preferredBacklightDevice;

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, target]() {
        watcher->deleteLater();
        m_setPending = false;

        if (m_pendingBrightness != target) {
            // A newer value arrived while we were running — fire it
            firePendingSetBrightness();
        } else {
            // Confirm actual hardware value
            refresh();
        }
    });

    watcher->setFuture(QtConcurrent::run([target, hasBrightnessctl, hasLight, preferredDevice]() {
        return applyBrightness(target, hasBrightnessctl, hasLight, preferredDevice);
    }));
}
