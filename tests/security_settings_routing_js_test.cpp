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
    QCOMPARE(networkListen.toString(), QStringLiteral("settings://firewall/mode"));

    const QJSValue socketBind = call(QStringLiteral("socket.bind"));
    QVERIFY2(!socketBind.isError(), qPrintable(socketBind.toString()));
    QCOMPARE(socketBind.toString(), QStringLiteral("settings://firewall/mode"));

    const QJSValue firewallManage = call(QStringLiteral("Firewall.Manage"));
    QVERIFY2(!firewallManage.isError(), qPrintable(firewallManage.toString()));
    QCOMPARE(firewallManage.toString(), QStringLiteral("settings://firewall/mode"));

    const QJSValue secretRead = call(QStringLiteral("Secret.Read"));
    QVERIFY2(!secretRead.isError(), qPrintable(secretRead.toString()));
    QCOMPARE(secretRead.toString(), QStringLiteral("settings://permissions/app-secrets"));

    const QJSValue emptyCapability = call(QString());
    QVERIFY2(!emptyCapability.isError(), qPrintable(emptyCapability.toString()));
    QCOMPARE(emptyCapability.toString(), QStringLiteral("settings://permissions/app-secrets"));
}

QTEST_GUILESS_MAIN(SecuritySettingsRoutingJsTest)
#include "security_settings_routing_js_test.moc"
