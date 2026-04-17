#include "../src/printing/core/PrintSettingsModel.h"
#include "../src/printing/core/PrintTicketSerializer.h"
#include "../src/printing/persistence/PrinterSettingsStore.h"

#include <QTemporaryDir>
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

    // New: collate / staple / punch
    void collateSerialization();
    void staplePunchCapabilityClamp();

    // New: last-used printer persistence
    void lastPrinterIdRoundtrip();
};

void PrintSettingsModelTest::defaultsAreStable()
{
    PrintSettingsModel model;
    QCOMPARE(model.copies(), 1);
    QCOMPARE(model.paperSize(), QStringLiteral("A4"));
    QCOMPARE(model.orientation(), QStringLiteral("portrait"));
    // Neutral naming: "off" not "one-sided"
    QCOMPARE(model.duplex(), QStringLiteral("off"));
    // Neutral naming: "color" not "RGB"
    QCOMPARE(model.colorMode(), QStringLiteral("color"));
    // Default quality: "standard"
    QCOMPARE(model.quality(), QStringLiteral("standard"));
    QCOMPARE(model.scale(), 100.0);
    QCOMPARE(model.collate(), true);
    QCOMPARE(model.staple(), false);
    QCOMPARE(model.punch(), false);
}

void PrintSettingsModelTest::serializeDeserializeRoundtrip()
{
    PrintSettingsModel source;
    source.setPrinterId(QStringLiteral("printer-1"));
    source.setCopies(3);
    source.setPageRange(QStringLiteral("1-3"));
    source.setPaperSize(QStringLiteral("Letter"));
    source.setOrientation(QStringLiteral("landscape"));
    // Accept legacy IPP value on input, normalize to neutral on output
    source.setDuplex(QStringLiteral("two-sided-long-edge"));
    source.setColorMode(QStringLiteral("monochrome"));
    source.setQuality(QStringLiteral("high"));
    source.setScale(85.0);
    source.setCollate(false);
    source.setStaple(true);
    source.setPunch(false);
    source.setPluginFeatures({ { QStringLiteral("booklet"), true } });

    PrintSettingsModel destination;
    destination.deserialize(source.serialize());

    QCOMPARE(destination.printerId(), QStringLiteral("printer-1"));
    QCOMPARE(destination.copies(), 3);
    QCOMPARE(destination.pageRange(), QStringLiteral("1-3"));
    QCOMPARE(destination.paperSize(), QStringLiteral("Letter"));
    QCOMPARE(destination.orientation(), QStringLiteral("landscape"));
    // Output is neutral value, not IPP legacy
    QCOMPARE(destination.duplex(), QStringLiteral("long-edge"));
    QCOMPARE(destination.colorMode(), QStringLiteral("grayscale"));
    QCOMPARE(destination.quality(), QStringLiteral("high"));
    QCOMPARE(destination.scale(), 85.0);
    QCOMPARE(destination.collate(), false);
    QCOMPARE(destination.staple(), true);
    QCOMPARE(destination.punch(), false);
    QCOMPARE(destination.pluginFeatures().value(QStringLiteral("booklet")).toBool(), true);
}

void PrintSettingsModelTest::capabilityClamp()
{
    PrintSettingsModel model;
    model.setPaperSize(QStringLiteral("Legal"));
    model.setQuality(QStringLiteral("photo"));
    model.setColorMode(QStringLiteral("color"));
    model.setDuplex(QStringLiteral("two-sided-short-edge"));
    model.setStaple(true);
    model.setPunch(true);

    PrinterCapability cap;
    cap.supportsColor  = false;
    cap.supportsDuplex = false;
    cap.supportsStaple = false;
    cap.supportsPunch  = false;
    cap.paperSizes     = { QStringLiteral("A4"), QStringLiteral("Letter") };
    cap.qualityModes   = { QStringLiteral("draft"), QStringLiteral("normal") };
    model.applyCapability(cap);

    // Neutral naming
    QCOMPARE(model.colorMode(), QStringLiteral("grayscale"));
    QCOMPARE(model.duplex(), QStringLiteral("off"));
    QCOMPARE(model.paperSize(), QStringLiteral("A4"));
    QCOMPARE(model.quality(), QStringLiteral("draft"));
    // Staple/punch cleared because printer doesn't support them
    QCOMPARE(model.staple(), false);
    QCOMPARE(model.punch(), false);
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
    ticket.collate = true;
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

void PrintSettingsModelTest::collateSerialization()
{
    PrintSettingsModel model;
    model.setCollate(false);
    QCOMPARE(model.collate(), false);

    const QVariantMap payload = model.serialize();
    QCOMPARE(payload.value(QStringLiteral("collate")).toBool(), false);

    // Roundtrip: default collate=true, override via deserialize.
    PrintSettingsModel restored;
    restored.deserialize(payload);
    QCOMPARE(restored.collate(), false);

    // Verify default collate is true (not changed by unrelated deserialize).
    PrintSettingsModel fresh;
    QCOMPARE(fresh.collate(), true);
}

void PrintSettingsModelTest::staplePunchCapabilityClamp()
{
    PrintSettingsModel model;
    model.setStaple(true);
    model.setPunch(true);

    // Printer advertises staple but not punch.
    PrinterCapability cap;
    cap.supportsStaple = true;
    cap.supportsPunch  = false;
    model.applyCapability(cap);

    QCOMPARE(model.staple(), true);   // preserved — printer supports it
    QCOMPARE(model.punch(),  false);  // cleared — printer doesn't support it
}

void PrintSettingsModelTest::lastPrinterIdRoundtrip()
{
    // Use a temp location so the test doesn't touch the real user config.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("HOME", tmp.path().toLocal8Bit());

    PrinterSettingsStore store;
    QVERIFY(store.lastPrinterId().isEmpty());

    store.saveLastPrinterId(QStringLiteral("Office_Printer"));
    QCOMPARE(store.lastPrinterId(), QStringLiteral("Office_Printer"));

    // Overwrite with a different printer.
    store.saveLastPrinterId(QStringLiteral("Photo_Printer"));
    QCOMPARE(store.lastPrinterId(), QStringLiteral("Photo_Printer"));

    // Empty string is ignored.
    store.saveLastPrinterId(QString());
    QCOMPARE(store.lastPrinterId(), QStringLiteral("Photo_Printer"));
}

QTEST_MAIN(PrintSettingsModelTest)
#include "printsettingsmodel_test.moc"
