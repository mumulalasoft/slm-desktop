#include "PortalResponseSerializer.h"

namespace Slm::PortalAdapter {

QString PortalResponseSerializer::errorToString(PortalError error)
{
    switch (error) {
    case PortalError::PermissionDenied: return QStringLiteral("PermissionDenied");
    case PortalError::CancelledByUser: return QStringLiteral("CancelledByUser");
    case PortalError::InvalidRequest: return QStringLiteral("InvalidRequest");
    case PortalError::SessionNotActive: return QStringLiteral("SessionNotActive");
    case PortalError::CapabilityNotMapped: return QStringLiteral("CapabilityNotMapped");
    case PortalError::PolicyEvaluationFailed: return QStringLiteral("PolicyEvaluationFailed");
    case PortalError::BackendUnavailable: return QStringLiteral("BackendUnavailable");
    case PortalError::UnsupportedOperation: return QStringLiteral("UnsupportedOperation");
    case PortalError::None:
    default:
        return QStringLiteral("None");
    }
}

uint PortalResponseSerializer::responseCodeForError(PortalError error)
{
    switch (error) {
    case PortalError::None:
        return 0;
    case PortalError::PermissionDenied:
        return 1;
    case PortalError::CancelledByUser:
        return 2;
    case PortalError::InvalidRequest:
    case PortalError::SessionNotActive:
    case PortalError::CapabilityNotMapped:
    case PortalError::PolicyEvaluationFailed:
    case PortalError::BackendUnavailable:
    case PortalError::UnsupportedOperation:
    default:
        return 3;
    }
}

QVariantMap PortalResponseSerializer::success(const QVariantMap &payload)
{
    QVariantMap out = payload;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("error"), QString());
    out.insert(QStringLiteral("response"), 0u);
    if (!out.contains(QStringLiteral("results"))) {
        out.insert(QStringLiteral("results"), payload);
    }
    return out;
}

QVariantMap PortalResponseSerializer::denied(const QString &reason)
{
    return error(PortalError::PermissionDenied, reason);
}

QVariantMap PortalResponseSerializer::cancelled(const QString &reason)
{
    return error(PortalError::CancelledByUser, reason);
}

QVariantMap PortalResponseSerializer::error(PortalError err,
                                            const QString &reason,
                                            const QVariantMap &details)
{
    QVariantMap out = details;
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("error"), errorToString(err));
    out.insert(QStringLiteral("reason"), reason);
    out.insert(QStringLiteral("response"), responseCodeForError(err));
    if (!out.contains(QStringLiteral("results"))) {
        QVariantMap results = details;
        results.insert(QStringLiteral("error"), errorToString(err));
        results.insert(QStringLiteral("reason"), reason);
        out.insert(QStringLiteral("results"), results);
    }
    return out;
}

} // namespace Slm::PortalAdapter
