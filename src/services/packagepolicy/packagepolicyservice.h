#pragma once

#include <QObject>
#include <QVariantMap>

namespace Slm::PackagePolicy {

class PackagePolicyService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.PackagePolicy1")

public:
    explicit PackagePolicyService(QObject *parent = nullptr);

public slots:
    QVariantMap Evaluate(const QString &tool, const QStringList &args);

private:
    QVariantMap evaluateInternal(const QString &tool, const QStringList &args);
};

} // namespace Slm::PackagePolicy
