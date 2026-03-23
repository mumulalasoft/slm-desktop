#include <QtTest>

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QLibraryInfo>
#include <QUrl>
#include <memory>

#include "../src/apps/settings/settingsapp.h"

class SettingsUiSmokeTest : public QObject
{
    Q_OBJECT

private:
    static bool hasMissingQtQmlRuntime(const QString &error)
    {
        return error.contains(QStringLiteral("module \"QtQuick\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQuick.Controls\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQuick.Layouts\" is not installed"), Qt::CaseInsensitive) ||
               error.contains(QStringLiteral("module \"QtQuick.Window\" is not installed"), Qt::CaseInsensitive);
    }

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    void settingsMainAndModules_load()
    {
        QQmlApplicationEngine engine;
        const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qtQmlImportsPath.isEmpty()) {
            engine.addImportPath(qtQmlImportsPath);
        }
        engine.addImportPath(QString::fromUtf8(BUILD_QML_IMPORT_ROOT));

        SettingsApp settingsApp(&engine);
        Q_UNUSED(settingsApp);

        QQmlComponent mainComponent(&engine,
                                    QUrl::fromLocalFile(QString::fromUtf8(SETTINGS_QML_MAIN_FILE)));
        if (!mainComponent.isReady() && hasMissingQtQmlRuntime(mainComponent.errorString())) {
            QSKIP("Qt QML runtime modules are unavailable in this environment.");
        }
        QVERIFY2(mainComponent.isReady(), qPrintable(mainComponent.errorString()));
        std::unique_ptr<QObject> window(mainComponent.create(engine.rootContext()));
        QVERIFY2(!!window, qPrintable(mainComponent.errorString()));

        const QString modulesRoot = QString::fromUtf8(SETTINGS_MODULES_DIR);
        const QStringList pages = {
            QStringLiteral("appearance/AppearancePage.qml"),
            QStringLiteral("network/NetworkPage.qml"),
            QStringLiteral("bluetooth/BluetoothPage.qml"),
            QStringLiteral("sound/SoundPage.qml"),
            QStringLiteral("display/DisplayPage.qml"),
            QStringLiteral("power/PowerPage.qml"),
            QStringLiteral("keyboard/KeyboardPage.qml"),
            QStringLiteral("mouse/MousePage.qml"),
            QStringLiteral("applications/ApplicationsPage.qml"),
            QStringLiteral("print/PrintPage.qml"),
            QStringLiteral("permissions/PermissionsPage.qml"),
            QStringLiteral("about/AboutPage.qml")
        };

        for (const QString &page : pages) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(modulesRoot + QLatin1Char('/') + page));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> obj(component.create(engine.rootContext()));
            QVERIFY2(!!obj, qPrintable(component.errorString()));
        }
    }
};

QTEST_MAIN(SettingsUiSmokeTest)
#include "settings_ui_smoke_test.moc"
