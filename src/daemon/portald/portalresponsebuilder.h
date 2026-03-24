#pragma once

#include "portalerrorcodes.h"

#include <QString>
#include <QVariantMap>

namespace SlmPortalResponseBuilder {

inline QVariantMap withError(const QString &method,
                             const QString &error,
                             const QVariantMap &extra = QVariantMap())
{
    QVariantMap out = extra;
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("error"), error);
    out.insert(QStringLiteral("method"), method);
    return out;
}

inline QVariantMap invalidArgument(const QString &method,
                                   const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QString::fromLatin1(SlmPortalError::kInvalidArgument),
                     extra);
}

inline QVariantMap notImplemented(const QString &method,
                                  const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QString::fromLatin1(SlmPortalError::kNotImplemented),
                     extra);
}

inline QVariantMap serviceUnavailable(const QString &method,
                                      const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QString::fromLatin1(SlmPortalError::kServiceUnavailable),
                     extra);
}

inline QVariantMap uiServiceUnavailable(const QString &method,
                                        const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QString::fromLatin1(SlmPortalError::kUiServiceUnavailable),
                     extra);
}

inline QVariantMap uiCallFailed(const QString &method,
                                const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QString::fromLatin1(SlmPortalError::kUiCallFailed),
                     extra);
}

inline QVariantMap permissionDenied(const QString &method,
                                    const QVariantMap &extra = QVariantMap())
{
    return withError(method,
                     QStringLiteral("permission-denied"),
                     extra);
}

}
