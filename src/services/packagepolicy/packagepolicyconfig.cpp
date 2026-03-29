#include "packagepolicyconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace Slm::PackagePolicy {

namespace {
QString readEnvOrDefault(const char *envName, const QString &fallback)
{
    const QString value = qEnvironmentVariable(envName).trimmed();
    return value.isEmpty() ? fallback : value;
}

QString parseYamlListValue(const QString &line)
{
    QString value = line.trimmed();
    if (!value.startsWith(QLatin1String("-"))) {
        return {};
    }
    value.remove(0, 1);
    value = value.trimmed();
    const int commentPos = value.indexOf(QLatin1Char('#'));
    if (commentPos >= 0) {
        value = value.left(commentPos).trimmed();
    }
    if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) ||
        (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))) {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}
} // namespace

QString PackagePolicyConfig::protectedCapabilitiesPath() const
{
    return readEnvOrDefault("SLM_PACKAGE_POLICY_PROTECTED_FILE",
                            QStringLiteral("/etc/slm/package-policy/protected-capabilities.yaml"));
}

QString PackagePolicyConfig::packageMappingsPath() const
{
    return readEnvOrDefault("SLM_PACKAGE_POLICY_MAPPING_FILE",
                            QStringLiteral("/etc/slm/package-policy/package-mappings/debian.yaml"));
}

QString PackagePolicyConfig::sourcePoliciesDirPath() const
{
    return readEnvOrDefault("SLM_PACKAGE_POLICY_SOURCE_POLICIES_DIR",
                            QStringLiteral("/etc/slm/package-policy/source-policies"));
}

QString PackagePolicyConfig::normalizePackageName(const QString &name)
{
    QString out = name.trimmed().toLower();
    const int archPos = out.indexOf(QLatin1Char(':'));
    if (archPos > 0) {
        out = out.left(archPos);
    }
    return out;
}

bool PackagePolicyConfig::parseYamlBoolValue(const QString &value, bool *ok)
{
    const QString v = value.trimmed().toLower();
    const bool valid = (v == QLatin1String("true")
                        || v == QLatin1String("yes")
                        || v == QLatin1String("1")
                        || v == QLatin1String("false")
                        || v == QLatin1String("no")
                        || v == QLatin1String("0"));
    if (ok) {
        *ok = valid;
    }
    if (!valid) {
        return false;
    }
    return (v == QLatin1String("true") || v == QLatin1String("yes") || v == QLatin1String("1"));
}

bool PackagePolicyConfig::load(QString *error)
{
    m_protectedCapabilities.clear();
    m_protectedPackages.clear();
    m_packageMappings.clear();
    m_sourcePolicies.clear();

    if (!loadProtectedCapabilities(protectedCapabilitiesPath(), error)) {
        return false;
    }
    if (!loadPackageMappings(packageMappingsPath(), error)) {
        return false;
    }
    if (!loadSourcePolicies(sourcePoliciesDirPath(), error)) {
        return false;
    }

    for (const QString &capability : m_protectedCapabilities) {
        const QStringList packages = m_packageMappings.value(capability);
        for (const QString &pkg : packages) {
            const QString normalized = normalizePackageName(pkg);
            if (!normalized.isEmpty()) {
                m_protectedPackages.insert(normalized);
            }
        }
    }

    if (m_protectedPackages.isEmpty()) {
        if (error) {
            *error = QStringLiteral("no protected packages resolved from mappings");
        }
        return false;
    }
    return true;
}

bool PackagePolicyConfig::parseSourcePolicyFile(const QString &path,
                                                SourcePolicy *policy,
                                                QString *error)
{
    if (!policy) {
        if (error) {
            *error = QStringLiteral("source policy object is null");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("cannot open source policy file: ") + path;
        }
        return false;
    }

    SourcePolicy parsed;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos <= 0) {
            continue;
        }
        const QString key = line.left(colonPos).trimmed().toLower();
        QString value = line.mid(colonPos + 1).trimmed();
        const int commentPos = value.indexOf(QLatin1Char('#'));
        if (commentPos >= 0) {
            value = value.left(commentPos).trimmed();
        }
        if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))) {
            value = value.mid(1, value.size() - 2);
        }

        if (key == QLatin1String("id")) {
            parsed.id = value.trimmed().toLower();
        } else if (key == QLatin1String("kind")) {
            parsed.kind = value.trimmed().toLower();
        } else if (key == QLatin1String("risk")) {
            parsed.risk = value.trimmed().toLower();
        } else if (key == QLatin1String("can_replace_core")) {
            bool ok = false;
            parsed.canReplaceCore = parseYamlBoolValue(value, &ok);
            if (!ok && error) {
                *error = QStringLiteral("invalid can_replace_core boolean in: ") + path;
                return false;
            }
        } else if (key == QLatin1String("advanced_only")) {
            bool ok = false;
            parsed.advancedOnly = parseYamlBoolValue(value, &ok);
            if (!ok && error) {
                *error = QStringLiteral("invalid advanced_only boolean in: ") + path;
                return false;
            }
        }
    }

    if (parsed.id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("source policy id is empty: ") + path;
        }
        return false;
    }
    if (parsed.kind.isEmpty()) {
        if (error) {
            *error = QStringLiteral("source policy kind is empty: ") + path;
        }
        return false;
    }
    if (parsed.risk.isEmpty()) {
        parsed.risk = QStringLiteral("unknown");
    }
    *policy = parsed;
    return true;
}

