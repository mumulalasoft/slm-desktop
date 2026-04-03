#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

struct SourcePolicy {
    QString id;
    QString kind;
    QString risk;
    bool canReplaceCore = false;
    bool advancedOnly = false;
};

class PackagePolicyConfig
{
public:
    bool load(QString *error = nullptr);

    QSet<QString> protectedCapabilities() const { return m_protectedCapabilities; }
    QSet<QString> protectedPackages() const { return m_protectedPackages; }
    QHash<QString, QStringList> packageMappings() const { return m_packageMappings; }
    QHash<QString, SourcePolicy> sourcePolicies() const { return m_sourcePolicies; }

    QString protectedCapabilitiesPath() const;
    QString packageMappingsPath() const;
    QString sourcePoliciesDirPath() const;

private:
    bool loadProtectedCapabilities(const QString &path, QString *error);
    bool loadPackageMappings(const QString &path, QString *error);
    bool loadSourcePolicies(const QString &directoryPath, QString *error);
    static bool parseSourcePolicyFile(const QString &path, SourcePolicy *policy, QString *error);

    static QString normalizePackageName(const QString &name);
    static bool parseYamlBoolValue(const QString &value, bool *ok = nullptr);

    QSet<QString> m_protectedCapabilities;
    QSet<QString> m_protectedPackages;
    QHash<QString, QStringList> m_packageMappings;
    QHash<QString, SourcePolicy> m_sourcePolicies;
};

} // namespace Slm::PackagePolicy
