#include "../src/printing/core/PrinterCapabilityProvider.h"
#include "../src/printing/core/PrinterManager.h"

#include <QtTest>

using namespace Slm::Print;

class PrinterManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void parseDefaultPrinter();
    void parsePrinterList();
    void parseLpoptionsCapability();
};

void PrinterManagerTest::parseDefaultPrinter()
{
    const QString output = QStringLiteral("system default destination: Office_Printer\n");
    QCOMPARE(PrinterManager::parseDefaultPrinter(output), QStringLiteral("Office_Printer"));
}

void PrinterManagerTest::parsePrinterList()
{
    const QString eOut = QStringLiteral("Office_Printer\nPhoto_Printer\n");
    const QString pOut =
        QStringLiteral("printer Office_Printer is idle. enabled since Thu 01 Jan 1970\n"
                       "printer Photo_Printer disabled since Thu 01 Jan 1970\n");

    const QVariantList printers =
        PrinterManager::parsePrinterList(eOut, pOut, QStringLiteral("Photo_Printer"));
    QCOMPARE(printers.size(), 2);

    const QVariantMap office = printers.at(0).toMap();
    const QVariantMap photo = printers.at(1).toMap();
    QCOMPARE(office.value(QStringLiteral("id")).toString(), QStringLiteral("Office_Printer"));
    QCOMPARE(office.value(QStringLiteral("isDefault")).toBool(), false);
    QCOMPARE(office.value(QStringLiteral("isAvailable")).toBool(), true);

    QCOMPARE(photo.value(QStringLiteral("id")).toString(), QStringLiteral("Photo_Printer"));
    QCOMPARE(photo.value(QStringLiteral("isDefault")).toBool(), true);
    QCOMPARE(photo.value(QStringLiteral("isAvailable")).toBool(), false);
}

void PrinterManagerTest::parseLpoptionsCapability()
{
    const QString lpoptions =
        QStringLiteral("PageSize/Page Size: *A4 Letter Legal\n"
                       "ColorModel/Color Mode: *RGB Gray\n"
                       "Duplex/2-Sided Printing: *None DuplexNoTumble DuplexTumble\n"
                       "Resolution/Output Resolution: *600dpi 1200dpi\n"
                       "print-quality/Print Quality: *normal draft high\n"
                       "vendor-foo/Foo: *On Off\n");

    const PrinterCapability capability =
        PrinterCapabilityProvider::parseLpoptions(QStringLiteral("Office_Printer"), lpoptions);
    QCOMPARE(capability.printerId, QStringLiteral("Office_Printer"));
    QCOMPARE(capability.supportsColor, true);
    QCOMPARE(capability.supportsDuplex, true);
    QVERIFY(capability.paperSizes.contains(QStringLiteral("A4")));
    QVERIFY(capability.qualityModes.contains(QStringLiteral("normal")));
    QVERIFY(capability.resolutionsDpi.contains(600));
    QVERIFY(capability.vendorExtensions.contains(QStringLiteral("vendor-foo")));
}

QTEST_MAIN(PrinterManagerTest)
#include "printermanager_test.moc"
