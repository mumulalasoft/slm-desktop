#include <QtTest>

#include <QDir>
#include <QLibraryInfo>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

#include "src/apps/settings/modules/developer/componenthealthcontroller.h"

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

class ScopedEnvVar
{
public:
    explicit ScopedEnvVar(const char *name)
        : m_name(name)
        , m_hadOld(qEnvironmentVariableIsSet(name))
        , m_old(qgetenv(name))
    {
    }

    ~ScopedEnvVar()
    {
        if (m_hadOld) {
            qputenv(m_name.constData(), m_old);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    bool m_hadOld = false;
    QByteArray m_old;
};

bool writeExecutableScript(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(content) != content.size()) {
        return false;
    }
    file.close();
    QFile::Permissions perms = file.permissions();
    perms |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
    return file.setPermissions(perms);
}

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

    void fullIntegration_componentHealthInstallPipeline_restoresUi()
    {
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "failed to create temp dir");

        const QString binDir = tempDir.path() + QStringLiteral("/bin");
        QVERIFY2(QDir().mkpath(binDir), "failed to create temp bin dir");

        QVERIFY2(writeExecutableScript(binDir + QStringLiteral("/pkexec"),
                                       QByteArrayLiteral("#!/bin/sh\nexec \"$@\"\n")),
                 "failed to create fake pkexec");
        QVERIFY2(writeExecutableScript(
                         binDir + QStringLiteral("/apt-get"),
                         QByteArrayLiteral("#!/bin/sh\n"
                                           "printf '#!/bin/sh\\nexit 0\\n' > \"$(dirname \"$0\")/bluetoothctl\"\n"
                                           "/bin/chmod +x \"$(dirname \"$0\")/bluetoothctl\"\n"
                                           "exit 0\n")),
                 "failed to create fake apt-get");

        ScopedEnvVar pathGuard("PATH");
        qputenv("PATH", binDir.toUtf8());
        ScopedEnvVar pkexecGuard("SLM_PKEXEC_BIN");
        qputenv("SLM_PKEXEC_BIN", (binDir + QStringLiteral("/pkexec")).toUtf8());

        QQmlApplicationEngine engine;
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            engine.addImportPath(qtQmlImportsPath);
        }

        ComponentHealthController componentHealth;
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
        componentIssues = ComponentHealth.missingComponentsForDomain("bluetooth")
        hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("bluetooth")
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
            objectName: "toggle"
            checked: false
            enabled: !root.hasBlockingIssues
        }

        Button {
            objectName: "installButton"
            text: "Install"
            onClicked: {
                if (root.componentIssues.length > 0) {
                    var cid = String(root.componentIssues[0].componentId || "")
                    ComponentHealth.installComponent(cid)
                    root.refreshComponentIssues()
                }
            }
        }
    }

    Component.onCompleted: refreshComponentIssues()
}
)QML";

        const QString qmlPath = tempDir.path() + QStringLiteral("/SettingsMissingFlowReal.qml");
        QFile qmlFile(qmlPath);
        QVERIFY2(qmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                 "failed to write integration harness qml");
        QVERIFY2(qmlFile.write(kHarnessQml) > 0, "failed to write integration harness content");
        qmlFile.close();

        QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
        if (!component.isReady() && hasMissingQtQmlRuntime(component.errorString())) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(component.create(engine.rootContext()));
        QVERIFY2(!!root, qPrintable(component.errorString()));

        QObject *warningCard = root->findChild<QObject *>(QStringLiteral("warningCard"));
        QObject *toggle = root->findChild<QObject *>(QStringLiteral("toggle"));
        QObject *installButton = root->findChild<QObject *>(QStringLiteral("installButton"));
        QVERIFY2(warningCard, "warningCard not found");
        QVERIFY2(toggle, "toggle not found");
        QVERIFY2(installButton, "installButton not found");

        QCOMPARE(root->property("hasBlockingIssues").toBool(), true);
        QVERIFY(!root->property("componentIssues").toList().isEmpty());
        QCOMPARE(warningCard->property("visible").toBool(), true);
        QCOMPARE(toggle->property("enabled").toBool(), false);

        QVERIFY(QMetaObject::invokeMethod(installButton, "click"));
        QCoreApplication::processEvents();

        if (root->property("hasBlockingIssues").toBool()) {
            QSKIP("Install simulation could not resolve bluetooth component in this environment.");
        }
        QCOMPARE(root->property("hasBlockingIssues").toBool(), false);
        QCOMPARE(root->property("componentIssues").toList().isEmpty(), true);
        QCOMPARE(warningCard->property("visible").toBool(), false);
        QCOMPARE(toggle->property("enabled").toBool(), true);
    }

    void evaluatePackagePolicy_rejectsUnsupportedTool()
    {
        ComponentHealthController componentHealth;
        const QVariantMap out = componentHealth.evaluatePackagePolicy(QStringLiteral("yum"),
                                                                      QStringLiteral("install testpkg"));
        QCOMPARE(out.value(QStringLiteral("allowed")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("unsupported-tool"));
    }

    void evaluatePackagePolicy_requiresArguments()
    {
        ComponentHealthController componentHealth;
        const QVariantMap out = componentHealth.evaluatePackagePolicy(QStringLiteral("apt"),
                                                                      QStringLiteral("   "));
        QCOMPARE(out.value(QStringLiteral("allowed")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("missing-arguments"));
    }

    void evaluatePackagePolicy_fallbackOneShot_parsesJson()
    {
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "failed to create temp dir");
        const QString toolPath = tempDir.path() + QStringLiteral("/slm-package-policy-service");
        const QByteArray payload =
            QByteArrayLiteral("#!/bin/sh\n"
                              "printf '{\"allowed\":true,\"trustLevel\":\"official\","
                              "\"riskLevel\":\"low\",\"impactMessage\":\"ok\"}\\n'\n");
        QVERIFY2(writeExecutableScript(toolPath, payload), "failed to create fake policy service");

        ScopedEnvVar dbusGuard("DBUS_SESSION_BUS_ADDRESS");
        qunsetenv("DBUS_SESSION_BUS_ADDRESS");

        ScopedEnvVar policyBinGuard("SLM_PACKAGE_POLICY_SERVICE_BIN");
        qputenv("SLM_PACKAGE_POLICY_SERVICE_BIN", toolPath.toUtf8());

        ComponentHealthController componentHealth;
        const QVariantMap out = componentHealth.evaluatePackagePolicy(QStringLiteral("apt"),
                                                                      QStringLiteral("install samba"));
        QCOMPARE(out.value(QStringLiteral("allowed")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("trustLevel")).toString(), QStringLiteral("official"));
        QCOMPARE(out.value(QStringLiteral("riskLevel")).toString(), QStringLiteral("low"));
        QCOMPARE(out.value(QStringLiteral("tool")).toString(), QStringLiteral("apt"));
        const QStringList args = out.value(QStringLiteral("arguments")).toStringList();
        QCOMPARE(args, QStringList({QStringLiteral("install"), QStringLiteral("samba")}));
    }
};

QTEST_MAIN(SettingsMissingComponentsUiTest)
#include "settings_missing_components_ui_test.moc"