bool PackagePolicyConfig::loadSourcePolicies(const QString &directoryPath, QString *error)
{
    auto injectDefaultPolicies = [this]() {
        SourcePolicy official;
        official.id = QStringLiteral("official");
        official.kind = QStringLiteral("official");
        official.risk = QStringLiteral("low");
        official.canReplaceCore = true;
        official.advancedOnly = false;
        m_sourcePolicies.insert(official.id, official);

        SourcePolicy vendor;
        vendor.id = QStringLiteral("vendor");
        vendor.kind = QStringLiteral("trusted-external");
        vendor.risk = QStringLiteral("medium");
        vendor.canReplaceCore = false;
        vendor.advancedOnly = false;
        m_sourcePolicies.insert(vendor.id, vendor);

        SourcePolicy community;
        community.id = QStringLiteral("community");
        community.kind = QStringLiteral("community");
        community.risk = QStringLiteral("high");
        community.canReplaceCore = false;
        community.advancedOnly = true;
        m_sourcePolicies.insert(community.id, community);

        SourcePolicy local;
        local.id = QStringLiteral("local");
        local.kind = QStringLiteral("local");
        local.risk = QStringLiteral("high");
        local.canReplaceCore = false;
        local.advancedOnly = true;
        m_sourcePolicies.insert(local.id, local);
    };

    QDir dir(directoryPath);
    if (!dir.exists()) {
        injectDefaultPolicies();
        return true;
    }

    const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("*.yaml"),
                                                               QStringLiteral("*.yml")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name);
    for (const QFileInfo &fi : files) {
        SourcePolicy policy;
        QString parseError;
        if (!parseSourcePolicyFile(fi.absoluteFilePath(), &policy, &parseError)) {
            if (error) {
                *error = parseError;
            }
            return false;
        }
        m_sourcePolicies.insert(policy.id, policy);
    }

    if (m_sourcePolicies.isEmpty()) {
        injectDefaultPolicies();
    }
    return true;
}

bool PackagePolicyConfig::loadProtectedCapabilities(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("cannot open protected capabilities file: ") + path;
        }
        return false;
    }

    bool insideList = false;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QStringLiteral("protected_capabilities:"))) {
            insideList = true;
            continue;
        }
        if (!insideList) {
            continue;
        }
        const QString value = parseYamlListValue(line);
        if (!value.isEmpty()) {
            m_protectedCapabilities.insert(value);
        }
    }

    if (m_protectedCapabilities.isEmpty()) {
        if (error) {
            *error = QStringLiteral("protected_capabilities is empty: ") + path;
        }
        return false;
    }
    return true;
}

bool PackagePolicyConfig::loadPackageMappings(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("cannot open package mapping file: ") + path;
        }
        return false;
    }

    QString currentCapability;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (!raw.startsWith(QLatin1Char(' ')) && trimmed.endsWith(QLatin1Char(':'))) {
            currentCapability = trimmed.left(trimmed.size() - 1).trimmed();
            if (!currentCapability.isEmpty() && !m_packageMappings.contains(currentCapability)) {
                m_packageMappings.insert(currentCapability, {});
            }
            continue;
        }

        if (currentCapability.isEmpty()) {
            continue;
        }

        const QString packageName = normalizePackageName(parseYamlListValue(trimmed));
        if (!packageName.isEmpty()) {
            QStringList &rows = m_packageMappings[currentCapability];
            if (!rows.contains(packageName)) {
                rows.push_back(packageName);
            }
        }
    }

    if (m_packageMappings.isEmpty()) {
        if (error) {
            *error = QStringLiteral("package mappings are empty: ") + path;
        }
        return false;
    }
    return true;
}

} // namespace Slm::PackagePolicy
