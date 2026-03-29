#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

class PackagePolicyConfig
{
public:
    bool load(QString *error = nullptr);

    QSet<QString> protectedCapabilities() const { return m_protectedCapabilities; }
    QSet<QString> protectedPackages() const { return m_protectedPackages; }
    QHash<QString, QStringList> packageMappings() const { return m_packageMappings; }

    QString protectedCapabilitiesPath() const;
    QString packageMappingsPath() const;

private:
    bool loadProtectedCapabilities(const QString &path, QString *error);
    bool loadPackageMappings(const QString &path, QString *error);

    static QString normalizePackageName(const QString &name);

    QSet<QString> m_protectedCapabilities;
    QSet<QString> m_protectedPackages;
    QHash<QString, QStringList> m_packageMappings;
};

} // namespace Slm::PackagePolicy
