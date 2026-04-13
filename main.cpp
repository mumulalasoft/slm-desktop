#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QUrl>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QtMath>
#include <QFile>
#include <QLibraryInfo>
#include <QPalette>
#include <QRegion>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QWindow>
#include <QTextStream>
#include <csignal>
#if defined(__linux__)
#include <execinfo.h>
#include <unistd.h>
#endif

#include "appmodel.h"
#include "src/core/execution/appcommandrouter.h"
#include "src/bootstrap/appstartupargs.h"
#include "src/bootstrap/appstartupbridge.h"
#include "src/bootstrap/daemonservicebootstraprunner.h"
#include "src/core/execution/appexecutiongate.h"
#include "src/services/power/batterymanager.h"
#include "src/services/power/powerbridge.h"
#include "src/services/bluetooth/bluetoothmanager.h"
#include "dockmodel.h"
#include "src/services/media/mediasessionmanager.h"
#include "src/services/network/networkmanager.h"
#include "src/services/notifications/notificationmanager.h"
#include "src/services/storage/storageattachnotifier.h"
#include "shortcutmodel.h"
#include "cursorcontroller.h"
#include "src/core/workspace/spacesmanager.h"
#include "src/core/workspace/workspacemanager.h"
#include "src/core/workspace/workspacestripmodel.h"
#include "src/core/workspace/multitaskingcontroller.h"
#include "src/core/workspace/windowthumbnaillayoutengine.h"
#include "src/core/icons/themeiconprovider.h"
#include "src/core/icons/themeiconcontroller.h"
#include "resourcepaths.h"
#include "src/services/sound/soundmanager.h"
#include "src/services/indicator/statusnotifierhost.h"
#include "src/services/portal/screencastprivacymodel.h"
#include "src/services/portal/inputcaptureprivacymodel.h"
#include "externalindicatorregistry.h"
#include "globalmenumanager.h"
#include "src/core/workspace/windowingbackendmanager.h"
#include "src/core/workspace/compositorinputcapturebackendservice.h"
#include "src/core/workspace/compositorinputcaptureprimitiveservice.h"
#include "src/core/workspace/workspacepreviewmanager.h"
#include "screenshotmanager.h"
#include "portalchooserlogichelper.h"
#include "screenshotsavehelper.h"
#include "portaluibridge.h"
#include "metadataindexserver.h"
#include "tothespotservice.h"
#include "tothespotcontextmenuhelper.h"
#include "tothespottexthighlighter.h"
#include "src/services/clipboard/ClipboardServiceClient.h"
#include "src/services/session/SessionStateClient.h"
#include "src/apps/filemanager/include/filemanagerapi.h"
#include "src/apps/filemanager/include/filemanagermodel.h"
#include "src/apps/filemanager/include/filemanagermodelfactory.h"
#include "src/apps/settings/modules/network/firewallserviceclient.h"
#include "src/apps/settings/desktopsettingsclient.h"
#include "src/filemanager/ops/globalprogresscenter.h"
#include "src/filemanager/FileManagerShellBridge.h"
#include "src/filemanager/ThumbnailImageProvider.h"
#include "src/core/motion/slmmotioncontroller.h"
#include "src/core/shell/shellstatecontroller.h"
#include "src/core/shell/shellinputrouter.h"
#include "src/core/shell/shelllayerwatchdog.h"
#ifdef SLM_HAVE_WAYLANDCLIENT
#include "src/core/wayland/layershell/dockbootstrapstate.h"
#include "src/core/wayland/layershell/wlrlayershell.h"
#endif
#include "src/core/system/missingcomponentcontroller.h"
#include "src/printing/core/PrinterManager.h"
#include "src/printing/core/PrintSession.h"
#include "src/printing/core/PrintPreviewModel.h"
#include "src/printing/core/JobSubmitter.h"

