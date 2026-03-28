#include "dependencyguard.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Slm::System {

namespace {

QString findPackageManagerExecutable(QString *managerId)
{
    const struct Candidate {
        const char *id;
        const char *exe;
    } candidates[] = {
        {"apt", "apt-get"},
        {"dnf", "dnf"},
        {"pacman", "pacman"},
        {"zypper", "zypper"},
    };

    for (const auto &candidate : candidates) {
        const QString exe = QStandardPaths::findExecutable(QString::fromLatin1(candidate.exe));
        if (!exe.isEmpty()) {
            if (managerId) {
                *managerId = QString::fromLatin1(candidate.id);
            }
            return exe;
        }
    }
    if (managerId) {
        managerId->clear();
    }
    return QString();
}

QStringList installArgsForManager(const QString &managerId, const QString &packageName)
{
    if (managerId == QStringLiteral("apt")) {
        return {QStringLiteral("install"), QStringLiteral("-y"), packageName};
    }
    if (managerId == QStringLiteral("dnf")) {
        return {QStringLiteral("-y"), QStringLiteral("install"), packageName};
    }
    if (managerId == QStringLiteral("pacman")) {
        return {QStringLiteral("--noconfirm"), QStringLiteral("-S"), packageName};
    }
    if (managerId == QStringLiteral("zypper")) {
        return {QStringLiteral("--non-interactive"), QStringLiteral("install"), packageName};
    }
    return {};
}

QVariantMap toResult(bool ok, const QString &error, const QVariantMap &extra = {})
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), ok);
    if (!error.isEmpty()) {
        out.insert(QStringLiteral("error"), error);
    }
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

QStringList normalizeDistroKeys(const QString &raw)
{
    QStringList out;
    const QStringList parts = raw.toLower().split(QRegularExpression(QStringLiteral("[\\s,]+")),
                                                  Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString key = part.trimmed();
        if (!key.isEmpty() && !out.contains(key)) {
            out.push_back(key);
        }
    }
    return out;
}

QVariantMap readOsRelease()
{
    QVariantMap out;
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int sep = trimmed.indexOf(QLatin1Char('='));
        if (sep <= 0) {
            continue;
        }
        QString key = trimmed.left(sep).trimmed().toUpper();
        QString value = trimmed.mid(sep + 1).trimmed();
        if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))) {
            value = value.mid(1, value.size() - 2);
        }
        out.insert(key, value);
    }
    return out;
}

QString resolvePackageName(const ComponentRequirement &req, const QString &managerId = QString())
{
    const QString fallback = req.packageName.trimmed();
    if (req.packageNamesByDistro.isEmpty()) {
        return fallback;
    }

    const QVariantMap osRelease = readOsRelease();
    QStringList probeKeys;
    probeKeys << normalizeDistroKeys(osRelease.value(QStringLiteral("ID")).toString());
    probeKeys << normalizeDistroKeys(osRelease.value(QStringLiteral("ID_LIKE")).toString());
    if (!managerId.trimmed().isEmpty()) {
        probeKeys << managerId.trimmed().toLower();
    }
    probeKeys << QStringLiteral("*");

    for (const QString &key : probeKeys) {
        const auto it = req.packageNamesByDistro.constFind(key);
        if (it != req.packageNamesByDistro.constEnd()) {
            const QString mapped = it.value().trimmed();
            if (!mapped.isEmpty()) {
                return mapped;
            }
        }
    }
    return fallback;
}

bool matchesAnyPathCandidate(const QString &spec)
{
    const QStringList candidates = spec.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString &candidateRaw : candidates) {
        const QString candidate = candidateRaw.trimmed();
        if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
            return true;
        }
    }
    return false;
}

QVariantList missingPathSpecs(const QStringList &pathSpecs)
{
    QVariantList missing;
    for (const QString &specRaw : pathSpecs) {
        const QString spec = specRaw.trimmed();
        if (spec.isEmpty()) {
            continue;
        }
        if (!matchesAnyPathCandidate(spec)) {
            missing.push_back(spec);
        }
    }
    return missing;
}

} // namespace

