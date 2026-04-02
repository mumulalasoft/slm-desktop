#include <QtTest/QtTest>

#include "../tothespottexthighlighter.h"

class TothespotTextHighlighterTest : public QObject
{
    Q_OBJECT

private slots:
    void noQuery_returnsEscapedPlainText()
    {
        const QString in = QStringLiteral("A < B & \"C\" 'D'");
        const QString out = TothespotTextHighlighter().highlightStyledText(
            in, QString(), QStringLiteral("#2266cc"));
        QCOMPARE(out, QStringLiteral("A &lt; B &amp; &quot;C&quot; &#39;D&#39;"));
    }

    void caseInsensitive_highlightsMatch()
    {
        TothespotTextHighlighter h;
        const QString out = h.highlightStyledText(QStringLiteral("GNU Image Manipulation Program"),
                                                  QStringLiteral("image"),
                                                  QStringLiteral("#2266cc"));
        QVERIFY(out.contains(QStringLiteral("<span style=\"font-weight:700;color:#2266cc;\">Image</span>")));
    }

    void escapesHtmlInsideHighlightedSpan()
    {
        TothespotTextHighlighter h;
        const QString out = h.highlightStyledText(QStringLiteral("a <tag> b"),
                                                  QStringLiteral("<tag>"),
                                                  QStringLiteral("#112233"));
        QVERIFY(out.contains(QStringLiteral("<span style=\"font-weight:700;color:#112233;\">&lt;tag&gt;</span>")));
    }

    void snapshotMatrix_data()
    {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("query");
        QTest::addColumn<QString>("accent");
        QTest::addColumn<QString>("expected");

        QTest::newRow("single-match")
            << QStringLiteral("Calculator")
            << QStringLiteral("calc")
            << QStringLiteral("#2255aa")
            << QStringLiteral("<span style=\"font-weight:700;color:#2255aa;\">Calc</span>ulator");

        QTest::newRow("multi-match-case-insensitive")
            << QStringLiteral("Gimp gIMP")
            << QStringLiteral("gimp")
            << QStringLiteral("#0099ff")
            << QStringLiteral("<span style=\"font-weight:700;color:#0099ff;\">Gimp</span> "
                              "<span style=\"font-weight:700;color:#0099ff;\">gIMP</span>");

        QTest::newRow("no-match-escaped")
            << QStringLiteral("A&B <C>")
            << QStringLiteral("zzz")
            << QStringLiteral("#ff0000")
            << QStringLiteral("A&amp;B &lt;C&gt;");

        QTest::newRow("empty-accent-fallback")
            << QStringLiteral("Finder")
            << QStringLiteral("find")
            << QString()
            << QStringLiteral("<span style=\"font-weight:700;color:#1d6fd8;\">Find</span>er");
    }

    void snapshotMatrix()
    {
        QFETCH(QString, title);
        QFETCH(QString, query);
        QFETCH(QString, accent);
        QFETCH(QString, expected);

        TothespotTextHighlighter h;
        QCOMPARE(h.highlightStyledText(title, query, accent), expected);
    }

    void htmlSanity_balancedAndEscaped()
    {
        TothespotTextHighlighter h;
        const QString out = h.highlightStyledText(
            QStringLiteral("<<script>alert(1)</script> gimp & gimp"),
            QStringLiteral("gimp"),
            QStringLiteral("#3355aa"));

        const int openSpanCount = out.count(QStringLiteral("<span "));
        const int closeSpanCount = out.count(QStringLiteral("</span>"));
        QCOMPARE(openSpanCount, closeSpanCount);
        QVERIFY(openSpanCount >= 1);

        QVERIFY(!out.contains(QStringLiteral("<script>")));
        QVERIFY(!out.contains(QStringLiteral("</script>")));
        QVERIFY(out.contains(QStringLiteral("&lt;script&gt;alert(1)&lt;/script&gt;")));
    }
};

QTEST_MAIN(TothespotTextHighlighterTest)
#include "tothespot_texthighlighter_test.moc"