#if defined(__linux__)
namespace {
void slmCrashSignalHandler(int sig)
{
    const char msg[] = "\n[crash] fatal signal received, dumping stack trace...\n";
    ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
    void *frames[128];
    const int n = ::backtrace(frames, 128);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void installCrashSignalHandlers()
{
    ::signal(SIGSEGV, slmCrashSignalHandler);
    ::signal(SIGABRT, slmCrashSignalHandler);
    ::signal(SIGBUS, slmCrashSignalHandler);
    ::signal(SIGILL, slmCrashSignalHandler);
    ::signal(SIGFPE, slmCrashSignalHandler);
}
} // namespace
#endif

int main(int argc, char *argv[])
{
#if defined(__linux__)
    installCrashSignalHandlers();
#endif
    const auto roundedRegionForRect = [](const QRect &rect, int radius) -> QRegion {
        if (rect.isEmpty() || radius <= 0) {
            return QRegion(rect);
        }
        const int rr = qMin(radius, qMin(rect.width(), rect.height()) / 2);
        QRegion region(rect.adjusted(rr, 0, -rr, 0));
        region += QRegion(rect.adjusted(0, rr, 0, -rr));
        region += QRegion(QRect(rect.topLeft(), QSize(rr * 2, rr * 2)), QRegion::Ellipse);
        region += QRegion(QRect(rect.topRight() - QPoint(rr * 2, 0), QSize(rr * 2, rr * 2)), QRegion::Ellipse);
        region += QRegion(QRect(rect.bottomLeft() - QPoint(0, rr * 2), QSize(rr * 2, rr * 2)), QRegion::Ellipse);
        region += QRegion(QRect(rect.bottomRight() - QPoint(rr * 2, rr * 2), QSize(rr * 2, rr * 2)), QRegion::Ellipse);
        return region;
    };
    const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    const bool startupLogs = qEnvironmentVariableIntValue("SLM_STARTUP_LOG") > 0;
    const bool startupTrace = qEnvironmentVariableIntValue("SLM_STARTUP_TRACE") > 0;
    const bool startupDiag = startupLogs || startupTrace;
    QString startupTracePath;
    QFile startupTraceFile;
    if (startupTrace) {
        startupTracePath = qEnvironmentVariable("SLM_STARTUP_LOG_FILE").trimmed();
        if (startupTracePath.isEmpty()) {
            startupTracePath = QDir::tempPath()
                + QStringLiteral("/slm-startup-")
                + QString::number(t0)
                + QStringLiteral(".log");
        }
        startupTraceFile.setFileName(startupTracePath);
        const bool opened = startupTraceFile.open(QIODevice::WriteOnly
                                                  | QIODevice::Truncate
                                                  | QIODevice::Text);
        if (!opened) {
            qWarning().noquote() << "[startup] unable to open trace file:" << startupTracePath;
        }
    }
    const auto startupMark = [&](const QString &phase, const QString &detail = QString()) {
        if (!startupTrace) {
            return;
        }
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - t0;
        QString line = QStringLiteral("[startup-trace] t=%1ms phase=%2")
                           .arg(elapsed)
                           .arg(phase.trimmed());
        const QString trimmedDetail = detail.trimmed();
        if (!trimmedDetail.isEmpty()) {
            line += QStringLiteral(" detail=%1").arg(trimmedDetail);
        }
        qInfo().noquote() << line;
        if (startupTraceFile.isOpen()) {
            QTextStream stream(&startupTraceFile);
            stream << line << '\n';
            stream.flush();
        }
    };
    const auto windowSummary = [](QWindow *window) -> QString {
        if (!window) {
            return QStringLiteral("null-window");
        }
        const QString title = window->title().trimmed().isEmpty()
                ? QStringLiteral("<untitled>")
                : window->title().trimmed();
        return QStringLiteral("%1 visible=%2 exposed=%3 size=%4x%5 type=%6")
            .arg(title,
                 window->isVisible() ? QStringLiteral("1") : QStringLiteral("0"),
                 window->isExposed() ? QStringLiteral("1") : QStringLiteral("0"))
            .arg(window->width())
            .arg(window->height())
            .arg(static_cast<int>(window->type()));
    };
    const auto topWindowsSummary = [&]() -> QString {
        const auto windows = QGuiApplication::topLevelWindows();
        if (windows.isEmpty()) {
            return QStringLiteral("none");
        }
        QStringList rows;
        rows.reserve(windows.size());
        for (QWindow *window : windows) {
            rows << windowSummary(window);
        }
        return rows.join(QStringLiteral(" | "));
    };
    {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        if (fmt.alphaBufferSize() < 8) {
            fmt.setAlphaBufferSize(8);
        }
        QSurfaceFormat::setDefaultFormat(fmt);
    }
    QQuickWindow::setDefaultAlphaBuffer(true);
    // Force project-level Controls style so all unqualified QtQuick.Controls
    // widgets follow slm-style consistently across shell surfaces.
    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    const QString appDir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
    qputenv("QT_QUICK_CONTROLS_STYLE", "SlmStyle");

    QGuiApplication app(argc, argv);
    if (startupDiag) {
        qInfo().noquote() << "[startup] QGuiApplication ready"
                          << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms";
    }
    startupMark(QStringLiteral("app.ready"));
    QObject::connect(&app, &QGuiApplication::focusWindowChanged, &app,
                     [&](QWindow *window) {
        startupMark(QStringLiteral("window.focusChanged"), windowSummary(window));
    });
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &app,
                     [&](Qt::ApplicationState state) {
        startupMark(QStringLiteral("app.stateChanged"),
                    QStringLiteral("state=%1").arg(static_cast<int>(state)));
    });
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Desktop"));
    const auto serviceRegistered = [](const QString &service) -> bool {
        QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
        return iface ? iface->isServiceRegistered(service).value() : false;
    };
    const auto startDaemonBinary = [](const QString &localBinary, const QString &fallbackBinary) -> bool {
        const QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
        const QString localPath = QDir(appDir).filePath(localBinary);
        bool started = false;
        if (QFileInfo::exists(localPath)) {
            started = QProcess::startDetached(localPath, {});
        }
        if (!started) {
            started = QProcess::startDetached(fallbackBinary, {});
        }
        return started;
    };
    const auto installServiceBootstrap = [&](const QString &service,
                                             const QString &localBinary,
                                             const QString &fallbackBinary,
                                             const QString &logName) {
        auto *runner = new Slm::Bootstrap::DaemonServiceBootstrapRunner(service,
                                                                        localBinary,
                                                                        fallbackBinary,
                                                                        logName,
                                                                        serviceRegistered,
                                                                        startDaemonBinary,
                                                                        &app);
        QObject::connect(runner, &Slm::Bootstrap::DaemonServiceBootstrapRunner::serviceReady,
                         &app, [logName, runner, startupDiag, startupMark](const QString &, int attempt) {
            if (startupDiag && attempt > 1) {
                qInfo().noquote() << "[startup] service ready:" << logName
                                  << "attempt=" << attempt
                                  << "spawns=" << runner->spawnAttemptCount()
                                  << "started=" << runner->spawnStartedCount()
                                  << "lastSpawnAttempt=" << runner->lastSpawnAttempt();
            }
            startupMark(QStringLiteral("service.ready"),
                        QStringLiteral("%1 attempt=%2").arg(logName).arg(attempt));
        });
        QObject::connect(runner, &Slm::Bootstrap::DaemonServiceBootstrapRunner::spawnAttempted,
                         &app, [logName, runner, startupDiag, startupMark](const QString &, int attempt, bool started) {
            if (startupDiag && started) {
                qInfo().noquote() << "[startup] started" << logName
                                  << "attempt=" << attempt
                                  << "spawnCount=" << runner->spawnAttemptCount()
                                  << "startedCount=" << runner->spawnStartedCount();
            }
            Q_UNUSED(runner)
            if (started) {
                startupMark(QStringLiteral("service.spawn"),
                            QStringLiteral("%1 attempt=%2").arg(logName).arg(attempt));
            }
        });
        QObject::connect(runner, &Slm::Bootstrap::DaemonServiceBootstrapRunner::gaveUp,
                         &app, [logName, runner, startupMark](const QString &, int attempt) {
            qWarning().noquote() << "[startup] service not ready after retries:" << logName;
            qWarning().noquote() << "[startup] bootstrap stats:" << logName
                                 << "attempt=" << attempt
                                 << "spawns=" << runner->spawnAttemptCount()
                                 << "started=" << runner->spawnStartedCount()
                                 << "lastSpawnAttempt=" << runner->lastSpawnAttempt();
            startupMark(QStringLiteral("service.gaveUp"),
                        QStringLiteral("%1 attempt=%2").arg(logName).arg(attempt));
        });
        runner->start();
    };
    installServiceBootstrap(QStringLiteral("org.freedesktop.SLMCapabilities"),
                            QStringLiteral("desktopd"),
                            QStringLiteral("desktopd"),
                            QStringLiteral("desktopd/SLMCapabilities"));
    installServiceBootstrap(QStringLiteral("org.desktop.Clipboard"),
                            QStringLiteral("slm-clipboardd"),
                            QStringLiteral("slm-clipboardd"),
                            QStringLiteral("slm-clipboardd"));
    installServiceBootstrap(QStringLiteral("org.slm.Desktop.Settings"),
                            QStringLiteral("slm-settingsd"),
                            QStringLiteral("slm-settingsd"),
                            QStringLiteral("slm-settingsd"));
    startupMark(QStringLiteral("service.bootstrap.installed"));
    const AppStartupArgs startupArgs = parseAppStartupArgs(app.arguments());
    startupMark(QStringLiteral("args.parsed"),
                QStringLiteral("windowed=%1 width=%2 height=%3")
                    .arg(startupArgs.startWindowed ? QStringLiteral("true") : QStringLiteral("false"))
                    .arg(startupArgs.windowWidth)
                    .arg(startupArgs.windowHeight));
    const bool slmActionTreeDebug =
        qEnvironmentVariableIntValue("SLM_ACTION_TREE_DEBUG") > 0;

