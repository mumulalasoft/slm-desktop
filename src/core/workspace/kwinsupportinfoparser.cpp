#include "kwinsupportinfoparser.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantList>

namespace {

QString normalizeLine(const QString &line)
{
    QString s = line.trimmed();
    s.replace('\t', ' ');
    return s;
}

QString canonicalAppId(QString raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    if (s.contains('/')) {
        s = QFileInfo(s).fileName();
    }
    if (s.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        s.chop(8);
    }
    return s.trimmed();
}

QVariantList parseGeometryFromString(const QString &raw)
{
    if (raw.isEmpty()) {
        return {};
    }
    QRegularExpressionMatch m = QRegularExpression(
        QStringLiteral("(\\-?\\d+)\\s*,\\s*(\\-?\\d+)\\s*[xX, ]\\s*(\\d+)\\s*[xX, ]\\s*(\\d+)"))
                                    .match(raw);
    if (m.hasMatch()) {
        return { m.captured(1).toInt(), m.captured(2).toInt(),
                 m.captured(3).toInt(), m.captured(4).toInt() };
    }
    m = QRegularExpression(QStringLiteral("(\\-?\\d+)\\D+(\\-?\\d+)\\D+(\\d+)\\D+(\\d+)"))
            .match(raw);
    if (m.hasMatch()) {
        return { m.captured(1).toInt(), m.captured(2).toInt(),
                 m.captured(3).toInt(), m.captured(4).toInt() };
    }
    return {};
}

QVariantMap parseSupportWindowBlockNoIcon(const QStringList &block, int activeSpace)
{
    QString title;
    QString appId;
    QString viewId;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int space = activeSpace;
    bool minimized = false;
    bool focused = false;
    bool hasUsefulField = false;

    for (const QString &rawLine : block) {
        const QString line = normalizeLine(rawLine);
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed().toLower();
        const QString val = line.mid(colon + 1).trimmed();
        if (key.contains(QStringLiteral("caption")) || key == QStringLiteral("title")) {
            title = val;
            hasUsefulField = true;
        } else if (key.contains(QStringLiteral("resourceclass")) ||
                   key.contains(QStringLiteral("desktopfilename")) ||
                   key == QStringLiteral("appid")) {
            appId = val;
            hasUsefulField = true;
        } else if (key.contains(QStringLiteral("internalid")) || key == QStringLiteral("id")) {
            viewId = val;
        } else if (key.contains(QStringLiteral("geometry"))) {
            const QVariantList geom = parseGeometryFromString(val);
            if (geom.size() >= 4) {
                x = geom.at(0).toInt();
                y = geom.at(1).toInt();
                width = qMax(0, geom.at(2).toInt());
                height = qMax(0, geom.at(3).toInt());
            }
        } else if (key.contains(QStringLiteral("desktop")) || key.contains(QStringLiteral("workspace"))) {
            bool ok = false;
            const int d = val.toInt(&ok);
            if (ok && d > 0) {
                space = d;
            }
        } else if (key.contains(QStringLiteral("minimized"))) {
            minimized = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                         val == QStringLiteral("1") ||
                         val.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        } else if (key == QStringLiteral("active") || key.contains(QStringLiteral("focused"))) {
            focused = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                       val == QStringLiteral("1") ||
                       val.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        }
    }

    if (!hasUsefulField) {
        return {};
    }
    appId = canonicalAppId(appId);
    if (viewId.isEmpty()) {
        viewId = QStringLiteral("kwin:%1:%2:%3").arg(appId, title).arg(space);
    }

    return {
        { QStringLiteral("viewId"), viewId },
        { QStringLiteral("title"), title },
        { QStringLiteral("appId"), appId },
        { QStringLiteral("iconSource"), QString() },
        { QStringLiteral("iconName"), QString() },
        { QStringLiteral("x"), x },
        { QStringLiteral("y"), y },
        { QStringLiteral("width"), width },
        { QStringLiteral("height"), height },
        { QStringLiteral("mapped"), true },
        { QStringLiteral("minimized"), minimized },
        { QStringLiteral("focused"), focused },
        { QStringLiteral("space"), space },
        { QStringLiteral("lastEvent"), QStringLiteral("snapshot") },
    };
}

} // namespace

namespace KWinSupportInfoParser {

QVector<QVariantMap> parseSupportInformationDump(const QString &dump, int activeSpace)
{
    QVector<QVariantMap> out;
    if (dump.trimmed().isEmpty()) {
        return out;
    }

    QStringList current;
    const auto flushBlock = [&]() {
        if (current.isEmpty()) {
            return;
        }
        const QVariantMap item = parseSupportWindowBlockNoIcon(current, activeSpace);
        if (!item.isEmpty()) {
            out.push_back(item);
        }
        current.clear();
    };

    const QStringList lines = dump.split('\n');
    bool inClientsSection = false;
    for (const QString &raw : lines) {
        const QString line = normalizeLine(raw);
        if (line.isEmpty()) {
            if (inClientsSection) {
                flushBlock();
            }
            continue;
        }
        const QString lower = line.toLower();
        if (lower.startsWith(QStringLiteral("clients")) ||
            lower.startsWith(QStringLiteral("windows"))) {
            inClientsSection = true;
            continue;
        }
        if (!inClientsSection) {
            continue;
        }
        if (lower.endsWith(QLatin1Char(':')) &&
            !lower.contains(QStringLiteral("caption")) &&
            !lower.contains(QStringLiteral("resource")) &&
            !lower.contains(QStringLiteral("desktop")) &&
            !lower.contains(QStringLiteral("geometry")) &&
            !lower.contains(QStringLiteral("minimized")) &&
            !lower.contains(QStringLiteral("active"))) {
            flushBlock();
            continue;
        }
        current.push_back(line);
    }
    flushBlock();
    return out;
}

} // namespace KWinSupportInfoParser
