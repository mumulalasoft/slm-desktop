#include "appidentityclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>

namespace Slm::Firewall {

namespace {

QString readFirstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

QStringList readProcCmdline(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray raw = file.readAll();
    if (raw.isEmpty()) {
        return {};
    }
    QStringList out;
    const QList<QByteArray> parts = raw.split('\0');
    for (const QByteArray &part : parts) {
        if (!part.isEmpty()) {
            out.append(QString::fromUtf8(part));
        }
    }
    return out;
}

QVariantMap readProcMeta(qint64 pid)
{
    QVariantMap meta;
    const QString statusPath = QStringLiteral("/proc/%1/status").arg(pid);
    QFile statusFile(statusPath);
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(statusFile.readAll());
        const QRegularExpression uidRx(QStringLiteral("^Uid:\\s*(\\d+)"), QRegularExpression::MultilineOption);
        const QRegularExpression ppidRx(QStringLiteral("^PPid:\\s*(\\d+)"), QRegularExpression::MultilineOption);
        const QRegularExpressionMatch uidMatch = uidRx.match(content);
        const QRegularExpressionMatch ppidMatch = ppidRx.match(content);
        if (uidMatch.hasMatch()) {
            meta.insert(QStringLiteral("uid"), uidMatch.captured(1).toLongLong());
        }
        if (ppidMatch.hasMatch()) {
            meta.insert(QStringLiteral("parent_pid"), ppidMatch.captured(1).toLongLong());
        }
    }

    const QString cwd = QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(pid));
    if (!cwd.isEmpty()) {
        meta.insert(QStringLiteral("cwd"), cwd);
    }

    const QString stdinLink = QFile::symLinkTarget(QStringLiteral("/proc/%1/fd/0").arg(pid));
    if (!stdinLink.isEmpty()) {
        meta.insert(QStringLiteral("tty"), stdinLink);
    }

    QFile cgroupFile(QStringLiteral("/proc/%1/cgroup").arg(pid));
    if (cgroupFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString cgroup = QString::fromUtf8(cgroupFile.readAll()).trimmed();
        if (!cgroup.isEmpty()) {
            meta.insert(QStringLiteral("cgroup"), cgroup);
        }
    }

    const QStringList argv = readProcCmdline(pid);
    if (!argv.isEmpty()) {
        QVariantList argValues;
        for (const QString &arg : argv) {
            argValues.append(arg);
        }
        meta.insert(QStringLiteral("argv"), argValues);
    }
    return meta;
}

QString inferSourceFromExecutable(const QString &executable)
{
    if (executable.startsWith(QStringLiteral("/var/lib/flatpak/"))
            || executable.startsWith(QStringLiteral("/app/"))) {
        return QStringLiteral("flatpak");
    }
    if (executable.startsWith(QStringLiteral("/usr/"))
            || executable.startsWith(QStringLiteral("/bin/"))
            || executable.startsWith(QStringLiteral("/sbin/"))
            || executable.startsWith(QStringLiteral("/lib/"))) {
        return QStringLiteral("apt");
    }
    if (executable.startsWith(QStringLiteral("/home/"))) {
        return QStringLiteral("manual");
    }
    return QStringLiteral("manual");
}

QString inferTrustLevel(const QString &source, const QString &executable)
{
    if (executable.startsWith(QStringLiteral("/usr/sbin/"))
            || executable.startsWith(QStringLiteral("/usr/lib/systemd/"))
            || executable.contains(QStringLiteral("/systemd"))) {
        return QStringLiteral("system");
    }
    if (source == QLatin1String("flatpak") || source == QLatin1String("apt")) {
        return QStringLiteral("trusted");
    }
    if (executable.startsWith(QStringLiteral("/usr/"))
            || executable.startsWith(QStringLiteral("/bin/"))
            || executable.startsWith(QStringLiteral("/sbin/"))) {
        return QStringLiteral("system");
    }
    return QStringLiteral("unknown");
}

QString inferContext(const QString &category, const QString &executable, const QString &tty)
{
    static const QSet<QString> interpreters{
        QStringLiteral("python"),
        QStringLiteral("python3"),
        QStringLiteral("node"),
        QStringLiteral("ruby"),
        QStringLiteral("perl"),
        QStringLiteral("bash"),
        QStringLiteral("sh"),
        QStringLiteral("zsh"),
        QStringLiteral("fish"),
    };

    const QString exeBase = QFileInfo(executable).fileName().toLower();
    if (interpreters.contains(exeBase)) {
        return QStringLiteral("interpreter");
    }
    if (category == QLatin1String("cli-app")
            || category == QLatin1String("background-agent")
            || category == QLatin1String("system-ignore")) {
        return QStringLiteral("cli");
    }
    if (!tty.isEmpty() && tty != QLatin1String("0")) {
        return QStringLiteral("cli");
    }
    return QStringLiteral("gui");
}

QVariantMap normalizedResult(qint64 pid,
                             const QString &appName,
                             const QString &appId,
                             const QString &executable,
                             const QString &source,
                             const QString &trustLevel,
                             const QString &context,
                             const QVariantMap &contextMeta = {},
                             const QString &scriptTarget = QString())
{
    QVariantMap result{
        {QStringLiteral("app_name"), appName.isEmpty() ? QStringLiteral("Unknown App") : appName},
        {QStringLiteral("app_id"), appId.isEmpty() ? QStringLiteral("unknown") : appId},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("executable"), executable.isEmpty() ? QStringLiteral("unknown") : executable},
        {QStringLiteral("source"), source.isEmpty() ? QStringLiteral("manual") : source},
        {QStringLiteral("trust_level"), trustLevel.isEmpty() ? QStringLiteral("unknown") : trustLevel},
        {QStringLiteral("context"), context.isEmpty() ? QStringLiteral("cli") : context},
    };
    if (!contextMeta.isEmpty()) {
        result.insert(QStringLiteral("context_meta"), contextMeta);
    }
    if (!scriptTarget.trimmed().isEmpty()) {
        result.insert(QStringLiteral("script_target"), scriptTarget.trimmed());
    }
    return result;
}

} // namespace

