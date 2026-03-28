#include <QtTest>

#include <QLibraryInfo>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

namespace {

bool hasMissingQtQmlRuntime(const QString &error)
{
    return error.contains(QStringLiteral("module \"QtQuick\" is not installed"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("module \"QtQuick.Controls\" is not installed"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("module \"QtQuick.Layouts\" is not installed"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("module \"QtQuick.Window\" is not installed"), Qt::CaseInsensitive);
}

class FakeComponentHealth final : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QVariantList missingComponentsForDomain(const QString &domain) const
    {
        if (domain.trimmed().toLower() != QStringLiteral("network") || m_installed) {
            return {};
        }
        return {
            QVariantMap{
                {QStringLiteral("componentId"), QStringLiteral("iproute2")},
                {QStringLiteral("title"), QStringLiteral("IPRoute2")},
                {QStringLiteral("description"), QStringLiteral("Network route tools missing.")},
                {QStringLiteral("guidance"), QStringLiteral("Install iproute2 package.")},
                {QStringLiteral("autoInstallable"), true},
                {QStringLiteral("severity"), QStringLiteral("required")},
            }
        };
    }

    Q_INVOKABLE bool hasBlockingMissingForDomain(const QString &domain) const
    {
        return domain.trimmed().toLower() == QStringLiteral("network") && !m_installed;
    }

    Q_INVOKABLE QVariantMap installComponent(const QString &componentId)
    {
        if (componentId.trimmed().toLower() != QStringLiteral("iproute2")) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unsupported-component")},
            };
        }
        m_installed = true;
        return {
            {QStringLiteral("ok"), true},
        };
    }

private:
    bool m_installed = false;
};

} // namespace

class SettingsMissingComponentsUiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    void warningInstallRefresh_flowRestoresUi()
    {
        QQmlApplicationEngine engine;
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            engine.addImportPath(qtQmlImportsPath);
        }
        FakeComponentHealth componentHealth;
        engine.rootContext()->setContextProperty(QStringLiteral("ComponentHealth"), &componentHealth);

        static const char kHarnessQml[] = R"QML(
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: 480
    height: 240

    property var componentIssues: []
    property bool hasBlockingIssues: false

    function refreshComponentIssues() {
        componentIssues = ComponentHealth.missingComponentsForDomain("network")
        hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("network")
    }

    Column {
        anchors.fill: parent
        spacing: 8

        Rectangle {
            objectName: "warningCard"
            width: parent.width
            height: 40
            visible: root.componentIssues.length > 0
        }

        Switch {
            objectName: "wifiToggle"
            checked: false
            enabled: !root.hasBlockingIssues
        }

        Button {
            objectName: "installButton"
            text: "Install"
            onClicked: {
                ComponentHealth.installComponent("iproute2")
                root.refreshComponentIssues()
            }
        }
    }

    Component.onCompleted: refreshComponentIssues()
}
)QML";

        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "failed to create temp dir");
        const QString qmlPath = tempDir.path() + QStringLiteral("/SettingsMissingFlow.qml");
        QFile qmlFile(qmlPath);
        QVERIFY2(qmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                 "failed to write harness qml");
        QVERIFY2(qmlFile.write(kHarnessQml) > 0, "failed to write harness qml content");
        qmlFile.close();

        QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
        if (!component.isReady() && hasMissingQtQmlRuntime(component.errorString())) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        QString err = component.errorString();
        if (err.trimmed().isEmpty()) {
            err = QStringLiteral("QQmlComponent failed with status=%1").arg(int(component.status()));
        }
        QVERIFY2(component.isReady(), qPrintable(err));
        std::unique_ptr<QObject> root(component.create(engine.rootContext()));
        QVERIFY2(!!root, qPrintable(component.errorString()));

        QObject *warningCard = root->findChild<QObject *>(QStringLiteral("warningCard"));
        QObject *wifiToggle = root->findChild<QObject *>(QStringLiteral("wifiToggle"));
        QObject *installButton = root->findChild<QObject *>(QStringLiteral("installButton"));
        QVERIFY2(warningCard, "warningCard not found");
        QVERIFY2(wifiToggle, "wifiToggle not found");
        QVERIFY2(installButton, "installButton not found");

        QCOMPARE(root->property("hasBlockingIssues").toBool(), true);
        QCOMPARE(warningCard->property("visible").toBool(), true);
        QCOMPARE(wifiToggle->property("enabled").toBool(), false);

        QVERIFY(QMetaObject::invokeMethod(installButton, "click"));
        QCoreApplication::processEvents();

        QCOMPARE(root->property("hasBlockingIssues").toBool(), false);
        QCOMPARE(root->property("componentIssues").toList().isEmpty(), true);
        QCOMPARE(warningCard->property("visible").toBool(), false);
        QCOMPARE(wifiToggle->property("enabled").toBool(), true);
    }
};

QTEST_MAIN(SettingsMissingComponentsUiTest)
#include "settings_missing_components_ui_test.moc"
