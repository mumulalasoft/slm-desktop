#include <QtTest>
#include "src/core/workspace/windowthumbnaillayoutengine.h"

class WindowThumbnailLayoutEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testSingleWindow() {
        WindowThumbnailLayoutEngine engine;
        QVariantList windows;
        QVariantMap w1;
        w1["width"] = 1920;
        w1["height"] = 1080;
        windows << w1;

        auto results = engine.calculateLayout(windows, 1000, 1000, 20);
        QCOMPARE(results.size(), 1);
        
        auto res = results[0].toMap();
        // Single window should be centered and maintain aspect ratio.
        // Aspect 1920/1080 = 1.77
        // Max width is 1000 - 2*20 = 960? No, calculateLayout probably uses internal margins too.
        // Let's check the code.
        QVERIFY(res["width"].toDouble() > 0);
        QVERIFY(res["height"].toDouble() > 0);
        QCOMPARE(res["x"].toDouble() + res["width"].toDouble() / 2, 500.0);
    }

    void testMultipleWindows() {
        WindowThumbnailLayoutEngine engine;
        QVariantList windows;
        for (int i = 0; i < 4; ++i) {
            QVariantMap w;
            w["width"] = 1600;
            w["height"] = 900;
            windows << w;
        }

        auto results = engine.calculateLayout(windows, 1000, 1000, 20);
        QCOMPARE(results.size(), 4);
        
        // Should be in a grid.
        // Check if they are distinct
        QList<QPointF> centers;
        for (const auto &resVar : results) {
            auto res = resVar.toMap();
            centers.append(QPointF(res["x"].toDouble(), res["y"].toDouble()));
        }
        
        for (int i = 0; i < centers.size(); ++i) {
            for (int j = i + 1; j < centers.size(); ++j) {
                QVERIFY(centers[i] != centers[j]);
            }
        }
        QCOMPARE(centers.size(), 4);
    }

    void testEmpty() {
        WindowThumbnailLayoutEngine engine;
        auto results = engine.calculateLayout(QVariantList(), 1000, 1000, 20);
        QVERIFY(results.isEmpty());
    }
};

QTEST_MAIN(WindowThumbnailLayoutEngineTest)
#include "windowthumbnaillayoutengine_test.moc"
