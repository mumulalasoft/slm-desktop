#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QList>

namespace Slm::Print {

// Allowed field types for declarative plugin descriptors.
// No native code, no filesystem/network access — declarative-only.
namespace PluginFieldType {
    constexpr char Bool[]      = "bool";
    constexpr char Enum[]      = "enum";
    constexpr char Segmented[] = "segmented";
    constexpr char Range[]     = "range";
} // namespace PluginFieldType

struct PluginFieldDescriptor {
    QString      id;
    QString      type;       // PluginFieldType constant
    QString      label;
    QStringList  values;     // valid values for enum/segmented
    int          rangeMin    = 0;
    int          rangeMax    = 100;
    QString      mapToIpp;   // IPP job-attribute name this field maps to
    QVariant     defaultValue;
};

struct PluginDescriptor {
    QString                    id;
    QString                    title;
    QList<PluginFieldDescriptor> fields;
    bool                       valid = false;  // false if parse/validation failed
};

} // namespace Slm::Print
