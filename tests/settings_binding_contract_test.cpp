#include <QtTest/QtTest>

#include "../src/apps/settings/include/mockbinding.h"
#include "../src/apps/settings/include/networkmanagerbinding.h"

class SettingsBindingContractTest : public QObject
{
    Q_OBJECT

private slots:
    void mockBinding_writeEmitsOperationFinished()
    {
        MockBinding binding(QVariant(1));
        QSignalSpy finishedSpy(&binding, SIGNAL(operationFinished(QString,bool,QString)));
        QSignalSpy valueSpy(&binding, SIGNAL(valueChanged()));
        QCOMPARE(binding.lastError(), QString());
        QVERIFY(!binding.isLoading());

        binding.setValue(2);

        QCOMPARE(binding.value().toInt(), 2);
        QCOMPARE(valueSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 1);
        const QList<QVariant> args = finishedSpy.takeFirst();
        QCOMPARE(args.value(0).toString(), QStringLiteral("write"));
        QCOMPARE(args.value(1).toBool(), true);
        QCOMPARE(args.value(2).toString(), QString());
        QVERIFY(!binding.isLoading());
        QCOMPARE(binding.lastError(), QString());
    }

    void networkManagerBinding_writeUnsupportedReportsError()
    {
        NetworkManagerBinding binding;
        QSignalSpy finishedSpy(&binding, SIGNAL(operationFinished(QString,bool,QString)));
        QSignalSpy errorSpy(&binding, SIGNAL(errorOccurred(QString)));

        binding.setValue(true);

        QCOMPARE(finishedSpy.count(), 1);
        const QList<QVariant> args = finishedSpy.takeFirst();
        QCOMPARE(args.value(0).toString(), QStringLiteral("write"));
        QCOMPARE(args.value(1).toBool(), false);
        QCOMPARE(args.value(2).toString(), QStringLiteral("networkmanager-write-not-supported"));
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(binding.lastError(), QStringLiteral("networkmanager-write-not-supported"));
        QVERIFY(!binding.isLoading());
    }
};

QTEST_GUILESS_MAIN(SettingsBindingContractTest)
#include "settings_binding_contract_test.moc"

