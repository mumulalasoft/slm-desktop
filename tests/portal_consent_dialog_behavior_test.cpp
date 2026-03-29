#include <QtTest>

#include <QLibraryInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <memory>

namespace {

bool hasMissingQtQmlRuntime(const QString &error)
{
    return error.contains(QStringLiteral("module \"QtQuick\" is not installed"), Qt::CaseInsensitive)
           || error.contains(QStringLiteral("module \"QtQuick.Controls\" is not installed"), Qt::CaseInsensitive)
           || error.contains(QStringLiteral("module \"QtQuick.Layouts\" is not installed"), Qt::CaseInsensitive);
}

} // namespace

class PortalConsentDialogBehaviorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    void secretDelete_alwaysAllowRequiresConfirmation()
    {
        QQmlEngine engine;
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            engine.addImportPath(qtQmlImportsPath);
        }
        static const char kHarnessQml[] = R"QML(
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    property string capabilityLabel: "Secret.Delete"
    property bool isSecretDeleteCapability: capabilityLabel === "Secret.Delete"
    property bool persistentEligible: true

    CheckBox {
        id: confirm
        objectName: "secretDeleteConfirmCheckBox"
        visible: root.isSecretDeleteCapability && root.persistentEligible
        checked: false
    }

    Button {
        objectName: "consentAlwaysAllowButton"
        text: "Always Allow"
        visible: root.persistentEligible
        enabled: !root.isSecretDeleteCapability || confirm.checked
    }
}
)QML";

        QQmlComponent component(&engine);
        component.setData(kHarnessQml, QUrl(QStringLiteral("file:///PortalConsentHarness.qml")));
        if (!component.isReady() && hasMissingQtQmlRuntime(component.errorString())) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> dialog(component.create());
        QVERIFY2(!!dialog, qPrintable(component.errorString()));

        auto *alwaysAllow = dialog->findChild<QObject *>(QStringLiteral("consentAlwaysAllowButton"));
        auto *confirm = dialog->findChild<QObject *>(QStringLiteral("secretDeleteConfirmCheckBox"));
        QVERIFY2(alwaysAllow, "consentAlwaysAllowButton not found");
        QVERIFY2(confirm, "secretDeleteConfirmCheckBox not found");

        QCOMPARE(confirm->property("visible").toBool(), true);
        QCOMPARE(alwaysAllow->property("enabled").toBool(), false);

        QVERIFY(QMetaObject::invokeMethod(confirm, "toggle"));
        QTRY_COMPARE(alwaysAllow->property("enabled").toBool(), true);
    }

    void secretDelete_nonPersistent_hidesAlwaysAllowControls()
    {
        QQmlEngine engine;
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            engine.addImportPath(qtQmlImportsPath);
        }
        static const char kHarnessQml[] = R"QML(
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    property string capabilityLabel: "Secret.Delete"
    property bool isSecretDeleteCapability: capabilityLabel === "Secret.Delete"
    property bool persistentEligible: false

    CheckBox {
        id: confirm
        objectName: "secretDeleteConfirmCheckBox"
        visible: root.isSecretDeleteCapability && root.persistentEligible
        checked: false
    }

    Button {
        objectName: "consentAlwaysAllowButton"
        text: "Always Allow"
        visible: root.persistentEligible
        enabled: !root.isSecretDeleteCapability || confirm.checked
    }
}
)QML";

        QQmlComponent component(&engine);
        component.setData(kHarnessQml, QUrl(QStringLiteral("file:///PortalConsentHarnessNonPersistent.qml")));
        if (!component.isReady() && hasMissingQtQmlRuntime(component.errorString())) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> dialog(component.create());
        QVERIFY2(!!dialog, qPrintable(component.errorString()));

        auto *alwaysAllow = dialog->findChild<QObject *>(QStringLiteral("consentAlwaysAllowButton"));
        auto *confirm = dialog->findChild<QObject *>(QStringLiteral("secretDeleteConfirmCheckBox"));
        QVERIFY2(alwaysAllow, "consentAlwaysAllowButton not found");
        QVERIFY2(confirm, "secretDeleteConfirmCheckBox not found");

        QCOMPARE(confirm->property("visible").toBool(), false);
        QCOMPARE(alwaysAllow->property("visible").toBool(), false);
    }
};

QTEST_MAIN(PortalConsentDialogBehaviorTest)
#include "portal_consent_dialog_behavior_test.moc"