QVariantMap AppIdentityClient::resolveByPid(qint64 pid) const
{
    const QVariantMap appdIdentity = resolveViaAppd(pid);
    if (!appdIdentity.isEmpty()) {
        return appdIdentity;
    }

    const QVariantMap procIdentity = resolveViaProc(pid);
    if (!procIdentity.isEmpty()) {
        return procIdentity;
    }

    return normalizedResult(pid,
                            QStringLiteral("Unknown App"),
                            QStringLiteral("unknown"),
                            QStringLiteral("unknown"),
                            QStringLiteral("manual"),
                            QStringLiteral("unknown"),
                            QStringLiteral("cli"));
}

QVariantMap AppIdentityClient::resolveViaAppd(qint64 pid) const
{
    if (pid <= 0) {
        return {};
    }

    QDBusInterface iface(QStringLiteral("org.desktop.Apps"),
                         QStringLiteral("/org/desktop/Apps"),
                         QStringLiteral("org.desktop.Apps"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }

    const QVariantMap procIdentity = resolveViaProc(pid);
    const QVariantMap procMeta = procIdentity.value(QStringLiteral("context_meta")).toMap();
    const QString scriptTarget = procIdentity.value(QStringLiteral("script_target")).toString();

    const QDBusReply<QVariantMap> resolveReply = iface.call(QStringLiteral("ResolveByPid"), pid);
    if (resolveReply.isValid()) {
        const QVariantMap payload = resolveReply.value();
        if (payload.value(QStringLiteral("ok"), false).toBool()) {
            const QString appId = payload.value(QStringLiteral("appId")).toString().trimmed();
            const QString category = payload.value(QStringLiteral("category")).toString().trimmed();
            QString executable = payload.value(QStringLiteral("executableHint")).toString().trimmed();
            if (executable.isEmpty()) {
                executable = procIdentity.value(QStringLiteral("executable")).toString().trimmed();
            }
            const QString appName = appId.isEmpty() ? QFileInfo(executable).baseName() : appId;
            const QString source = inferSourceFromExecutable(executable);
            const QString trustLevel = inferTrustLevel(source, executable);
            const QString ttyHint = procIdentity.value(QStringLiteral("context")).toString() == QLatin1String("cli")
                ? QStringLiteral("/dev/tty")
                : QString();
            const QString context = inferContext(category, executable, ttyHint);
            return normalizedResult(pid, appName, appId, executable, source, trustLevel, context, procMeta, scriptTarget);
        }
    }

    const QDBusReply<QVariantList> listReply = iface.call(QStringLiteral("ListRunningApps"));
    if (!listReply.isValid()) {
        return {};
    }
    const QVariantList apps = listReply.value();
    for (const QVariant &item : apps) {
        const QVariantMap app = item.toMap();
        const QVariantList pids = app.value(QStringLiteral("pids")).toList();
        for (const QVariant &candidate : pids) {
            if (candidate.toLongLong() != pid) {
                continue;
            }
            const QString appId = app.value(QStringLiteral("appId")).toString().trimmed();
            const QString category = app.value(QStringLiteral("category")).toString().trimmed();
            QString executable = procIdentity.value(QStringLiteral("executable")).toString().trimmed();
            const QString appName = appId.isEmpty() ? QFileInfo(executable).baseName() : appId;
            const QString source = inferSourceFromExecutable(executable);
            const QString trustLevel = inferTrustLevel(source, executable);
            const QString ttyHint = procIdentity.value(QStringLiteral("context")).toString() == QLatin1String("cli")
                ? QStringLiteral("/dev/tty")
                : QString();
            const QString context = inferContext(category, executable, ttyHint);
            return normalizedResult(pid, appName, appId, executable, source, trustLevel, context, procMeta, scriptTarget);
        }
    }
    return {};
}

QVariantMap AppIdentityClient::resolveViaProc(qint64 pid) const
{
    if (pid <= 0) {
        return {};
    }
    const QString procRoot = QStringLiteral("/proc/%1").arg(pid);
    const QString executable = QFile::symLinkTarget(procRoot + QStringLiteral("/exe"));
    const QString comm = readFirstLine(procRoot + QStringLiteral("/comm"));
    const QVariantMap procMeta = readProcMeta(pid);
    const QString stdinLink = procMeta.value(QStringLiteral("tty")).toString();
    const QString tty = (stdinLink.startsWith(QStringLiteral("/dev/pts/"))
                         || stdinLink.startsWith(QStringLiteral("/dev/tty")))
        ? stdinLink
        : QString();

    QString appName = QFileInfo(executable).baseName();
    if (appName.isEmpty()) {
        appName = comm;
    }
    QString appId = appName.toLower();
    appId.replace(QLatin1Char(' '), QLatin1Char('-'));
    if (appId.isEmpty()) {
        appId = QStringLiteral("unknown");
    }
    const QString source = inferSourceFromExecutable(executable);
    const QString trustLevel = inferTrustLevel(source, executable);
    const QString context = inferContext(QString(), executable, tty);
    QString scriptTarget;
    if (context == QLatin1String("interpreter")) {
        const QVariantList argv = procMeta.value(QStringLiteral("argv")).toList();
        if (argv.size() > 1) {
            scriptTarget = argv.at(1).toString();
        }
    }

    return normalizedResult(pid, appName, appId, executable, source, trustLevel, context, procMeta, scriptTarget);
}

} // namespace Slm::Firewall
