#include "../src/printing/core/JobSubmitter.h"
#include "../src/printing/core/PrintJobBuilder.h"
#include "../src/printing/core/PrintSession.h"

#include <QtTest>

using namespace Slm::Print;

class PrintJobBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void buildRejectsMissingInputs();
    void buildAppliesCapabilityConstraints();
    void parseJobId();
};

void PrintJobBuilderTest::buildRejectsMissingInputs()
{
    PrintSession session;
    session.begin(QString(), QStringLiteral("Doc"));
    session.settingsModel()->setPrinterId(QStringLiteral("Office_Printer"));

    const QVariantMap payload = PrintJobBuilder::build(&session);
    QCOMPARE(payload.value(QStringLiteral("success")).toBool(), false);
    QVERIFY(!payload.value(QStringLiteral("error")).toString().isEmpty());
}

void PrintJobBuilderTest::buildAppliesCapabilityConstraints()
{
    PrintSession session;
    session.begin(QStringLiteral("file:///tmp/a.pdf"), QStringLiteral("Doc"));
    session.settingsModel()->setPrinterId(QStringLiteral("Office_Printer"));
    session.settingsModel()->setColorMode(QStringLiteral("color"));
    session.settingsModel()->setDuplex(QStringLiteral("two-sided-long-edge"));
    session.settingsModel()->setPaperSize(QStringLiteral("A3"));
    session.settingsModel()->setQuality(QStringLiteral("ultra"));

    session.setPrinterCapability({
        { QStringLiteral("supportsColor"), false },
        { QStringLiteral("supportsDuplex"), false },
        { QStringLiteral("paperSizes"), QStringList { QStringLiteral("A4"), QStringLiteral("Letter") } },
        { QStringLiteral("qualityModes"), QStringList { QStringLiteral("draft"), QStringLiteral("normal") } },
    });

    const QVariantMap payload = PrintJobBuilder::build(&session);
    QCOMPARE(payload.value(QStringLiteral("success")).toBool(), true);

    const QVariantMap ticket = payload.value(QStringLiteral("ticket")).toMap();
    QCOMPARE(ticket.value(QStringLiteral("colorMode")).toString(), QStringLiteral("monochrome"));
    QCOMPARE(ticket.value(QStringLiteral("duplex")).toString(), QStringLiteral("one-sided"));
    QCOMPARE(ticket.value(QStringLiteral("paperSize")).toString(), QStringLiteral("A4"));
    QCOMPARE(ticket.value(QStringLiteral("quality")).toString(), QStringLiteral("draft"));
}

void PrintJobBuilderTest::parseJobId()
{
    const QString output = QStringLiteral("request id is Office_Printer-42 (1 file(s))");
    QCOMPARE(JobSubmitter::parseJobId(output), QStringLiteral("Office_Printer-42"));
}

QTEST_MAIN(PrintJobBuilderTest)
#include "printjobbuilder_test.moc"
