#pragma once

#include <QVariantMap>
#include <QVector>

namespace KWinSupportInfoParser {

QVector<QVariantMap> parseSupportInformationDump(const QString &dump, int activeSpace);

}
