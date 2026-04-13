#include <QtTest/QtTest>

#include "../src/daemon/recoveryd/recoveryservice.h"
#include "../src/login/libslmlogin/slmconfigmanager.h"
#include "../src/login/libslmlogin/slmsessionstate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

class RecoveryServiceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        const QString testName = QString::fromLatin1(QTest::currentTestFunction());
        m_root = QDir::tempPath()
                + QStringLiteral("/slm-recoveryd-test-")
                + QString::number(QCoreApplication::applicationPid())
                + QStringLiteral("-")
                + testName;
        QDir(m_root).removeRecursively();
        QDir().mkpath(m_root);
        qputenv("XDG_CONFIG_HOME", m_root.toUtf8());
    }

    void cleanup()
    {
        QDir(m_root).removeRecursively();
    }

    void ping_contract()
    {
        RecoveryService service;
        const QVariantMap ping = service.Ping();
        QCOMPARE(ping.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(ping.value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.Recovery"));
        QCOMPARE(ping.value(QStringLiteral("apiVersion")).toString(), QStringLiteral("1.0"));
    }

    void request_safe_mode_persists_state()
    {
        RecoveryService service;
        const QVariantMap result = service.RequestSafeMode(QStringLiteral("manual-test"));
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);

        Slm::Login::SessionState state;
        QString error;
        QVERIFY2(Slm::Login::SessionStateIO::load(state, error), qPrintable(error));
        QCOMPARE(state.safeModeForced, true);
        QCOMPARE(state.recoveryReason, QStringLiteral("manual-test"));
    }

    void crash_loop_triggers_auto_recovery()
    {
        RecoveryService service;
        QVariantMap result;
        for (int i = 0; i < 4; ++i) {
            result = service.ReportCrash(QStringLiteral("compositor"), QStringLiteral("simulated"));
        }

        QCOMPARE(result.value(QStringLiteral("action")).toString(), QStringLiteral("auto-recovery"));
        QCOMPARE(result.value(QStringLiteral("safeModeRequested")).toBool(), true);

        Slm::Login::SessionState state;
        QString error;
        QVERIFY2(Slm::Login::SessionStateIO::load(state, error), qPrintable(error));
        QCOMPARE(state.safeModeForced, true);
        QVERIFY(state.recoveryReason.contains(QStringLiteral("crash-loop"))
                || state.recoveryReason.contains(QStringLiteral("simulated")));
    }

    void recovery_partition_marker_roundtrip()
    {
        RecoveryService service;
        const QVariantMap request = service.RequestRecoveryPartition(QStringLiteral("root-corruption"));
        QCOMPARE(request.value(QStringLiteral("ok")).toBool(), true);

        const QString marker = Slm::Login::ConfigManager::configDir()
                + QStringLiteral("/recovery-partition-request.json");
        QVERIFY(QFileInfo::exists(marker));

        const QVariantMap cleared = service.ClearRecoveryFlags();
        QCOMPARE(cleared.value(QStringLiteral("ok")).toBool(), true);
        QVERIFY(!QFileInfo::exists(marker));

        Slm::Login::SessionState state;
        QString error;
        QVERIFY2(Slm::Login::SessionStateIO::load(state, error), qPrintable(error));
        QCOMPARE(state.safeModeForced, false);
    }

private:
    QString m_root;
};

QTEST_GUILESS_MAIN(RecoveryServiceContractTest)
#include "recoveryservice_contract_test.moc"