    QQmlApplicationEngine engine;
    startupMark(QStringLiteral("engine.created"));
    const QString qtQmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    if (!qtQmlImportsPath.isEmpty()) {
        engine.addImportPath(qtQmlImportsPath);
    }
    // Ensure qrc QML modules (including SlmStyle) are always resolvable.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.addImageProvider(QStringLiteral("themeicon"), new ThemeIconProvider);
    ThemeIconController themeIconController;
    ExternalIndicatorRegistry externalIndicatorRegistry;
    StatusNotifierHost statusNotifierHost;
    ScreencastPrivacyModel screencastPrivacyModel;
    InputCapturePrivacyModel inputCapturePrivacyModel;
    GlobalMenuManager globalMenuManager;
    BatteryManager batteryManager;
    BluetoothManager bluetoothManager;
    SoundManager soundManager;
    MediaSessionManager mediaSessionManager;
    NotificationManager notificationManager;
    StorageAttachNotifier storageAttachNotifier(&notificationManager);
    NetworkManager networkManager;
    DesktopAppModel appModel;
    ShortcutModel shortcutModel;
    DockModel dockModel;
    SpacesManager spacesManager;
    CursorController cursorController;
    DesktopSettingsClient desktopSettings;
    FirewallServiceClient firewallServiceClient;
    ScreenshotManager screenshotManager;
    PortalChooserLogicHelper portalChooserLogicHelper;
    ScreenshotSaveHelper screenshotSaveHelper;
    PortalUiBridge portalUiBridge;
    FileManagerApi fileManagerApi;
    FileManagerShellBridge fileManagerShellBridge(&fileManagerApi);
    // ThumbnailImageProvider: QML Image { source: "image://thumbnail/256/" + filePath }
    engine.addImageProvider(QStringLiteral("thumbnail"),
                            new ThumbnailImageProvider(&fileManagerApi));
    MetadataIndexServer metadataIndexServer(&fileManagerApi);
    FileManagerModel fileManagerModel(&fileManagerApi, &metadataIndexServer);
    FileManagerModelFactory fileManagerModelFactory(&fileManagerApi, &metadataIndexServer);
    GlobalProgressCenter globalProgressCenter;
    TothespotService tothespotService;
    TothespotContextMenuHelper tothespotContextMenuHelper;
    TothespotTextHighlighter tothespotTextHighlighter;
    Slm::Clipboard::ClipboardServiceClient clipboardServiceClient;
    Slm::Session::SessionStateClient sessionStateClient;
    Slm::Motion::MotionController motionController;
    ShellStateController shellStateController;
    ShellInputRouter shellInputRouter(&shellStateController);
    ShellLayerWatchdog shellLayerWatchdog(&shellStateController);
    PowerBridge powerBridge;
    Slm::System::MissingComponentController missingComponentController;
#ifdef SLM_HAVE_WAYLANDCLIENT
    DockBootstrapState dockBootstrapState;
    WlrLayerShell wlrLayerShell;
    wlrLayerShell.setDockBootstrapState(&dockBootstrapState);
    QObject::connect(&wlrLayerShell, &WlrLayerShell::activeChanged, &app, [&]() {
        dockBootstrapState.setIntegrationEnabled(wlrLayerShell.isActive());
    });
    dockBootstrapState.setIntegrationEnabled(wlrLayerShell.isActive());
#endif
    Slm::Print::PrinterManager printerManager;
    Slm::Print::PrintSession printSession;
    Slm::Print::PrintPreviewModel printPreviewModel;
    Slm::Print::JobSubmitter printJobSubmitter;
    WindowingBackendManager windowingBackendManager;
    CompositorInputCaptureBackendService compositorInputCaptureBackendService(&windowingBackendManager);
    CompositorInputCapturePrimitiveService compositorInputCapturePrimitiveService(&windowingBackendManager);
    WorkspacePreviewManager workspacePreviewManager;
    WorkspaceManager workspaceManager(&windowingBackendManager,
                                      &spacesManager,
                                      windowingBackendManager.compositorStateObject());
    WorkspaceStripModel workspaceStripModel(&spacesManager, &workspaceManager);
    MultitaskingController multitaskingController(&workspaceManager, &spacesManager);
    WindowThumbnailLayoutEngine windowThumbnailLayoutEngine;
    const auto applyIconThemePref = [&]() {
        const QString light = desktopSettings.gtkIconThemeLight().trimmed();
        const QString dark = desktopSettings.gtkIconThemeDark().trimmed();
        if (!light.isEmpty() && !dark.isEmpty()) {
            themeIconController.setThemeMapping(light, dark);
        } else {
            themeIconController.useAutoDetectedMapping();
        }
        const QString mode = desktopSettings.themeMode().trimmed().toLower();
        const bool darkMode = (mode == QStringLiteral("dark"))
                || (mode != QStringLiteral("light")
                    && app.palette().color(QPalette::Window).lightness() < 128);
        themeIconController.applyForDarkMode(darkMode);
    };
    applyIconThemePref();

