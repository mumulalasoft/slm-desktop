#include "sshadapter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

SshAdapter::SshAdapter(QObject *parent)
    : ISharingAdapter(parent)
{}

ISharingAdapter::Status SshAdapter::probe()
{
    if (!sshdAvailable())
        return Status::Unavailable;
    return Status::Available;
}

bool SshAdapter::activate()
{
    if (!sshdAvailable())
        return false;
    const bool ok = sshdRunning() || startSshd();
    m_active = ok;
    if (ok)
        emit statusChanged(Status::Available);
    return ok;
}

bool SshAdapter::deactivate()
{
    stopSshd();
    m_active = false;
    emit statusChanged(Status::Unavailable);
    return true;
}

bool SshAdapter::recover()
{
    emit capabilityEvent(QStringLiteral("recovery-suggestion"), {
        {QStringLiteral("issue"), QStringLiteral("sshd-not-found")},
        {QStringLiteral("package"), QStringLiteral("openssh-server")},
        {QStringLiteral("message"), QStringLiteral("Remote access requires an SSH server.")},
    });
    return false;
}

QVariantMap SshAdapter::statusDetail() const
{
    return {
        {QStringLiteral("sshdAvailable"), sshdAvailable()},
        {QStringLiteral("sshdRunning"), sshdRunning()},
        {QStringLiteral("active"), m_active},
    };
}

bool SshAdapter::addAuthorizedKey(const QString &publicKey, const QString &deviceLabel)
{
    const QString path = authorizedKeysPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << publicKey.trimmed() << QLatin1Char(' ')
        << commentForLabel(deviceLabel) << QLatin1Char('\n');
    return true;
}

bool SshAdapter::removeAuthorizedKey(const QString &deviceLabel)
{
    const QString path = authorizedKeysPath();
    QFile f(path);
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QString marker = commentForLabel(deviceLabel);
    QStringList kept;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.contains(marker))
            kept.append(line);
    }
    f.close();

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream out(&f);
    for (const auto &line : std::as_const(kept))
        out << line << QLatin1Char('\n');
    return true;
}

QVariantList SshAdapter::listAuthorizedDevices() const
{
    QVariantList result;
    QFile f(authorizedKeysPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const auto parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString comment = parts.size() >= 3 ? parts.last() : QString();
        result.append(QVariantMap{
            {QStringLiteral("keyType"), parts.value(0)},
            {QStringLiteral("label"), comment.startsWith(QLatin1String("slm-device:"))
                 ? comment.mid(11)
                 : comment},
        });
    }
    return result;
}

bool SshAdapter::sshdAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("sshd")).isEmpty();
}

bool SshAdapter::sshdRunning() const
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("is-active"), QStringLiteral("sshd")});
    p.waitForFinished(2000);
    return p.readAllStandardOutput().trimmed() == "active";
}

bool SshAdapter::startSshd() const
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("sshd")});
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

bool SshAdapter::stopSshd() const
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("stop"), QStringLiteral("sshd")});
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

QString SshAdapter::authorizedKeysPath() const
{
    return QDir::homePath() + QLatin1String("/.ssh/authorized_keys");
}

QString SshAdapter::commentForLabel(const QString &label) const
{
    return QLatin1String("slm-device:") + label.simplified().replace(QLatin1Char(' '), QLatin1Char('-'));
}
