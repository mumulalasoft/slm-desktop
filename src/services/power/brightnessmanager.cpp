#include "brightnessmanager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
int parsePercent(const QString &text)
{
    static const QRegularExpression kPercentRe(QStringLiteral(R"((\d{1,3})\s*%)"));
    const QRegularExpressionMatch match = kPercentRe.match(text);
    if (!match.hasMatch()) {
        return -1;
    }
    bool ok = false;
    const int value = match.captured(1).toInt(&ok);
    if (!ok) {
        return -1;
    }
    return qBound(0, value, 100);
}
}

BrightnessManager::BrightnessManager(QObject *parent)
    : QObject(parent)
{
    m_hasBrightnessctl = !QStandardPaths::findExecutable(QStringLiteral("brightnessctl")).isEmpty();
    m_hasLight = !QStandardPaths::findExecutable(QStringLiteral("light")).isEmpty();

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &BrightnessManager::refresh);
    m_timer->start();

    refresh();
}

bool BrightnessManager::available() const
{
    return m_available;
}

int BrightnessManager::brightness() const
{
    return m_brightness;
}

QString BrightnessManager::iconName() const
{
    return m_iconName;
}

QString BrightnessManager::statusText() const
{
    return m_statusText;
}

QString BrightnessManager::runProgram(const QString &program,
                                      const QStringList &args,
                                      int timeoutMs) const
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs) || proc.exitCode() != 0) {
        return QString();
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

bool BrightnessManager::refreshWithBrightnessctl()
{
    QString output = runProgram(QStringLiteral("brightnessctl"),
                                {QStringLiteral("--class=backlight"), QStringLiteral("-m")});
    if (output.isEmpty()) {
        output = runProgram(QStringLiteral("brightnessctl"),
                            {QStringLiteral("-c"), QStringLiteral("backlight"), QStringLiteral("-m")});
    }
    if (output.isEmpty()) {
        output = runProgram(QStringLiteral("brightnessctl"), {QStringLiteral("-m")});
    }
    return parseBrightnessctlOutput(output);
}

bool BrightnessManager::parseBrightnessctlOutput(const QString &output)
{
    if (output.isEmpty()) {
        return false;
    }

    // brightnessctl -m format:
    // device,class,current,max,percent
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
        const QString klass = parts.at(1).trimmed().toLower();
        if (klass == QStringLiteral("backlight")) {
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

    m_preferredBacklightDevice = chosenParts.at(0).trimmed();
    m_available = true;
    m_brightness = value;
    m_statusText = QStringLiteral("%1%").arg(m_brightness);
    return true;
}

bool BrightnessManager::refreshWithLight()
{
    const QString output = runProgram(QStringLiteral("light"), {QStringLiteral("-G")});
    if (output.isEmpty()) {
        return false;
    }
    bool ok = false;
    const double raw = output.toDouble(&ok);
    if (!ok) {
        return false;
    }
    m_available = true;
    m_brightness = qBound(0, qRound(raw), 100);
    m_statusText = QStringLiteral("%1%").arg(m_brightness);
    return true;
}

void BrightnessManager::refresh()
{
    const bool oldAvailable = m_available;
    const int oldBrightness = m_brightness;
    const QString oldStatus = m_statusText;
    const QString oldIcon = m_iconName;

    m_available = false;

    bool ok = false;
    if (m_hasBrightnessctl) {
        ok = refreshWithBrightnessctl();
    }
    if (!ok && m_hasLight) {
        ok = refreshWithLight();
    }

    if (!ok) {
        m_available = false;
        m_brightness = 0;
        m_statusText = QStringLiteral("Display unavailable");
    }

    m_iconName = QStringLiteral("display-brightness-symbolic");

    if (oldAvailable != m_available ||
        oldBrightness != m_brightness ||
        oldStatus != m_statusText ||
        oldIcon != m_iconName) {
        emit changed();
    }
}

bool BrightnessManager::setBrightness(int brightness)
{
    const int clamped = qBound(1, brightness, 100);
    bool success = false;

    if (m_hasBrightnessctl) {
        QProcess proc;
        proc.start(QStringLiteral("brightnessctl"),
                   {QStringLiteral("--class=backlight"),
                    QStringLiteral("set"),
                    QStringLiteral("%1%").arg(clamped)});
        success = proc.waitForFinished(1500) && proc.exitCode() == 0;

        if (!success) {
            proc.start(QStringLiteral("brightnessctl"),
                       {QStringLiteral("-c"),
                        QStringLiteral("backlight"),
                        QStringLiteral("set"),
                        QStringLiteral("%1%").arg(clamped)});
            success = proc.waitForFinished(1500) && proc.exitCode() == 0;
        }

        if (!success && !m_preferredBacklightDevice.trimmed().isEmpty()) {
            proc.start(QStringLiteral("brightnessctl"),
                       {QStringLiteral("--device"),
                        m_preferredBacklightDevice,
                        QStringLiteral("set"),
                        QStringLiteral("%1%").arg(clamped)});
            success = proc.waitForFinished(1500) && proc.exitCode() == 0;
        }

        if (!success) {
            proc.start(QStringLiteral("brightnessctl"),
                       {QStringLiteral("set"),
                        QStringLiteral("%1%").arg(clamped)});
            success = proc.waitForFinished(1500) && proc.exitCode() == 0;
        }
    }

    if (!success && m_hasLight) {
        QProcess proc;
        proc.start(QStringLiteral("light"),
                   {QStringLiteral("-S"), QString::number(clamped)});
        success = proc.waitForFinished(1500) && proc.exitCode() == 0;
    }

    if (!success) {
        return false;
    }

    refresh();
    return true;
}
