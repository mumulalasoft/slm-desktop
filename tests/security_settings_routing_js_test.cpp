#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>

class SecuritySettingsRoutingJsTest : public QObject
{
    Q_OBJECT

private slots:
    void deepLinkForCapability_contract();
    void firewallQuickBlockUi_contract();
    void firewallPendingPromptBlockUi_contract();
    void firewallResetUi_contract();
    void firewallRuntimePromptUi_contract();
};

void SecuritySettingsRoutingJsTest::deepLinkForCapability_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/Qml/components/overlay/SecuritySettingsRouting.js");
    QVERIFY2(QFileInfo::exists(path), "SecuritySettingsRouting.js is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open SecuritySettingsRouting.js");
    const QString script = QString::fromUtf8(file.readAll());

    QJSEngine engine;
    QJSValue eval = engine.evaluate(script, path);
    QVERIFY2(!eval.isError(), qPrintable(eval.toString()));

    QJSValue fn = engine.globalObject().property(QStringLiteral("deepLinkForCapability"));
    QVERIFY2(fn.isCallable(), "deepLinkForCapability is not callable");

    auto call = [&](const QString &capability) -> QJSValue {
        return fn.call(QJSValueList{QJSValue(capability)});
    };

    const QJSValue networkListen = call(QStringLiteral("Network.Listen"));
    QVERIFY2(!networkListen.isError(), qPrintable(networkListen.toString()));
    QCOMPARE(networkListen.toString(), QStringLiteral("settings://firewall/pending-prompts"));

    const QJSValue socketBind = call(QStringLiteral("socket.bind"));
    QVERIFY2(!socketBind.isError(), qPrintable(socketBind.toString()));
    QCOMPARE(socketBind.toString(), QStringLiteral("settings://firewall/pending-prompts"));

    const QJSValue firewallManage = call(QStringLiteral("Firewall.Manage"));
    QVERIFY2(!firewallManage.isError(), qPrintable(firewallManage.toString()));
    QCOMPARE(firewallManage.toString(), QStringLiteral("settings://firewall/pending-prompts"));

    const QJSValue secretRead = call(QStringLiteral("Secret.Read"));
    QVERIFY2(!secretRead.isError(), qPrintable(secretRead.toString()));
    QCOMPARE(secretRead.toString(), QStringLiteral("settings://permissions/app-secrets"));

    const QJSValue emptyCapability = call(QString());
    QVERIFY2(!emptyCapability.isError(), qPrintable(emptyCapability.toString()));
    QCOMPARE(emptyCapability.toString(), QStringLiteral("settings://permissions/app-secrets"));
}

void SecuritySettingsRoutingJsTest::firewallQuickBlockUi_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/src/apps/settings/modules/firewall/FirewallPage.qml");
    QVERIFY2(QFileInfo::exists(path), "FirewallPage.qml is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open FirewallPage.qml");
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY(text.contains(QStringLiteral("function quickBlockConnectionIp(entry, temporary)")));
    QVERIFY(text.contains(QStringLiteral("function quickBlockConnectionSubnet24(entry)")));
    QVERIFY(text.contains(QStringLiteral("function undoLastQuickBlock()")));
    QVERIFY(text.contains(QStringLiteral("Undo Last Quick Block")));
    QVERIFY(text.contains(QStringLiteral("Quick block expires in %1")));
    QVERIFY(text.contains(QStringLiteral("Block 1h")));
    QVERIFY(text.contains(QStringLiteral("Block Permanent")));
    QVERIFY(text.contains(QStringLiteral("Block Subnet (/24)")));
    QVERIFY(text.contains(QStringLiteral("quickBlockCountdownColor")));
}

void SecuritySettingsRoutingJsTest::firewallPendingPromptBlockUi_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/src/apps/settings/modules/firewall/FirewallPage.qml");
    QVERIFY2(QFileInfo::exists(path), "FirewallPage.qml is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open FirewallPage.qml");
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY(text.contains(QStringLiteral("function pendingPromptTargetIp(item)")));
    QVERIFY(text.contains(QStringLiteral("function pendingPromptEffectiveDecision(item, requestedDecision)")));
    QVERIFY(text.contains(QStringLiteral("function appPolicyTargetScopeLabel(entry)")));
    QVERIFY(text.contains(QStringLiteral("local-only")));
    QVERIFY(text.contains(QStringLiteral("payload.targetScope = \"local\"")));
    QVERIFY(text.contains(QStringLiteral("Application rule saved (allow local-only).")));
    QVERIFY(text.contains(QStringLiteral("function pendingPromptBlockTargetIp(sourceIndex, item, temporary)")));
    QVERIFY(text.contains(QStringLiteral("function pendingPromptBlockTargetSubnet24(sourceIndex, item)")));
    QVERIFY(text.contains(QStringLiteral("Only local network")));
    QVERIFY(text.contains(QStringLiteral("Block IP")));
    QVERIFY(text.contains(QStringLiteral("pendingPromptBlockTargetIp(sourceIndex, promptItem, true)")));
    QVERIFY(text.contains(QStringLiteral("Block /24")));
}

void SecuritySettingsRoutingJsTest::firewallResetUi_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/src/apps/settings/modules/firewall/FirewallPage.qml");
    QVERIFY2(QFileInfo::exists(path), "FirewallPage.qml is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open FirewallPage.qml");
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY(text.contains(QStringLiteral("function resetFirewallConfiguration()")));
    QVERIFY(text.contains(QStringLiteral("Reset Configuration")));
    QVERIFY(text.contains(QStringLiteral("Reset Firewall")));
    QVERIFY(text.contains(QStringLiteral("Reset Firewall Configuration?")));
}

void SecuritySettingsRoutingJsTest::firewallRuntimePromptUi_contract()
{
    const QString mainPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                             + QStringLiteral("/Main.qml");
    QVERIFY2(QFileInfo::exists(mainPath), "Main.qml is missing");

    QFile mainFile(mainPath);
    QVERIFY2(mainFile.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open Main.qml");
    const QString mainText = QString::fromUtf8(mainFile.readAll());
    QVERIFY(mainText.contains(QStringLiteral("OverlayComp.FirewallPromptDialog")));
    QVERIFY(mainText.contains(QStringLiteral("hostRoot: root")));

    const QString dialogPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                               + QStringLiteral("/Qml/components/overlay/FirewallPromptDialog.qml");
    QVERIFY2(QFileInfo::exists(dialogPath), "FirewallPromptDialog.qml is missing");

    QFile dialogFile(dialogPath);
    QVERIFY2(dialogFile.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open FirewallPromptDialog.qml");
    const QString dialogText = QString::fromUtf8(dialogFile.readAll());
    QVERIFY(dialogText.contains(QStringLiteral("FirewallServiceClient.pendingPrompts")));
    QVERIFY(dialogText.contains(QStringLiteral("resolvePendingPrompt(0")));
    QVERIFY(dialogText.contains(QStringLiteral("Only local network")));
    QVERIFY(dialogText.contains(QStringLiteral("Remember this decision")));
    QVERIFY(dialogText.contains(QStringLiteral("Incoming Connection Request")));
    QVERIFY(dialogText.contains(QStringLiteral("Outgoing Connection Request")));
    QVERIFY(dialogText.contains(QStringLiteral("Block IP")));
    QVERIFY(dialogText.contains(QStringLiteral("setIpPolicyDetailed")));
}

QTEST_GUILESS_MAIN(SecuritySettingsRoutingJsTest)
#include "security_settings_routing_js_test.moc"
