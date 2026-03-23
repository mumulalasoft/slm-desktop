#pragma once

#include <QString>
#include "urlutils.h"

namespace ResourcePaths {
namespace Qml {
inline QString mainQtPrefix()
{
    return QStringLiteral(":/qt/qml/Slm_Desktop/Main.qml");
}

inline QString mainModulePrefix()
{
    return QStringLiteral(":/Slm_Desktop/Main.qml");
}

inline QString mainQtPrefixUrl()
{
    return QStringLiteral("qrc:/qt/qml/Slm_Desktop/Main.qml");
}

inline QString mainModulePrefixUrl()
{
    return QStringLiteral("qrc:/Slm_Desktop/Main.qml");
}

inline QString indicatorExampleQml()
{
    return QStringLiteral("qrc:/MyApp/VpnIndicator.qml");
}
} // namespace Qml

namespace Icons {
inline QString wifiLow()
{
    return QStringLiteral("qrc:/icons/wifi_1_bar.svg");
}

inline QString wifiMedium()
{
    return QStringLiteral("qrc:/icons/wifi_2_bar.svg");
}

inline QString wifiHigh()
{
    return QStringLiteral("qrc:/icons/wifi_3_bar.svg");
}

inline QString ethernet()
{
    return QStringLiteral("qrc:/icons/ethernet.svg");
}
} // namespace Icons

namespace Url {
inline QString fileSchemePrefix()
{
    return UrlUtils::fileSchemePrefix();
}

inline bool isFileUrl(const QString &value)
{
    return UrlUtils::isFileUrl(value);
}

inline QString toFileUrl(const QString &localPath)
{
    return UrlUtils::toFileUrl(localPath);
}

inline bool isExplicitIconSource(const QString &value)
{
    const QString s = value.trimmed();
    return s.startsWith(QLatin1String("/")) ||
           isFileUrl(s) ||
           s.startsWith(QLatin1String("qrc:/")) ||
           s.startsWith(QLatin1String("image://")) ||
           s.startsWith(QLatin1String("http://")) ||
           s.startsWith(QLatin1String("https://"));
}
} // namespace Url
} // namespace ResourcePaths
