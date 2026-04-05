#include <QtTest/QtTest>

#include <QFile>

namespace {
QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
}

class AppearanceContextTimeUiContractTest : public QObject
{
    Q_OBJECT

private slots:
    void appearancePage_exposesContextTimeControls()
    {
        const QString path = QFINDTESTDATA("src/apps/settings/modules/appearance/AppearancePage.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("id: contextTimeModeCombo")));
        QVERIFY(text.contains(QStringLiteral("DesktopSettings.contextTimeMode")));
        QVERIFY(text.contains(QStringLiteral("DesktopSettings.setContextTimeMode(entry.key)")));
        QVERIFY(text.contains(QStringLiteral("DesktopSettings.setContextTimeSunriseHour(value)")));
        QVERIFY(text.contains(QStringLiteral("DesktopSettings.setContextTimeSunsetHour(value)")));
    }

    void appearancePage_sunHourEditors_followMode()
    {
        const QString path = QFINDTESTDATA("src/apps/settings/modules/appearance/AppearancePage.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("enabled: DesktopSettings.contextTimeMode === \"sun\"")));
        QVERIFY(text.contains(QStringLiteral("opacity: enabled ? 1.0 : Theme.opacityHint")));
        QVERIFY(text.contains(QStringLiteral("id: sunriseHourSpin")));
        QVERIFY(text.contains(QStringLiteral("id: sunsetHourSpin")));
    }
};

QTEST_MAIN(AppearanceContextTimeUiContractTest)
#include "appearance_contexttime_ui_contract_test.moc"
