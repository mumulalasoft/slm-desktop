#pragma once

#include "PrintTypes.h"

#include <QVariantMap>

namespace Slm::Print {

class PrintTicketSerializer
{
public:
    static QVariantMap toVariantMap(const PrintTicket &ticket);
    static PrintTicket fromVariantMap(const QVariantMap &payload);

    // Returns IPP-like key/value attributes for JobSubmitter integration.
    static QVariantMap toIppAttributes(const PrintTicket &ticket);
};

} // namespace Slm::Print
