#include "sambaadapter.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

SambaAdapter::SambaAdapter(QObject *parent)
    : ISharingAdapter(parent)
{}

ISharingAdapter::Status SambaAdapter::probe()
{
    if (!smbNetAvailable())
        return Status::Unavailable;
    if (!usersharePermission())
        return Status::Degraded;
    return Status::Available;
}

bool SambaAdapter::activate()
{
    if (probe() == Status::Unavailable)
        return false;
    m_active = true;
    emit statusChanged(Status::Available);
    return true;
}

bool SambaAdapter::deactivate()
{
    m_active = false;
    emit statusChanged(Status::Unavailable);
    return true;
}

bool SambaAdapter::recover()
{
    // Suggest installing samba package via slm-package-policy-service
    emit capabilityEvent(QStringLiteral("recovery-suggestion"), {
        {QStringLiteral("issue"), QStringLiteral("samba-not-found")},
        {QStringLiteral("package"), QStringLiteral("samba")},
        {QStringLiteral("message"), QStringLiteral("File sharing requires Samba to be installed.")},
    });
    return false;
}

QVariantMap SambaAdapter::statusDetail() const
{
    return {
        {QStringLiteral("smbNetAvailable"), smbNetAvailable()},
        {QStringLiteral("usersharePermission"), usersharePermission()},
        {QStringLiteral("active"), m_active},
    };
}

bool SambaAdapter::configureShare(const QString &path, const QVariantMap &options)
{
    if (!m_active)
        return false;

    const QString shareName = sanitizeName(path);
    const QString accessMode = options.value(QStringLiteral("access"), QStringLiteral("ro")).toString();
    const bool guestAccess = options.value(QStringLiteral("guestAccess"), false).toBool();
    const QString acl = guestAccess
        ? QStringLiteral("Everyone:R")
        : (accessMode == QLatin1String("rw") ? QStringLiteral("Everyone:F") : QStringLiteral("Everyone:R"));

    return runUsershareCommand({
        QStringLiteral("usershare"),
        QStringLiteral("add"),
        shareName,
        path,
        QString(),
        acl,
        guestAccess ? QStringLiteral("guest_ok=y") : QStringLiteral("guest_ok=n"),
    });
}

bool SambaAdapter::removeShare(const QString &path)
{
    if (!m_active)
        return false;
    return runUsershareCommand({
        QStringLiteral("usershare"),
        QStringLiteral("delete"),
        sanitizeName(path),
    });
}

QVariantList SambaAdapter::listShares() const
{
    QVariantList result;
    if (!m_active)
        return result;

    QProcess p;
    p.start(QStringLiteral("net"), {QStringLiteral("usershare"), QStringLiteral("list")});
    if (!p.waitForFinished(3000))
        return result;

    const auto lines = QString::fromLocal8Bit(p.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        const QString name = line.trimmed();
        if (!name.isEmpty())
            result.append(QVariantMap{{QStringLiteral("name"), name}});
    }
    return result;
}

QVariantMap SambaAdapter::applyShare(const QString &shareName, const QString &path,
                                      const QString &acl, bool guestOk)
{
    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("samba-net-not-found")},
                {QStringLiteral("message"), QStringLiteral("Samba tools are not installed")}};
    }
    QProcess removeProc;
    removeProc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("delete"), shareName});
    removeProc.waitForFinished(3000);

    QProcess proc;
    proc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("add"),
                          shareName, path,
                          QStringLiteral("SLM shared folder"),
                          acl,
                          guestOk ? QStringLiteral("guest_ok=y") : QStringLiteral("guest_ok=n")});
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        proc.waitForFinished();
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("usershare-timeout")},
                {QStringLiteral("message"), QStringLiteral("Timed out while creating network share")}};
    }
    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString e = err.toLower();
        QString code = QStringLiteral("usershare-add-failed");
        if (e.contains(QStringLiteral("permission denied")) || e.contains(QStringLiteral("denied")))
            code = QStringLiteral("usershare-permission-denied");
        else if (e.contains(QStringLiteral("usershare")) && e.contains(QStringLiteral("not allowed")))
            code = QStringLiteral("usershare-not-enabled");
        else if (e.contains(QStringLiteral("invalid usershare")))
            code = QStringLiteral("usershare-invalid-config");
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), code},
                {QStringLiteral("message"), err.isEmpty() ? QStringLiteral("Unable to create network share") : err}};
    }
    return {{QStringLiteral("applied"), true}, {QStringLiteral("message"), QStringLiteral("share-configured")}};
}

QVariantMap SambaAdapter::deleteShare(const QString &shareName)
{
    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("samba-net-not-found")},
                {QStringLiteral("message"), QStringLiteral("Samba tools are not installed")}};
    }
    if (shareName.trimmed().isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("missing-share-name")}};
    }
    QProcess proc;
    proc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("delete"), shareName});
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        proc.waitForFinished();
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("usershare-timeout")},
                {QStringLiteral("message"), QStringLiteral("Timed out while disabling network share")}};
    }
    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString e = err.toLower();
        if (e.contains(QStringLiteral("does not exist")) || e.contains(QStringLiteral("not found")))
            return {{QStringLiteral("applied"), true}, {QStringLiteral("message"), QStringLiteral("share-already-disabled")}};
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("usershare-delete-failed")},
                {QStringLiteral("message"), err.isEmpty() ? QStringLiteral("Unable to disable network share") : err}};
    }
    return {{QStringLiteral("applied"), true}, {QStringLiteral("message"), QStringLiteral("share-disabled")}};
}

bool SambaAdapter::runUsershareCommand(const QStringList &args) const
{
    QProcess p;
    p.start(QStringLiteral("net"), args);
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

bool SambaAdapter::smbNetAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("net")).isEmpty();
}

bool SambaAdapter::usersharePermission() const
{
    return QFileInfo(QStringLiteral("/var/lib/samba/usershares")).isWritable()
        || QFileInfo(QStringLiteral("/run/samba/usershares")).isWritable();
}

QString SambaAdapter::sanitizeName(const QString &path) const
{
    return QFileInfo(path).fileName().toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("_")).left(12);
}
