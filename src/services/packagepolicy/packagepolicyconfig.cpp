#include "packagepolicyconfig.h"

#include <QFile>
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

QString PackagePolicyConfig::normalizePackageName(const QString &name)
{
    QString out = name.trimmed().toLower();
    const int archPos = out.indexOf(QLatin1Char(':'));
    if (archPos > 0) {
        out = out.left(archPos);
    }
    return out;
}

bool PackagePolicyConfig::load(QString *error)
{
    m_protectedCapabilities.clear();
    m_protectedPackages.clear();
    m_packageMappings.clear();

    if (!loadProtectedCapabilities(protectedCapabilitiesPath(), error)) {
        return false;
    }
    if (!loadPackageMappings(packageMappingsPath(), error)) {
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
