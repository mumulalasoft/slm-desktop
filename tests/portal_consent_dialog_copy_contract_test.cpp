#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PortalConsentDialogCopyContractTest : public QObject
{
    Q_OBJECT

private slots:
    void consentDialog_secretMicrocopy_contract();
    void shellBridge_settingsDeeplink_contract();
};

void PortalConsentDialogCopyContractTest::consentDialog_secretMicrocopy_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/Qml/components/overlay/PortalConsentDialogWindow.qml");
    QVERIFY2(QFileInfo::exists(path), "PortalConsentDialogWindow.qml is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open PortalConsentDialogWindow.qml");
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY2(text.contains(QStringLiteral("Allow Access to Saved Sign-in Data?")),
             "missing secret read microcopy");
    QVERIFY2(text.contains(QStringLiteral("Open Security Settings")),
             "missing settings shortcut action");
    QVERIFY2(text.contains(QStringLiteral("Open Firewall Settings")),
             "missing firewall-specific settings shortcut action");
    QVERIFY2(text.contains(QStringLiteral("readonly property bool isNetworkCapability")),
             "missing network capability classification contract");
    QVERIFY2(text.contains(QStringLiteral("I understand this can remove saved sign-in data for this app.")),
             "missing destructive confirmation copy");
    QVERIFY2(text.contains(QStringLiteral("readonly property bool persistentEligible")),
             "missing consent persistence eligibility contract");
    QVERIFY2(text.contains(QStringLiteral("visible: root.persistentEligible")),
             "missing always-allow visibility gate");
    QVERIFY2(text.contains(QStringLiteral("enabled: !root.isSecretDeleteCapability || root.deleteAlwaysConfirm")),
             "missing Secret.Delete always-allow guard");
}

void PortalConsentDialogCopyContractTest::shellBridge_settingsDeeplink_contract()
{
    const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                         + QStringLiteral("/Qml/components/overlay/ShellServiceBridge.qml");
    QVERIFY2(QFileInfo::exists(path), "ShellServiceBridge.qml is missing");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open ShellServiceBridge.qml");
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY2(text.contains(QStringLiteral("function openConsentSettings()")),
             "missing openConsentSettings helper");
    QVERIFY2(text.contains(QStringLiteral("settings://permissions/app-secrets")),
             "missing settings deep-link target");
    QVERIFY2(text.contains(QStringLiteral("settings://firewall/mode")),
             "missing firewall settings deep-link target");
    QVERIFY2(text.contains(QStringLiteral("capabilityLower.indexOf(\"network\")")),
             "missing capability-based deeplink routing");
    QVERIFY2(text.contains(QStringLiteral("portal-consent-open-settings")),
             "missing execution source tag for consent settings jump");
}

QTEST_GUILESS_MAIN(PortalConsentDialogCopyContractTest)
#include "portal_consent_dialog_copy_contract_test.moc"
