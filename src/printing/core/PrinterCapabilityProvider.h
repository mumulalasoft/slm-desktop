#pragma once

#include "PrintTypes.h"

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

class PrinterCapabilityProvider : public QObject
{
    Q_OBJECT
public:
    explicit PrinterCapabilityProvider(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap capabilityMapForPrinter(const QString &printerId) const;
    Q_INVOKABLE PrinterCapability capabilityForPrinter(const QString &printerId) const;

    static PrinterCapability parseLpoptions(const QString &printerId, const QString &lpoptionsOutput);
    static QVariantMap capabilityToVariantMap(const PrinterCapability &capability);

private:
    static QString runCommand(const QString &program, const QStringList &arguments);
};

} // namespace Slm::Print