QVariantMap checkComponent(const ComponentRequirement &req)
{
    const QString resolvedPackageName = resolvePackageName(req);
    QVariantList missingExecutables;
    for (const QString &name : req.requiredExecutables) {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (QStandardPaths::findExecutable(trimmed).isEmpty()) {
            missingExecutables.push_back(trimmed);
        }
    }
    const QVariantList missingPaths = missingPathSpecs(req.requiredPaths);
    QVariantList missing = missingExecutables;
    missing += missingPaths;

    const bool ready = missing.isEmpty();
    return toResult(ready, ready ? QString() : QStringLiteral("missing-component"),
                    {
                        {QStringLiteral("componentId"), req.id},
                        {QStringLiteral("title"), req.title},
                        {QStringLiteral("description"), req.description},
                        {QStringLiteral("packageName"), resolvedPackageName},
                        {QStringLiteral("packageNameDefault"), req.packageName},
                        {QStringLiteral("guidance"), req.guidance},
                        {QStringLiteral("ready"), ready},
                        {QStringLiteral("missingExecutables"), missingExecutables},
                        {QStringLiteral("missingArtifacts"), missing},
                        {QStringLiteral("missingPaths"), missingPaths},
                        {QStringLiteral("autoInstallable"), req.autoInstallable && !resolvedPackageName.isEmpty()},
                    });
}

QVariantMap installComponentWithPolkit(const ComponentRequirement &req, int timeoutMs)
{
    const QVariantMap status = checkComponent(req);
    if (status.value(QStringLiteral("ready")).toBool()) {
        QVariantMap extra = status;
        extra.insert(QStringLiteral("alreadyInstalled"), true);
        return toResult(true, QString(), extra);
    }

    if (!status.value(QStringLiteral("autoInstallable")).toBool()) {
        return toResult(false, QStringLiteral("component-not-auto-installable"), status);
    }

    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        return toResult(false, QStringLiteral("pkexec-not-found"), status);
    }

    QString managerId;
    const QString pkgManagerExe = findPackageManagerExecutable(&managerId);
    if (pkgManagerExe.isEmpty() || managerId.isEmpty()) {
        return toResult(false, QStringLiteral("package-manager-not-found"), status);
    }

    const QString packageName = resolvePackageName(req, managerId);
    const QStringList pkgArgs = installArgsForManager(managerId, packageName);
    if (pkgArgs.isEmpty()) {
        return toResult(false, QStringLiteral("package-manager-unsupported"), status);
    }

    QProcess proc;
    QStringList args;
    args << pkgManagerExe;
    args << pkgArgs;
    proc.start(pkexec, args);
    if (!proc.waitForStarted(5000)) {
        return toResult(false, QStringLiteral("install-process-start-failed"), status);
    }
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished();
        return toResult(false, QStringLiteral("install-timeout"), status);
    }

    const int exitCode = proc.exitCode();
    const bool ok = (proc.exitStatus() == QProcess::NormalExit && exitCode == 0);
    QVariantMap extra = status;
    extra.insert(QStringLiteral("packageManager"), managerId);
    extra.insert(QStringLiteral("command"), QStringList{pkexec, pkgManagerExe} + pkgArgs);
    extra.insert(QStringLiteral("stdout"), QString::fromUtf8(proc.readAllStandardOutput()));
    extra.insert(QStringLiteral("stderr"), QString::fromUtf8(proc.readAllStandardError()));
    extra.insert(QStringLiteral("exitCode"), exitCode);
    if (!ok) {
        return toResult(false, QStringLiteral("install-failed"), extra);
    }

    const QVariantMap after = checkComponent(req);
    extra.insert(QStringLiteral("readyAfterInstall"), after.value(QStringLiteral("ready")).toBool());
    return toResult(after.value(QStringLiteral("ready")).toBool(),
                    after.value(QStringLiteral("ready")).toBool() ? QString() : QStringLiteral("component-still-missing"),
                    extra);
}

} // namespace Slm::System
