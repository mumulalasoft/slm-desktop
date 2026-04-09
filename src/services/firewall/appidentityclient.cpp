#include "appidentityclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
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
                             const QString &context)
{
    return {
        {QStringLiteral("app_name"), appName.isEmpty() ? QStringLiteral("Unknown App") : appName},
        {QStringLiteral("app_id"), appId.isEmpty() ? QStringLiteral("unknown") : appId},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("executable"), executable.isEmpty() ? QStringLiteral("unknown") : executable},
        {QStringLiteral("source"), source.isEmpty() ? QStringLiteral("manual") : source},
        {QStringLiteral("trust_level"), trustLevel.isEmpty() ? QStringLiteral("unknown") : trustLevel},
        {QStringLiteral("context"), context.isEmpty() ? QStringLiteral("cli") : context},
    };
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
            return normalizedResult(pid, appName, appId, executable, source, trustLevel, context);
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
            return normalizedResult(pid, appName, appId, executable, source, trustLevel, context);
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
    const QString stdinLink = QFile::symLinkTarget(procRoot + QStringLiteral("/fd/0"));
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

    return normalizedResult(pid, appName, appId, executable, source, trustLevel, context);
}

} // namespace Slm::Firewall
