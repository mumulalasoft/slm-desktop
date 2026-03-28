#include <QtTest>

#include "src/core/system/missingcomponentcontroller.h"

class MissingComponentControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void installComponentForDomain_rejectsUnsupportedComponent();
    void blockingApi_available();
};

void MissingComponentControllerTest::installComponentForDomain_rejectsUnsupportedComponent()
{
    Slm::System::MissingComponentController controller;
    const QVariantMap result = controller.installComponentForDomain(QStringLiteral("recovery"),
                                                                    QStringLiteral("samba"));
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("unsupported-component"));
}

void MissingComponentControllerTest::blockingApi_available()
{
    Slm::System::MissingComponentController controller;
    const QVariantList blocking = controller.blockingMissingComponentsForDomain(QStringLiteral("filemanager"));
    const bool hasBlocking = controller.hasBlockingMissingForDomain(QStringLiteral("filemanager"));
    QCOMPARE(hasBlocking, !blocking.isEmpty());
}

QTEST_MAIN(MissingComponentControllerTest)
#include "missingcomponentcontroller_test.moc"
