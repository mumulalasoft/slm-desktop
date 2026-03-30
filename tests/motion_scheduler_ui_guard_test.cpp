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

class MotionSchedulerUiGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void keyMicroInteractionViews_areSchedulerGuarded()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QStringList files{
            base + QStringLiteral("/Qml/components/dock/DockItem.qml"),
            base + QStringLiteral("/Qml/components/dock/Dock.qml"),
            base + QStringLiteral("/Qml/components/topbar/TopBarSearchButton.qml"),
            base + QStringLiteral("/Qml/components/topbar/TopBarMainMenuControl.qml"),
            base + QStringLiteral("/Qml/components/topbar/TopBarScreenshotControl.qml"),
            base + QStringLiteral("/Qml/components/topbar/TopBarBrandSection.qml"),
            base + QStringLiteral("/Qml/components/shell/ShellShortcutTile.qml"),
            base + QStringLiteral("/Qml/components/notification/NotificationCard.qml"),
            base + QStringLiteral("/Qml/components/notification/BannerContainer.qml"),
            base + QStringLiteral("/Qml/components/portalchooser/PortalChooserListRow.qml"),
            base + QStringLiteral("/Qml/apps/settings/Sidebar.qml"),
            base + QStringLiteral("/Qml/apps/settings/components/SettingCard.qml"),
            base + QStringLiteral("/Qml/apps/settings/components/FontPickerDialog.qml"),
            base + QStringLiteral("/Qml/apps/filemanager/FileManagerContentView.qml"),
            base + QStringLiteral("/Qml/components/applet/NetworkApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/SoundApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/BluetoothApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/BatteryApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/ClipboardApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/NotificationApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/PrintJobApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/DatetimeApplet.qml"),
            base + QStringLiteral("/Qml/components/applet/ScreencastIndicator.qml"),
            base + QStringLiteral("/Qml/components/applet/InputCaptureIndicator.qml"),
            base + QStringLiteral("/Qml/components/indicators/AppIndicatorItem.qml"),
            base + QStringLiteral("/Qml/components/style/WindowControlsCapsule.qml"),
            base + QStringLiteral("/Qml/apps/settings/components/SettingToggle.qml"),
            base + QStringLiteral("/Qml/apps/filemanager/FileManagerMenus.qml"),
            base + QStringLiteral("/Qml/apps/filemanager/FileManagerHeaderBar.qml"),
            base + QStringLiteral("/Qml/apps/settings/Main.qml")
        };

        for (const QString &file : files) {
            const QString text = readTextFile(file);
            QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(file)));
            QVERIFY2(text.contains(QStringLiteral("allowMotionPriority")),
                     qPrintable(QStringLiteral("missing allowMotionPriority guard in %1").arg(file)));
        }
    }
};

QTEST_MAIN(MotionSchedulerUiGuardTest)
#include "motion_scheduler_ui_guard_test.moc"
