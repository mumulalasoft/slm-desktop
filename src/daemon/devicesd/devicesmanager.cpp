#include "devicesmanager.h"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QUrl>

DevicesManager::DevicesManager(QObject *parent)
    : QObject(parent)
{
}

QVariantMap DevicesManager::Mount(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (isUriTarget(t)) {
        return runCommand(QStringLiteral("gio"), {QStringLiteral("mount"), t}, 30000);
    }
    return runCommand(QStringLiteral("udisksctl"),
                      {QStringLiteral("mount"), QStringLiteral("-b"), t},
                      30000);
}

QVariantMap DevicesManager::Eject(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (isUriTarget(t)) {
        return runCommand(QStringLiteral("gio"), {QStringLiteral("mount"), QStringLiteral("-e"), t}, 30000);
    }

    QVariantMap result = runCommand(QStringLiteral("udisksctl"),
                                    {QStringLiteral("unmount"), QStringLiteral("-b"), t},
                                    30000);
    if (!result.value(QStringLiteral("ok")).toBool()) {
        return result;
    }
    return runCommand(QStringLiteral("udisksctl"),
                      {QStringLiteral("power-off"), QStringLiteral("-b"), t},
                      30000);
}

QVariantMap DevicesManager::Unlock(const QString &target, const QString &passphrase)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (passphrase.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-passphrase"));
    }

    QTemporaryFile keyFile;
    keyFile.setAutoRemove(true);
    if (!keyFile.open()) {
        return makeResult(false, QStringLiteral("temp-keyfile-open-failed"));
    }
    keyFile.write(passphrase.toUtf8());
    keyFile.write("\n");
    keyFile.flush();

    return runCommand(QStringLiteral("udisksctl"),
                      {QStringLiteral("unlock"),
                       QStringLiteral("-b"), t,
                       QStringLiteral("--key-file"), keyFile.fileName()},
                      30000);
}

QVariantMap DevicesManager::Format(const QString &target, const QString &filesystem)
{
    const QString t = target.trimmed();
    QString fs = filesystem.trimmed().toLower();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (isUriTarget(t)) {
        return makeResult(false, QStringLiteral("format-uri-not-supported"));
    }
    if (fs.isEmpty()) {
        fs = QStringLiteral("ext4");
    }
    return runCommand(QStringLiteral("udisksctl"),
                      {QStringLiteral("format"),
                       QStringLiteral("-b"), t,
                       QStringLiteral("--type"), fs},
                      120000);
}

QVariantMap DevicesManager::makeResult(bool ok,
                                       const QString &error,
                                       const QString &stdoutText,
                                       const QString &stderrText)
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), ok);
    out.insert(QStringLiteral("error"), error);
    out.insert(QStringLiteral("stdout"), stdoutText);
    out.insert(QStringLiteral("stderr"), stderrText);
    return out;
}

bool DevicesManager::isUriTarget(const QString &target)
{
    const QUrl url(target);
    return url.isValid() && !url.scheme().trimmed().isEmpty() && target.contains(QStringLiteral("://"));
}

QVariantMap DevicesManager::runCommand(const QString &program,
                                       const QStringList &args,
                                       int timeoutMs)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(5000)) {
        return makeResult(false, QStringLiteral("start-failed"));
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        return makeResult(false, QStringLiteral("timeout"),
                          QString::fromUtf8(p.readAllStandardOutput()).trimmed(),
                          QString::fromUtf8(p.readAllStandardError()).trimmed());
    }

    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return makeResult(false,
                          err.isEmpty() ? QStringLiteral("command-failed") : err,
                          out,
                          err);
    }
    return makeResult(true, QString(), out, err);
}

