#include "../src/printing/core/PrintSettingsModel.h"
#include "../src/printing/core/PrintTicketSerializer.h"

#include <QtTest>

using namespace Slm::Print;

class PrintSettingsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreStable();
    void serializeDeserializeRoundtrip();
    void capabilityClamp();
    void ippSerialization();
};

void PrintSettingsModelTest::defaultsAreStable()
{
    PrintSettingsModel model;
    QCOMPARE(model.copies(), 1);
    QCOMPARE(model.paperSize(), QStringLiteral("A4"));
    QCOMPARE(model.orientation(), QStringLiteral("portrait"));
    QCOMPARE(model.duplex(), QStringLiteral("one-sided"));
    QCOMPARE(model.colorMode(), QStringLiteral("color"));
    QCOMPARE(model.quality(), QStringLiteral("normal"));
    QCOMPARE(model.scale(), 100.0);
}

void PrintSettingsModelTest::serializeDeserializeRoundtrip()
{
    PrintSettingsModel source;
    source.setPrinterId(QStringLiteral("printer-1"));
    source.setCopies(3);
    source.setPageRange(QStringLiteral("1-3"));
    source.setPaperSize(QStringLiteral("Letter"));
    source.setOrientation(QStringLiteral("landscape"));
    source.setDuplex(QStringLiteral("two-sided-long-edge"));
    source.setColorMode(QStringLiteral("monochrome"));
    source.setQuality(QStringLiteral("high"));
    source.setScale(85.0);
    source.setPluginFeatures({ { QStringLiteral("booklet"), true } });

    PrintSettingsModel destination;
    destination.deserialize(source.serialize());

    QCOMPARE(destination.printerId(), QStringLiteral("printer-1"));
    QCOMPARE(destination.copies(), 3);
    QCOMPARE(destination.pageRange(), QStringLiteral("1-3"));
    QCOMPARE(destination.paperSize(), QStringLiteral("Letter"));
    QCOMPARE(destination.orientation(), QStringLiteral("landscape"));
    QCOMPARE(destination.duplex(), QStringLiteral("two-sided-long-edge"));
    QCOMPARE(destination.colorMode(), QStringLiteral("monochrome"));
    QCOMPARE(destination.quality(), QStringLiteral("high"));
    QCOMPARE(destination.scale(), 85.0);
    QCOMPARE(destination.pluginFeatures().value(QStringLiteral("booklet")).toBool(), true);
}

void PrintSettingsModelTest::capabilityClamp()
{
    PrintSettingsModel model;
    model.setPaperSize(QStringLiteral("Legal"));
    model.setQuality(QStringLiteral("photo"));
    model.setColorMode(QStringLiteral("color"));
    model.setDuplex(QStringLiteral("two-sided-short-edge"));

    PrinterCapability cap;
    cap.supportsColor = false;
    cap.supportsDuplex = false;
    cap.paperSizes = { QStringLiteral("A4"), QStringLiteral("Letter") };
    cap.qualityModes = { QStringLiteral("draft"), QStringLiteral("normal") };
    model.applyCapability(cap);

    QCOMPARE(model.colorMode(), QStringLiteral("monochrome"));
    QCOMPARE(model.duplex(), QStringLiteral("one-sided"));
    QCOMPARE(model.paperSize(), QStringLiteral("A4"));
    QCOMPARE(model.quality(), QStringLiteral("draft"));
}

void PrintSettingsModelTest::ippSerialization()
{
    PrintTicket ticket;
    ticket.copies = 2;
    ticket.paperSize = QStringLiteral("A5");
    ticket.orientation = Orientation::Landscape;
    ticket.duplex = DuplexMode::ShortEdge;
    ticket.colorMode = ColorMode::Grayscale;
    ticket.quality = QStringLiteral("high");
    ticket.pageRange = QStringLiteral("2-4");
    ticket.scale = 90.0;
    ticket.pluginFeatures.insert(QStringLiteral("watermark"), QStringLiteral("confidential"));

    const QVariantMap ipp = PrintTicketSerializer::toIppAttributes(ticket);
    QCOMPARE(ipp.value(QStringLiteral("copies")).toInt(), 2);
    QCOMPARE(ipp.value(QStringLiteral("media")).toString(), QStringLiteral("A5"));
    QCOMPARE(ipp.value(QStringLiteral("orientation-requested")).toString(), QStringLiteral("4"));
    QCOMPARE(ipp.value(QStringLiteral("sides")).toString(), QStringLiteral("two-sided-short-edge"));
    QCOMPARE(ipp.value(QStringLiteral("print-color-mode")).toString(), QStringLiteral("monochrome"));
    QCOMPARE(ipp.value(QStringLiteral("print-quality")).toString(), QStringLiteral("high"));
    QCOMPARE(ipp.value(QStringLiteral("x-slm-feature-watermark")).toString(), QStringLiteral("confidential"));
}

QTEST_MAIN(PrintSettingsModelTest)
#include "printsettingsmodel_test.moc"
