#pragma once

#include "PrintSession.h"
#include "PrintTicketSerializer.h"

#include <QVariantMap>

namespace Slm::Print {

class PrintJobBuilder
{
public:
    // Returns payload:
    // {
    //   success: bool,
    //   error: string,
    //   documentUri: string,
    //   documentTitle: string,
    //   printerId: string,
    //   ticket: QVariantMap,
    //   ippAttributes: QVariantMap
    // }
    static QVariantMap build(const PrintSession *session);
};

} // namespace Slm::Print
