#include "../src/printing/core/PrintPreviewModel.h"

#include <QtTest>

using namespace Slm::Print;

class PrintPreviewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void pageBounds();
    void zoomNormalization();
    void cacheKeyChanges();
};

void PrintPreviewModelTest::pageBounds()
{
    PrintPreviewModel model;
    model.setTotalPages(3);
    model.setCurrentPage(0);
    QCOMPARE(model.currentPage(), 1);
    model.setCurrentPage(10);
    QCOMPARE(model.currentPage(), 3);
    model.previousPage();
    QCOMPARE(model.currentPage(), 2);
    model.nextPage();
    QCOMPARE(model.currentPage(), 3);
}

void PrintPreviewModelTest::zoomNormalization()
{
    PrintPreviewModel model;
    model.setZoomMode(QStringLiteral("fitwidth"));
    QCOMPARE(model.zoomMode(), QStringLiteral("fitWidth"));
    model.setZoomMode(QStringLiteral("custom"));
    QCOMPARE(model.zoomMode(), QStringLiteral("custom"));
    model.setZoomFactor(0.01);
    QCOMPARE(model.zoomFactor(), 0.1);
    model.setZoomFactor(9.0);
    QCOMPARE(model.zoomFactor(), 5.0);
}

void PrintPreviewModelTest::cacheKeyChanges()
{
    PrintPreviewModel model;
    model.setDocumentUri(QStringLiteral("file:///tmp/demo.pdf"));
    model.setTotalPages(5);
    const QString key1 = model.previewCacheKey();

    model.setCurrentPage(2);
    QCOMPARE(model.currentPage(), 2);
    const QString key2 = model.previewCacheKey();
    QVERIFY(key2 != key1);

    model.applySettingsDigest({ { QStringLiteral("copies"), 2 }, { QStringLiteral("duplex"), QStringLiteral("one-sided") } });
    const QString key3 = model.previewCacheKey();
    QVERIFY(key3 != key2);

    model.applySettingsDigest({ { QStringLiteral("copies"), 2 }, { QStringLiteral("duplex"), QStringLiteral("one-sided") } });
    const QString key4 = model.previewCacheKey();
    QCOMPARE(key4, key3);
}

QTEST_MAIN(PrintPreviewModelTest)
#include "printpreviewmodel_test.moc"
