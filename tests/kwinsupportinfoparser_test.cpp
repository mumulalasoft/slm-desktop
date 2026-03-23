#include <QtTest/QtTest>

#include "../src/core/workspace/kwinsupportinfoparser.h"

class KWinSupportInfoParserTest : public QObject {
    Q_OBJECT

private slots:
    void parseSupportInformationDump_extractsWindow()
    {
        const QString dump =
            QStringLiteral("Clients:\n")
            + QStringLiteral("  caption: Terminal\n")
            + QStringLiteral("  desktopFileName: org.kde.konsole.desktop\n")
            + QStringLiteral("  geometry: 10,20 1000x700\n")
            + QStringLiteral("  desktop: 2\n")
            + QStringLiteral("  active: true\n")
            + QStringLiteral("\n");

        const QVector<QVariantMap> out = KWinSupportInfoParser::parseSupportInformationDump(dump, 1);
        QCOMPARE(out.size(), 1);
        const QVariantMap w = out.at(0);
        QCOMPARE(w.value(QStringLiteral("title")).toString(), QStringLiteral("Terminal"));
        QCOMPARE(w.value(QStringLiteral("appId")).toString(), QStringLiteral("org.kde.konsole"));
        QCOMPARE(w.value(QStringLiteral("x")).toInt(), 10);
        QCOMPARE(w.value(QStringLiteral("y")).toInt(), 20);
        QCOMPARE(w.value(QStringLiteral("width")).toInt(), 1000);
        QCOMPARE(w.value(QStringLiteral("height")).toInt(), 700);
        QCOMPARE(w.value(QStringLiteral("space")).toInt(), 2);
        QCOMPARE(w.value(QStringLiteral("focused")).toBool(), true);
    }
};

QTEST_MAIN(KWinSupportInfoParserTest)
#include "kwinsupportinfoparser_test.moc"
