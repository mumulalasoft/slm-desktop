#pragma once

#include "src/daemon/firewalld/firewalltypes.h"

#include <QVariantMap>

namespace Slm::Firewall {

class PolicyStore
{
public:
    bool start(QString *error = nullptr);

    QVariantMap snapshot() const;
    bool setValue(const QString &path, const QVariant &value, QString *error = nullptr);
    QVariant value(const QString &path, const QVariant &fallback = QVariant()) const;

private:
    QVariantMap m_root;
};

} // namespace Slm::Firewall
