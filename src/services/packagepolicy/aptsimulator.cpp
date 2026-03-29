#include "aptsimulator.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace Slm::PackagePolicy {

QString AptSimulator::normalizePackageName(const QString &name)
{
    QString out = name.trimmed().toLower();
    if (out.endsWith(QLatin1String(","))) {
        out.chop(1);
    }
    const int archPos = out.indexOf(QLatin1Char(':'));
    if (archPos > 0) {
        out = out.left(archPos);
    }
    return out;
}

bool AptSimulator::simulateApt(const QStringList &args, PackageTransaction *transaction, QString *error)
{
    if (!transaction) {
        if (error) {
            *error = QStringLiteral("transaction is null");
        }
        return false;
    }

    QProcess process;
    QStringList simArgs;
    simArgs << QStringLiteral("-s");
    simArgs << args;
    process.start(QStringLiteral("/usr/bin/apt-get"), simArgs);
    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("failed to start /usr/bin/apt-get");
        }
        return false;
    }
    if (!process.waitForFinished(120000)) {
        process.kill();
        if (error) {
            *error = QStringLiteral("apt simulation timeout");
        }
        return false;
    }

    const QString out = QString::fromUtf8(process.readAllStandardOutput());
    const QString errOut = QString::fromUtf8(process.readAllStandardError());
    const QString combined = out + (errOut.isEmpty() ? QString() : QStringLiteral("\n") + errOut);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("apt simulation failed: %1").arg(combined.trimmed());
        }
        return false;
    }

    return parseAptSimulationOutput(combined, transaction, error);
}

bool AptSimulator::parseAptSimulationOutput(const QString &output,
                                            PackageTransaction *transaction,
                                            QString *error)
{
    if (!transaction) {
        if (error) {
            *error = QStringLiteral("transaction is null");
        }
        return false;
    }

    transaction->rawOutput = output;
    transaction->install.clear();
    transaction->remove.clear();
    transaction->upgrade.clear();
    transaction->replace.clear();
    transaction->downgrade.clear();

    const QRegularExpression remvRx(QStringLiteral("^Remv\\s+([^\\s:]+(?::[^\\s]+)?)"));
    const QRegularExpression instRx(QStringLiteral("^Inst\\s+([^\\s:]+(?::[^\\s]+)?)"));
    const QRegularExpression upgrRx(QStringLiteral("^Upgr\\s+([^\\s:]+(?::[^\\s]+)?)"));
    const QRegularExpression replacingRx(QStringLiteral("Replacing\\s+([^\\s,]+)"),
                                         QRegularExpression::CaseInsensitiveOption);

    QSet<QString> removeSet;
    QSet<QString> installSet;
    QSet<QString> upgradeSet;
    QSet<QString> downgradeSet;
    QSet<QString> replaceSet;

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QRegularExpressionMatch match = remvRx.match(line);
        if (match.hasMatch()) {
            const QString pkg = normalizePackageName(match.captured(1));
            if (!pkg.isEmpty()) {
                removeSet.insert(pkg);
            }
            continue;
        }

        match = instRx.match(line);
        if (match.hasMatch()) {
            const QString pkg = normalizePackageName(match.captured(1));
            if (!pkg.isEmpty()) {
                installSet.insert(pkg);
            }
            if (line.contains(QLatin1Char('['))) {
                if (!pkg.isEmpty()) {
                    upgradeSet.insert(pkg);
                }
            }
            if (line.contains(QStringLiteral("downgrad"), Qt::CaseInsensitive)) {
                if (!pkg.isEmpty()) {
                    downgradeSet.insert(pkg);
                }
            }

            QRegularExpressionMatch replacingMatch = replacingRx.match(line);
            if (replacingMatch.hasMatch()) {
                const QString replaced = normalizePackageName(replacingMatch.captured(1));
                if (!replaced.isEmpty()) {
                    replaceSet.insert(replaced);
                }
            }
            continue;
        }

        match = upgrRx.match(line);
        if (match.hasMatch()) {
            const QString pkg = normalizePackageName(match.captured(1));
            if (!pkg.isEmpty()) {
                upgradeSet.insert(pkg);
            }
            if (line.contains(QStringLiteral("downgrad"), Qt::CaseInsensitive)) {
                if (!pkg.isEmpty()) {
                    downgradeSet.insert(pkg);
                }
            }
            continue;
        }

        if (line.contains(QStringLiteral("downgrad"), Qt::CaseInsensitive)) {
            for (const QString &installed : installSet) {
                if (line.contains(installed, Qt::CaseInsensitive)) {
                    downgradeSet.insert(installed);
                }
            }
        }
    }

    transaction->install = QStringList(installSet.begin(), installSet.end());
    transaction->remove = QStringList(removeSet.begin(), removeSet.end());
    transaction->upgrade = QStringList(upgradeSet.begin(), upgradeSet.end());
    transaction->replace = QStringList(replaceSet.begin(), replaceSet.end());
    transaction->downgrade = QStringList(downgradeSet.begin(), downgradeSet.end());

    transaction->install.sort();
    transaction->remove.sort();
    transaction->upgrade.sort();
    transaction->replace.sort();
    transaction->downgrade.sort();

    return true;
}

bool AptSimulator::parseDpkgIntent(const QStringList &args, PackageTransaction *transaction, QString *error)
{
    if (!transaction) {
        if (error) {
            *error = QStringLiteral("transaction is null");
        }
        return false;
    }

    transaction->install.clear();
    transaction->remove.clear();
    transaction->upgrade.clear();
    transaction->replace.clear();
    transaction->downgrade.clear();

    QSet<QString> installSet;
    QSet<QString> removeSet;

    bool removeMode = false;
    bool installMode = false;
    for (const QString &arg : args) {
        if (arg == QLatin1String("-r")
            || arg == QLatin1String("--remove")
            || arg == QLatin1String("-P")
            || arg == QLatin1String("--purge")) {
            removeMode = true;
            installMode = false;
            continue;
        }
        if (arg == QLatin1String("-i")
            || arg == QLatin1String("--install")) {
            installMode = true;
            removeMode = false;
            continue;
        }
        if (arg.startsWith(QLatin1Char('-'))) {
            continue;
        }

        if (removeMode) {
            const QString pkg = normalizePackageName(arg);
            if (!pkg.isEmpty()) {
                removeSet.insert(pkg);
            }
        } else if (installMode) {
            const QFileInfo info(arg);
            QString pkg = info.baseName().toLower();
            const int underscorePos = pkg.indexOf(QLatin1Char('_'));
            if (underscorePos > 0) {
                pkg = pkg.left(underscorePos);
            }
            pkg = normalizePackageName(pkg);
            if (!pkg.isEmpty()) {
                installSet.insert(pkg);
            }
        }
    }

    transaction->install = QStringList(installSet.begin(), installSet.end());
    transaction->remove = QStringList(removeSet.begin(), removeSet.end());
    transaction->install.sort();
    transaction->remove.sort();
    return true;
}

} // namespace Slm::PackagePolicy
