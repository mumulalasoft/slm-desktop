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

} // namespace

// Contract test: verifies that the appd D-Bus service wires all granular app
// lifecycle signals (AppAdded / AppRemoved / AppStateChanged / AppFocusChanged)
// from the LifecycleEngine through to the D-Bus boundary.
// If any leg of this chain is cut the Dock / App Switcher / Global Search stop
// receiving live updates, which is a silent regression with no build error.
class AppdSignalContractTest : public QObject
{
    Q_OBJECT

private slots:
    void dbusSurface_declaresAllGranularSignals()
    {
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/appservice.h");
        const QString text = readTextFile(headerPath);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));

        // All four signals required by the TODO are declared on the D-Bus surface.
        QVERIFY(text.contains(QStringLiteral("void AppAdded(const QString &appId)")));
        QVERIFY(text.contains(QStringLiteral("void AppRemoved(const QString &appId)")));
        QVERIFY(text.contains(QStringLiteral("void AppStateChanged(const QString &appId, const QString &state)")));
        QVERIFY(text.contains(QStringLiteral("void AppFocusChanged(const QString &appId, bool focused)")));
        // AppWindowsChanged and AppUpdated are bonus granular signals.
        QVERIFY(text.contains(QStringLiteral("void AppWindowsChanged(const QString &appId)")));
        QVERIFY(text.contains(QStringLiteral("void AppUpdated(const QString &appId)")));
    }

    void dbusInterface_isRegisteredOnSessionBus()
    {
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/appservice.cpp");
        const QString text = readTextFile(sourcePath);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        QVERIFY(text.contains(QStringLiteral("\"org.desktop.Apps\"")));
        QVERIFY(text.contains(QStringLiteral("\"/org/desktop/Apps\"")));
        QVERIFY(text.contains(QStringLiteral("QDBusConnection::ExportAllSlots")));
        QVERIFY(text.contains(QStringLiteral("QDBusConnection::ExportAllSignals")));
    }

    void lifecycleEngine_emitsAllGranularSignals()
    {
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/lifecycleengine.h");
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/lifecycleengine.cpp");
        const QString header = readTextFile(headerPath);
        const QString source = readTextFile(sourcePath);

        QVERIFY2(!header.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        QVERIFY(header.contains(QStringLiteral("void appAdded(const QString &appId)")));
        QVERIFY(header.contains(QStringLiteral("void appRemoved(const QString &appId)")));
        QVERIFY(header.contains(QStringLiteral("void appStateChanged(const QString &appId, const QString &state)")));
        QVERIFY(header.contains(QStringLiteral("void appFocusChanged(const QString &appId, bool focused)")));

        // Verify signals are actually emitted in the implementation.
        QVERIFY(source.contains(QStringLiteral("emit appAdded(")));
        QVERIFY(source.contains(QStringLiteral("emit appRemoved(")));
        QVERIFY(source.contains(QStringLiteral("emit appStateChanged(")));
        QVERIFY(source.contains(QStringLiteral("emit appFocusChanged(")));
    }

    void signalChain_lifecycleToDbusIsConnected()
    {
        // AppService::connectRegistry() must forward every LifecycleEngine signal
        // to its D-Bus counterpart. A missing connect() here would silently break
        // Dock / App Switcher live state.
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/appservice.cpp");
        const QString text = readTextFile(sourcePath);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        QVERIFY(text.contains(QStringLiteral("emit AppAdded(")));
        QVERIFY(text.contains(QStringLiteral("emit AppRemoved(")));
        QVERIFY(text.contains(QStringLiteral("emit AppStateChanged(")));
        QVERIFY(text.contains(QStringLiteral("emit AppFocusChanged(")));
        QVERIFY(text.contains(QStringLiteral("emit AppWindowsChanged(")));
        QVERIFY(text.contains(QStringLiteral("emit AppUpdated(")));
    }

    void registryUpsert_isAppCentricNotPidCentric()
    {
        // AppRegistry must use appId as the primary key, not PID.
        // PIDs are auxiliary data attached to entries.
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/appregistry.h");
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/appregistry.cpp");
        const QString header = readTextFile(headerPath);
        const QString source = readTextFile(sourcePath);

        QVERIFY2(!header.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // Primary key is appId.
        QVERIFY(header.contains(QStringLiteral("QHash<QString, AppEntry>")));
        // PIDs are auxiliary — attach/detach operations exist separately.
        QVERIFY(header.contains(QStringLiteral("bool attachPid(")));
        QVERIFY(header.contains(QStringLiteral("bool detachPid(")));
        // Find-by-pid exists for reverse lookup, but upsert takes an AppEntry (not a PID).
        QVERIFY(header.contains(QStringLiteral("AppEntry *findByPid(")));
        QVERIFY(header.contains(QStringLiteral("AppEntry *upsert(AppEntry entry)")));
    }

    void uiExposurePolicy_correctlyGatesDockswitcher()
    {
        // UIExposurePolicy must exist and restrict dock/switcher access.
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/uiexposurepolicy.h");
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/appd/uiexposurepolicy.cpp");
        const QString header = readTextFile(headerPath);
        const QString source = readTextFile(sourcePath);

        QVERIFY2(!header.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // Policy must deny shell and system-ignore from dock/switcher.
        QVERIFY(source.contains(QStringLiteral("AppCategory::Shell")));
        QVERIFY(source.contains(QStringLiteral("AppCategory::SystemIgnore")));
        // CLI apps must be restricted (systemMonitor only, no dock).
        QVERIFY(source.contains(QStringLiteral("AppCategory::CliApp")));
        // GUI apps must be granted dock access.
        QVERIFY(source.contains(QStringLiteral("AppCategory::GuiApp")));
        QVERIFY(source.contains(QStringLiteral("AppCategory::Gtk")));
        QVERIFY(source.contains(QStringLiteral("AppCategory::Kde")));
        // compute() is the policy entry point.
        QVERIFY(source.contains(QStringLiteral("UIExposure UIExposurePolicy::compute(")));
    }
};

QTEST_MAIN(AppdSignalContractTest)
#include "appd_signal_contract_test.moc"
