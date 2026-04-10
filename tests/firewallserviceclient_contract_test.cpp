#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class FirewallServiceClientContractTest : public QObject
{
    Q_OBJECT

private slots:
    void confirmBatchTriagePreference_contract()
    {
        const QString base = QString::fromUtf8(QT_TESTCASE_SOURCEDIR);
        QVERIFY2(!base.trimmed().isEmpty(), "DESKTOP_SOURCE_DIR is empty");

        const QString headerPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.h");
        const QString cppPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.cpp");

        QVERIFY2(QFileInfo::exists(headerPath), qPrintable(QStringLiteral("missing file: %1").arg(headerPath)));
        QVERIFY2(QFileInfo::exists(cppPath), qPrintable(QStringLiteral("missing file: %1").arg(cppPath)));

        QFile headerFile(headerPath);
        QVERIFY2(headerFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(headerPath)));
        const QString header = QString::fromUtf8(headerFile.readAll());

        QFile cppFile(cppPath);
        QVERIFY2(cppFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(cppPath)));
        const QString cpp = QString::fromUtf8(cppFile.readAll());

        QVERIFY2(header.contains(QStringLiteral("Q_PROPERTY(bool confirmBatchTriagePreset READ confirmBatchTriagePreset WRITE setConfirmBatchTriagePreset NOTIFY triagePreferencesChanged)")),
                 "missing confirmBatchTriagePreset Q_PROPERTY contract");
        QVERIFY2(header.contains(QStringLiteral("void triagePreferencesChanged();")),
                 "missing triagePreferencesChanged signal contract");
        QVERIFY2(cpp.contains(QStringLiteral("kConfirmBatchTriagePresetPath[] = \"firewall.pendingTriage.confirmBatchPreset\"")),
                 "missing persistent setting key contract");
        QVERIFY2(cpp.contains(QStringLiteral("setSettingsValue(QString::fromLatin1(kConfirmBatchTriagePresetPath), enabled);")),
                 "missing persistence write-through for confirmBatchTriagePreset");
        QVERIFY2(cpp.contains(QStringLiteral("valueByPath(settings,")),
                 "missing restore read call from settings payload");
        QVERIFY2(cpp.contains(QStringLiteral("QString::fromLatin1(kConfirmBatchTriagePresetPath)")),
                 "missing restore key usage for confirmBatchTriagePreset");
    }

    void pendingPromptSpamThrottle_contract()
    {
        const QString base = QString::fromUtf8(QT_TESTCASE_SOURCEDIR);
        QVERIFY2(!base.trimmed().isEmpty(), "DESKTOP_SOURCE_DIR is empty");

        const QString headerPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.h");
        const QString cppPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.cpp");

        QVERIFY2(QFileInfo::exists(headerPath), qPrintable(QStringLiteral("missing file: %1").arg(headerPath)));
        QVERIFY2(QFileInfo::exists(cppPath), qPrintable(QStringLiteral("missing file: %1").arg(cppPath)));

        QFile headerFile(headerPath);
        QVERIFY2(headerFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(headerPath)));
        const QString header = QString::fromUtf8(headerFile.readAll());

        QFile cppFile(cppPath);
        QVERIFY2(cppFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(cppPath)));
        const QString cpp = QString::fromUtf8(cppFile.readAll());

        QVERIFY2(header.contains(QStringLiteral("QString promptThrottleKey(const QVariantMap &request, const QVariantMap &evaluation) const;")),
                 "missing prompt throttle key function contract");
        QVERIFY2(header.contains(QStringLiteral("QHash<QString, qint64> m_lastPromptEpochByKey;")),
                 "missing prompt throttle cache member contract");
        QVERIFY2(cpp.contains(QStringLiteral("kPromptDuplicateSuppressSecs = 8")),
                 "missing prompt suppress window constant contract");
        QVERIFY2(cpp.contains(QStringLiteral("const QString throttleKey = promptThrottleKey(request, evaluation);")),
                 "missing prompt throttle key usage in onConnectionPromptRequested");
        QVERIFY2(cpp.contains(QStringLiteral("inSuppressWindow")),
                 "missing suppress-window branching contract");
        QVERIFY2(cpp.contains(QStringLiteral("row.insert(QStringLiteral(\"throttleKey\"), throttleKey);")),
                 "missing throttleKey persistence on pending row");
    }

    void policyChangedRealtimeSync_contract()
    {
        const QString base = QString::fromUtf8(QT_TESTCASE_SOURCEDIR);
        QVERIFY2(!base.trimmed().isEmpty(), "DESKTOP_SOURCE_DIR is empty");

        const QString headerPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.h");
        const QString cppPath = base
                + QStringLiteral("/src/apps/settings/modules/network/firewallserviceclient.cpp");

        QVERIFY2(QFileInfo::exists(headerPath), qPrintable(QStringLiteral("missing file: %1").arg(headerPath)));
        QVERIFY2(QFileInfo::exists(cppPath), qPrintable(QStringLiteral("missing file: %1").arg(cppPath)));

        QFile headerFile(headerPath);
        QVERIFY2(headerFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(headerPath)));
        const QString header = QString::fromUtf8(headerFile.readAll());

        QFile cppFile(cppPath);
        QVERIFY2(cppFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(cppPath)));
        const QString cpp = QString::fromUtf8(cppFile.readAll());

        QVERIFY2(header.contains(QStringLiteral("void onPolicyChanged(const QVariantMap &change);")),
                 "missing onPolicyChanged slot declaration");
        QVERIFY2(cpp.contains(QStringLiteral("void FirewallServiceClient::onPolicyChanged(const QVariantMap &change)")),
                 "missing onPolicyChanged slot implementation");
        QVERIFY2(cpp.contains(QStringLiteral("QStringLiteral(\"PolicyChanged\")")),
                 "missing dbus policy changed subscription");
        QVERIFY2(cpp.contains(QStringLiteral("SLOT(onPolicyChanged(QVariantMap))")),
                 "missing onPolicyChanged slot dbus wiring");
    }

    void triageDontAskAgainUi_contract()
    {
        const QString base = QString::fromUtf8(QT_TESTCASE_SOURCEDIR);
        QVERIFY2(!base.trimmed().isEmpty(), "DESKTOP_SOURCE_DIR is empty");

        const QString qmlPath = base
                + QStringLiteral("/src/apps/settings/modules/firewall/FirewallPage.qml");
        QVERIFY2(QFileInfo::exists(qmlPath), qPrintable(QStringLiteral("missing file: %1").arg(qmlPath)));

        QFile file(qmlPath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("cannot open: %1").arg(qmlPath)));
        const QString text = QString::fromUtf8(file.readAll());

        QVERIFY2(text.contains(QStringLiteral("Don't ask again for triage batch actions")),
                 "missing triage dont-ask-again checkbox copy");
        QVERIFY2(text.contains(QStringLiteral("FirewallServiceClient.confirmBatchTriagePreset = false")),
                 "missing dont-ask-again persistence binding");
        QVERIFY2(text.contains(QStringLiteral("visible: !FirewallServiceClient.confirmBatchTriagePreset")),
                 "missing re-enable-confirm affordance contract");
    }
};

QTEST_GUILESS_MAIN(FirewallServiceClientContractTest)
#include "firewallserviceclient_contract_test.moc"
