#include "slmmotiontypes.h"

namespace Slm::Motion {

QString toString(MotionPreset preset)
{
    switch (preset) {
    case MotionPreset::Snappy:
        return QStringLiteral("snappy");
    case MotionPreset::Smooth:
        return QStringLiteral("smooth");
    case MotionPreset::Gentle:
        return QStringLiteral("gentle");
    case MotionPreset::Responsive:
        return QStringLiteral("responsive");
    case MotionPreset::Launcher:
        return QStringLiteral("launcher");
    case MotionPreset::Heavy:
        return QStringLiteral("heavy");
    case MotionPreset::Elastic:
        return QStringLiteral("elastic");
    }
    return QStringLiteral("smooth");
}

MotionPreset motionPresetFromString(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("snappy")) {
        return MotionPreset::Snappy;
    }
    if (v == QStringLiteral("gentle")) {
        return MotionPreset::Gentle;
    }
    if (v == QStringLiteral("responsive")) {
        return MotionPreset::Responsive;
    }
    if (v == QStringLiteral("launcher")) {
        return MotionPreset::Launcher;
    }
    if (v == QStringLiteral("heavy")) {
        return MotionPreset::Heavy;
    }
    if (v == QStringLiteral("elastic")) {
        return MotionPreset::Elastic;
    }
    return MotionPreset::Smooth;
}

} // namespace Slm::Motion
