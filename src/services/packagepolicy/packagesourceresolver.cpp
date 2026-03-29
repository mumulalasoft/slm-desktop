#include "packagesourceresolver.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace Slm::PackagePolicy {

namespace {
QString hostFromUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    const int schemePos = trimmed.indexOf(QStringLiteral("://"));
    if (schemePos < 0) {
        return QString();
    }
    QString hostPart = trimmed.mid(schemePos + 3);
    const int slashPos = hostPart.indexOf(QLatin1Char('/'));
    if (slashPos >= 0) {
        hostPart = hostPart.left(slashPos);
    }
    const int atPos = hostPart.lastIndexOf(QLatin1Char('@'));
    if (atPos >= 0) {
        hostPart = hostPart.mid(atPos + 1);
    }
    const int portPos = hostPart.indexOf(QLatin1Char(':'));
    if (portPos > 0) {
        hostPart = hostPart.left(portPos);
    }
    return hostPart.trimmed().toLower();
}

PackageSourceInfo fromId(const QString &id, const QString &detail)
{
    PackageSourceInfo out;
    out.sourceId = id.trimmed().toLower();
    out.sourceDetail = detail.trimmed();
    if (out.sourceId == QLatin1String("official")) {
        out.sourceKind = QStringLiteral("official");
        out.sourceRisk = QStringLiteral("low");
    } else if (out.sourceId == QLatin1String("vendor")) {
        out.sourceKind = QStringLiteral("trusted-external");
        out.sourceRisk = QStringLiteral("medium");
    } else if (out.sourceId == QLatin1String("community")) {
        out.sourceKind = QStringLiteral("community");
        out.sourceRisk = QStringLiteral("high");
    } else if (out.sourceId == QLatin1String("local")) {
        out.sourceKind = QStringLiteral("local");
        out.sourceRisk = QStringLiteral("high");
    } else {
        out.sourceKind = QStringLiteral("unknown");
        out.sourceRisk = QStringLiteral("unknown");
    }
    return out;
}
} // namespace

QString PackageSourceResolver::normalizePackageName(const QString &name)
{
    QString out = name.trimmed().toLower();
    if (out.endsWith(QLatin1Char(','))) {
        out.chop(1);
    }
    const int archPos = out.indexOf(QLatin1Char(':'));
    if (archPos > 0) {
        out = out.left(archPos);
    }
    return out;
}

PackageSourceInfo PackageSourceResolver::classifyByRepositoryHint(const QString &hint)
{
    const QString normalized = hint.trimmed();
    if (normalized.isEmpty()) {
        return fromId(QStringLiteral("community"), QStringLiteral("unknown-repository"));
    }
    if (normalized.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        || normalized.startsWith(QStringLiteral("cdrom:"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("/var/lib/dpkg/status"), Qt::CaseInsensitive)) {
        return fromId(QStringLiteral("local"), normalized);
    }

    const QString host = hostFromUrl(normalized);
    if (host.isEmpty()) {
        return fromId(QStringLiteral("community"), normalized);
    }

    static const QSet<QString> officialHosts = {
        QStringLiteral("archive.ubuntu.com"),
        QStringLiteral("security.ubuntu.com"),
        QStringLiteral("deb.debian.org"),
        QStringLiteral("packages.elementary.io")
    };
    if (officialHosts.contains(host)) {
        return fromId(QStringLiteral("official"), host);
    }

    if (host.contains(QStringLiteral("ppa.launchpad.net"))
        || host.contains(QStringLiteral("github.io"))
        || host.contains(QStringLiteral("sourceforge.net"))) {
        return fromId(QStringLiteral("community"), host);
    }

    return fromId(QStringLiteral("vendor"), host);
}

PackageSourceInfo PackageSourceResolver::detectAptSource(const QString &packageName)
{
    QProcess process;
    process.start(QStringLiteral("/usr/bin/apt-cache"),
                  QStringList{QStringLiteral("policy"), packageName});
    if (!process.waitForStarted(3000)) {
        return fromId(QStringLiteral("community"), QStringLiteral("apt-cache-unavailable"));
    }
    if (!process.waitForFinished(10000)) {
        process.kill();
        return fromId(QStringLiteral("community"), QStringLiteral("apt-cache-timeout"));
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (output.trimmed().isEmpty()) {
        return fromId(QStringLiteral("community"), QStringLiteral("empty-policy-output"));
    }

    const QRegularExpression repoLineRx(
        QStringLiteral("^\\s*\\d+\\s+(https?://\\S+|ftp://\\S+|file:\\S+|cdrom:\\S+)"));
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch m = repoLineRx.match(line);
        if (!m.hasMatch()) {
            continue;
        }
        return classifyByRepositoryHint(m.captured(1));
    }

    if (output.contains(QStringLiteral("/var/lib/dpkg/status"))) {
        return fromId(QStringLiteral("local"), QStringLiteral("/var/lib/dpkg/status"));
    }
    return fromId(QStringLiteral("community"), QStringLiteral("unclassified-policy-output"));
}

QHash<QString, PackageSourceInfo> PackageSourceResolver::detectForPackages(const QStringList &packages,
                                                                           const QString &tool)
{
    QHash<QString, PackageSourceInfo> out;
    const QString normalizedTool = tool.trimmed().toLower();
    for (const QString &raw : packages) {
        const QString pkg = normalizePackageName(raw);
        if (pkg.isEmpty() || out.contains(pkg)) {
            continue;
        }

        if (normalizedTool == QLatin1String("dpkg")) {
            out.insert(pkg, fromId(QStringLiteral("local"), QStringLiteral("dpkg-local-install")));
            continue;
        }
        out.insert(pkg, detectAptSource(pkg));
    }
    return out;
}

} // namespace Slm::PackagePolicy
