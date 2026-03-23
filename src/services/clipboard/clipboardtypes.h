#pragma once

#include <QVariantMap>

namespace Slm::Clipboard {

inline constexpr const char kTypeText[] = "TEXT";
inline constexpr const char kTypeImage[] = "IMAGE";
inline constexpr const char kTypeUrl[] = "URL";
inline constexpr const char kTypeFile[] = "FILE";
inline constexpr const char kTypeCode[] = "CODE";
inline constexpr const char kTypeColor[] = "COLOR";

inline QVariantMap normalizeRecord(const QVariantMap &row)
{
    QVariantMap out = row;
    out.insert(QStringLiteral("id"), out.value(QStringLiteral("id")).toLongLong());
    out.insert(QStringLiteral("isPinned"), out.value(QStringLiteral("isPinned")).toBool());
    out.insert(QStringLiteral("timestamp"), out.value(QStringLiteral("timestamp")).toLongLong());
    return out;
}

} // namespace Slm::Clipboard
