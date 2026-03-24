#pragma once

#include "PluginFieldDescriptor.h"

#include <QString>
#include <QJsonObject>

namespace Slm::Print {

// Parses and validates a print plugin descriptor from JSON.
//
// Security contract:
//   - Only declarative field types allowed: bool, enum, segmented, range.
//   - Maximum 32 fields per descriptor (DoS guard).
//   - Maximum 64 enum values per field.
//   - id and mapToIpp values must match a restrictive allowlist pattern:
//       only [a-zA-Z0-9._-] characters, max 64 chars.
//   - No native code paths, no filesystem/network references.
//   - Any violation returns a descriptor with valid=false and a warning logged.
class PluginDescriptorValidator
{
public:
    // Parse descriptor from a JSON text string.
    // Returns descriptor with valid=false and logs a warning on any violation.
    static PluginDescriptor parse(const QString &jsonText);

    // Parse descriptor from a pre-parsed QJsonObject.
    static PluginDescriptor parse(const QJsonObject &obj);

    // Returns true if the given field type string is in the allowlist.
    static bool isAllowedType(const QString &type);

    // Returns true if the id/mapToIpp token is safe (no injection risk).
    static bool isSafeToken(const QString &token);

private:
    static PluginFieldDescriptor parseField(const QJsonObject &fieldObj, bool &ok);
};

} // namespace Slm::Print
