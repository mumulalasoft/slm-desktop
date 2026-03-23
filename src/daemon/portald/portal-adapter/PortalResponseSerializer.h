#pragma once

#include <QString>
#include <QVariantMap>

namespace Slm::PortalAdapter {

enum class PortalError {
    None = 0,
    PermissionDenied,
    CancelledByUser,
    InvalidRequest,
    SessionNotActive,
    CapabilityNotMapped,
    PolicyEvaluationFailed,
    BackendUnavailable,
    UnsupportedOperation,
};

class PortalResponseSerializer {
public:
    static QString errorToString(PortalError error);
    static uint responseCodeForError(PortalError error);
    static QVariantMap success(const QVariantMap &payload = {});
    static QVariantMap denied(const QString &reason = QStringLiteral("denied"));
    static QVariantMap cancelled(const QString &reason = QStringLiteral("cancelled"));
    static QVariantMap error(PortalError error,
                             const QString &reason,
                             const QVariantMap &details = {});
};

} // namespace Slm::PortalAdapter
