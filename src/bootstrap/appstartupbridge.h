#pragma once

#include <QObject>

class QQmlContext;

class NetworkManager;
class BluetoothManager;
class SoundManager;
class MediaSessionManager;
class NotificationManager;
class BatteryManager;
class ExternalIndicatorRegistry;
class GlobalMenuManager;
class GlobalMenuAdaptiveController;
class GlobalMenuSuspendBridge;
class StatusNotifierHost;
class ScreencastPrivacyModel;
class InputCapturePrivacyModel;
class DesktopAppModel;
class ShortcutModel;
class AppDeckModel;
class SpacesManager;
class AppExecutionGate;
class AppCommandRouter;
class CursorController;
class ThemeIconController;
class WindowingBackendManager;
class WorkspaceManager;
class WorkspaceStripModel;
class MultitaskingController;
class WindowThumbnailLayoutEngine;
class WorkspacePreviewManager;
class ScreenshotManager;
class PortalChooserLogicHelper;
class ScreenshotSaveHelper;
class PortalUiBridge;
class FileManagerApi;
class FileManagerModel;
class FileManagerModelFactory;
class GlobalProgressCenter;
class PulseService;
class PulseContextMenuHelper;
class PulseTextHighlighter;
class MetadataIndexServer;
namespace Slm::Clipboard {
class ClipboardServiceClient;
}
namespace Slm::Motion {
class MotionController;
}
class ShellStateController;
class ShellInputRouter;
class ShellLayerWatchdog;
class PowerBridge;
namespace Slm::ContextMenu {
class ContextMenuService;
}

namespace AppStartupBridge {
void registerCrownIndicatorContext(QQmlContext *context,
                                    NetworkManager *networkManager,
                                    BluetoothManager *bluetoothManager,
                                    SoundManager *soundManager,
                                    MediaSessionManager *mediaSessionManager,
                                    NotificationManager *notificationManager,
                                    BatteryManager *batteryManager,
                                    ExternalIndicatorRegistry *externalIndicatorRegistry,
                                    GlobalMenuManager *globalMenuManager,
                                    GlobalMenuAdaptiveController *globalMenuAdaptiveController,
                                    GlobalMenuSuspendBridge *globalMenuSuspendBridge,
                                    StatusNotifierHost *statusNotifierHost,
                                    ScreencastPrivacyModel *screencastPrivacyModel,
                                    InputCapturePrivacyModel *inputCapturePrivacyModel);

void registerCoreContext(QQmlContext *context,
                         DesktopAppModel *appModel,
                         ShortcutModel *shortcutModel,
                         AppDeckModel *dockModel,
                         SpacesManager *spacesManager,
                         AppExecutionGate *appExecutionGate,
                         AppCommandRouter *appCommandRouter,
                         CursorController *cursorController,
                         ThemeIconController *themeIconController,
                         QObject *desktopSettings,
                         WindowingBackendManager *windowingBackendManager,
                         WorkspaceManager *workspaceManager,
                         WorkspaceStripModel *workspaceStripModel,
                         MultitaskingController *multitaskingController,
                         WindowThumbnailLayoutEngine *windowThumbnailLayoutEngine,
                         WorkspacePreviewManager *workspacePreviewManager,
                         ScreenshotManager *screenshotManager,
                         PortalChooserLogicHelper *portalChooserLogicHelper,
                         ScreenshotSaveHelper *screenshotSaveHelper,
                         PortalUiBridge *portalUiBridge,
                         FileManagerApi *fileManagerApi,
                         FileManagerModel *fileManagerModel,
                         FileManagerModelFactory *fileManagerModelFactory,
                         GlobalProgressCenter *globalProgressCenter,
                         PulseService *pulseService,
                         PulseContextMenuHelper *pulseContextMenuHelper,
                         PulseTextHighlighter *pulseTextHighlighter,
                         MetadataIndexServer *metadataIndexServer,
                         Slm::Clipboard::ClipboardServiceClient *clipboardServiceClient,
                         Slm::Motion::MotionController *motionController,
                         ShellStateController *shellStateController,
                         ShellInputRouter *shellInputRouter,
                         ShellLayerWatchdog *shellLayerWatchdog,
                         PowerBridge *powerBridge,
                         Slm::ContextMenu::ContextMenuService *contextMenuService);

void setStartupWindowContext(QQmlContext *context,
                             bool appStartWindowed,
                             int appStartWindowWidth,
                             int appStartWindowHeight);

void wireGlobalBatchProgress(QObject *owner,
                             FileManagerApi *fileManagerApi,
                             WindowingBackendManager *windowingBackendManager,
                             GlobalProgressCenter *globalProgressCenter);
}