    const QString sessionMode = qEnvironmentVariable("SLM_SESSION_MODE").trimmed().toLower();
    const bool safeModeActive = (sessionMode == QStringLiteral("safe")
                                 || sessionMode == QStringLiteral("recovery"));
    const bool userAnimationEnabled = desktopSettings.windowingAnimationEnabled();
    const bool runtimeAnimationsEnabled = userAnimationEnabled && !safeModeActive;
    const auto applyAnimationMode = [&]() {
        const QString amode = desktopSettings.settingValue(QStringLiteral("animation.mode"),
                                                           QStringLiteral("full")).toString().trimmed().toLower();
        const bool needsReduced = !runtimeAnimationsEnabled
                                  || amode == QLatin1String("reduced")
                                  || amode == QLatin1String("minimal");
        motionController.setReducedMotion(needsReduced);
    };
    applyAnimationMode();
    QObject::connect(&desktopSettings, &DesktopSettingsClient::windowingAnimationEnabledChanged, &app, [&]() {
        applyAnimationMode();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::settingChanged, &app, [&](const QString &path) {
        if (path == QLatin1String("animation.mode")) {
            applyAnimationMode();
        }
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::themeModeChanged, &app, [&]() {
        applyAnimationMode();
        applyIconThemePref();
    });

    QObject::connect(&desktopSettings, &DesktopSettingsClient::gtkIconThemeLightChanged, &app, [&]() {
        applyIconThemePref();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::gtkIconThemeDarkChanged, &app, [&]() {
        applyIconThemePref();
    });
    AppExecutionGate appExecutionGate(&dockModel, &shortcutModel, &desktopSettings);
    appModel.setExecutionGate(&appExecutionGate);
    appModel.setDesktopSettings(&desktopSettings);
    AppCommandRouter appCommandRouter(&appExecutionGate,
                                      &screenshotManager,
                                      &desktopSettings);
    const QString envBackend = qEnvironmentVariable("DS_WINDOWING_BACKEND").trimmed();
    const QString prefBackend = desktopSettings.settingValue(QStringLiteral("windowing.backend"),
                                                             QStringLiteral("kwin-wayland")).toString();
    const QString requestedBackend = envBackend.isEmpty() ? prefBackend : envBackend;
    windowingBackendManager.configureBackend(requestedBackend);
    printerManager.reload();
    const auto syncPrintSessionWithSelection = [&]() {
        QString selectedPrinter = printSession.settingsModel()->printerId();
        if (selectedPrinter.trimmed().isEmpty()) {
            selectedPrinter = printerManager.defaultPrinterId();
            if (!selectedPrinter.trimmed().isEmpty()) {
                printSession.settingsModel()->setPrinterId(selectedPrinter);
            }
        }
        if (!selectedPrinter.trimmed().isEmpty()) {
            printSession.setPrinterCapability(printerManager.capabilities(selectedPrinter));
        }
    };
    syncPrintSessionWithSelection();
    QObject::connect(&printerManager, &Slm::Print::PrinterManager::printersChanged,
                     &app, [&]() { syncPrintSessionWithSelection(); });
    QObject::connect(printSession.settingsModel(), &Slm::Print::PrintSettingsModel::settingsChanged,
                     &app, [&]() {
                         const QString selected = printSession.settingsModel()->printerId();
                         if (!selected.trimmed().isEmpty()) {
                             printSession.setPrinterCapability(printerManager.capabilities(selected));
                         }
                     });
    if (startupLogs) {
        qInfo().noquote() << "[startup] managers constructed"
                          << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms";
    }
    startupMark(QStringLiteral("managers.constructed"));
    AppStartupBridge::registerTopBarIndicatorContext(engine.rootContext(),
                                                     &networkManager,
                                                     &bluetoothManager,
                                                     &soundManager,
                                                     &mediaSessionManager,
                                                     &notificationManager,
                                                     &batteryManager,
                                                     &externalIndicatorRegistry,
                                                     &globalMenuManager,
                                                     &statusNotifierHost,
                                                     &screencastPrivacyModel,
                                                     &inputCapturePrivacyModel);
    AppStartupBridge::registerCoreContext(engine.rootContext(),
                                          &appModel,
                                          &shortcutModel,
                                          &dockModel,
                                          &spacesManager,
                                          &appExecutionGate,
                                          &appCommandRouter,
                                          &cursorController,
                                          &themeIconController,
                                          &desktopSettings,
                                          &windowingBackendManager,
                                          &workspaceManager,
                                          &workspaceStripModel,
                                          &multitaskingController,
                                          &windowThumbnailLayoutEngine,
                                          &workspacePreviewManager,
                                          &screenshotManager,
                                          &portalChooserLogicHelper,
                                          &screenshotSaveHelper,
                                          &portalUiBridge,
                                          &fileManagerApi,
                                          &fileManagerModel,
                                          &fileManagerModelFactory,
                                          &globalProgressCenter,
                                          &tothespotService,
                                          &tothespotContextMenuHelper,
                                          &tothespotTextHighlighter,
                                          &metadataIndexServer,
                                          &clipboardServiceClient,
                                          &motionController,
                                          &shellStateController,
                                          &shellInputRouter,
                                          &shellLayerWatchdog,
                                          &powerBridge);
    AppStartupBridge::setStartupWindowContext(engine.rootContext(),
                                              startupArgs.startWindowed,
                                              startupArgs.windowWidth,
                                              startupArgs.windowHeight);
    engine.rootContext()->setContextProperty(QStringLiteral("FileManagerShellBridge"),
                                             &fileManagerShellBridge);
    // Recent files tersedia via FileManagerApi.recentFiles(limit) dari QML.
    // Tidak perlu model terpisah — QML memanggil langsung saat sidebar dibuka.
    engine.rootContext()->setContextProperty(QStringLiteral("slmActionTreeDebug"),
                                             slmActionTreeDebug);
#ifdef SLM_HAVE_WAYLANDCLIENT
    engine.rootContext()->setContextProperty(QStringLiteral("WlrLayerShell"), &wlrLayerShell);
    engine.rootContext()->setContextProperty(QStringLiteral("DockBootstrapState"), &dockBootstrapState);
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("SessionStateClient"), &sessionStateClient);
    engine.rootContext()->setContextProperty(QStringLiteral("FirewallServiceClient"), &firewallServiceClient);
    engine.rootContext()->setContextProperty(QStringLiteral("MissingComponents"), &missingComponentController);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintManager"), &printerManager);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintSession"), &printSession);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintPreviewModel"), &printPreviewModel);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintJobSubmitter"), &printJobSubmitter);
    engine.rootContext()->setContextProperty(QStringLiteral("AppBinaryDir"),
                                             QCoreApplication::applicationDirPath());
    engine.rootContext()->setContextProperty(QStringLiteral("SessionStartupMode"), sessionMode);
    engine.rootContext()->setContextProperty(QStringLiteral("SafeModeActive"), safeModeActive);
    engine.rootContext()->setContextProperty(QStringLiteral("AnimationsEnabled"), runtimeAnimationsEnabled);
    engine.rootContext()->setContextProperty(QStringLiteral("StartupTraceEnabled"), startupTrace);
    AppStartupBridge::wireGlobalBatchProgress(&app,
                                              &fileManagerApi,
                                              &windowingBackendManager,
                                              &globalProgressCenter);
    engine.addImportPath(QDir::currentPath());
    engine.addImportPath(appDir);
    if (startupLogs) {
        qInfo().noquote() << "[startup] qml context configured"
                          << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms";
    }
    startupMark(QStringLiteral("qml.context.configured"));
    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreated,
                     &app,
                     [&](QObject *obj, const QUrl &url) {
        const QString detail = QStringLiteral("url=%1 created=%2 rootCount=%3")
                                   .arg(url.toString(),
                                        obj ? QStringLiteral("true") : QStringLiteral("false"))
                                   .arg(engine.rootObjects().size());
        startupMark(QStringLiteral("qml.objectCreated"), detail);
    });
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    // Prefer direct qrc loading for startup reliability. Support both resource
    // prefixes used by different Qt/CMake generator configurations.
    const QString mainQmlQtPrefix = ResourcePaths::Qml::mainQtPrefix();
    const QString mainQmlModulePrefix = ResourcePaths::Qml::mainModulePrefix();
    startupMark(QStringLiteral("qml.load.begin"));
    if (QFile::exists(mainQmlQtPrefix)) {
        engine.load(QUrl(ResourcePaths::Qml::mainQtPrefixUrl()));
    } else if (QFile::exists(mainQmlModulePrefix)) {
        engine.load(QUrl(ResourcePaths::Qml::mainModulePrefixUrl()));
    } else {
        qCritical().noquote() << "[startup] Main.qml resource not found in qrc prefixes:"
                              << mainQmlQtPrefix << "or" << mainQmlModulePrefix;
    }
    if (startupLogs) {
        qInfo().noquote() << "[startup] qml loaded"
                          << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms"
                          << "rootObjects=" << engine.rootObjects().size();
    }
    startupMark(QStringLiteral("qml.load.end"),
                QStringLiteral("rootCount=%1").arg(engine.rootObjects().size()));
    startupMark(QStringLiteral("window.snapshot.afterLoad"), topWindowsSummary());

    // Wire the motion scheduler to the display vsync via QQuickWindow::afterAnimating.
    // This replaces the internal 16ms QTimer with a vsync-synchronised tick so that
    // gesture-driven spring physics stays aligned with the render frame budget.
    if (!engine.rootObjects().isEmpty()) {
        auto *shellWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (shellWindow) {
            motionController.enableVsyncDriving();
            QObject::connect(shellWindow, &QQuickWindow::afterAnimating,
                             &motionController, &Slm::Motion::MotionController::windowFrame);
            QObject::connect(shellWindow, &QQuickWindow::frameSwapped,
                             &app, [&, shellWindow]() {
                static bool firstFrameLogged = false;
                if (firstFrameLogged) {
                    return;
                }
                firstFrameLogged = true;
                startupMark(QStringLiteral("window.firstFrameSwapped"),
                            QStringLiteral("visible=%1 size=%2x%3")
                                .arg(shellWindow->isVisible() ? QStringLiteral("true")
                                                              : QStringLiteral("false"))
                                .arg(shellWindow->width())
                                .arg(shellWindow->height()));
            });
        }
    }

    QTimer detachedWindowMaskTimer;
    detachedWindowMaskTimer.setInterval(250);
    QObject::connect(&detachedWindowMaskTimer, &QTimer::timeout, &app, [&]() {
        const auto windows = QGuiApplication::topLevelWindows();
        for (QWindow *w : windows) {
            if (!w || w->title() != QStringLiteral("Desktop File Manager")) {
                continue;
            }
            if ((w->flags() & Qt::FramelessWindowHint) == 0) {
                continue;
            }
            const QRect rect(0, 0, w->width(), w->height());
            if (rect.isEmpty()) {
                continue;
            }
            w->setMask(roundedRegionForRect(rect, 18));
        }
    });
    detachedWindowMaskTimer.start();

    if (startupLogs) {
        QTimer::singleShot(1800, &app, [&engine, t0]() {
            qInfo().noquote() << "[startup] heartbeat"
                              << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms"
                              << "rootObjects=" << engine.rootObjects().size();
        });
        qInfo().noquote() << "[startup] event loop entering"
                          << (QDateTime::currentMSecsSinceEpoch() - t0) << "ms";
    }
    startupMark(QStringLiteral("eventloop.enter"));
    if (startupTrace) {
        auto *startupPulseTimer = new QTimer(&app);
        startupPulseTimer->setInterval(1000);
        QString lastWindowSnapshot;
        QObject::connect(startupPulseTimer, &QTimer::timeout, &app, [&, startupPulseTimer]() {
            static int pulse = 0;
            ++pulse;
            QString detail = QStringLiteral("n=%1 rootCount=%2 windows=%3")
                            .arg(pulse)
                            .arg(engine.rootObjects().size())
                            .arg(QGuiApplication::topLevelWindows().size());
            const QString snapshot = topWindowsSummary();
            if (snapshot != lastWindowSnapshot || pulse <= 5) {
                detail += QStringLiteral(" snapshot=%1").arg(snapshot);
                lastWindowSnapshot = snapshot;
            }
            startupMark(QStringLiteral("runtime.pulse"), detail);
            if (pulse >= 20) {
                startupPulseTimer->stop();
                startupMark(QStringLiteral("runtime.pulse.stop"));
            }
        });
        startupPulseTimer->start();
        startupMark(QStringLiteral("runtime.pulse.start"),
                    QStringLiteral("path=%1").arg(startupTracePath));
    }

    return app.exec();
}
