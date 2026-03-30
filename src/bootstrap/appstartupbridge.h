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
class StatusNotifierHost;
class ScreencastPrivacyModel;
class InputCapturePrivacyModel;
class DesktopAppModel;
class ShortcutModel;
class DockModel;
class SpacesManager;
class AppExecutionGate;
class AppCommandRouter;
class CursorController;
class ThemeIconController;
class UIPreferences;
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
class TothespotService;
class TothespotContextMenuHelper;
class TothespotTextHighlighter;
class MetadataIndexServer;
namespace Slm::Clipboard {
class ClipboardServiceClient;
}
namespace Slm::Motion {
class MotionController;
}
class ShellStateController;

namespace AppStartupBridge {
void registerTopBarIndicatorContext(QQmlContext *context,
                                    NetworkManager *networkManager,
                                    BluetoothManager *bluetoothManager,
                                    SoundManager *soundManager,
                                    MediaSessionManager *mediaSessionManager,
                                    NotificationManager *notificationManager,
                                    BatteryManager *batteryManager,
                                    ExternalIndicatorRegistry *externalIndicatorRegistry,
                                    GlobalMenuManager *globalMenuManager,
                                    StatusNotifierHost *statusNotifierHost,
                                    ScreencastPrivacyModel *screencastPrivacyModel,
                                    InputCapturePrivacyModel *inputCapturePrivacyModel);

void registerCoreContext(QQmlContext *context,
                         DesktopAppModel *appModel,
                         ShortcutModel *shortcutModel,
                         DockModel *dockModel,
                         SpacesManager *spacesManager,
                         AppExecutionGate *appExecutionGate,
                         AppCommandRouter *appCommandRouter,
                         CursorController *cursorController,
                         ThemeIconController *themeIconController,
                         UIPreferences *uiPreferences,
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
                         TothespotService *tothespotService,
                         TothespotContextMenuHelper *tothespotContextMenuHelper,
                         TothespotTextHighlighter *tothespotTextHighlighter,
                         MetadataIndexServer *metadataIndexServer,
                         Slm::Clipboard::ClipboardServiceClient *clipboardServiceClient,
                         Slm::Motion::MotionController *motionController,
                         ShellStateController *shellStateController);

void setStartupWindowContext(QQmlContext *context,
                             bool appStartWindowed,
                             int appStartWindowWidth,
                             int appStartWindowHeight);

void wireGlobalBatchProgress(QObject *owner,
                             FileManagerApi *fileManagerApi,
                             WindowingBackendManager *windowingBackendManager,
                             GlobalProgressCenter *globalProgressCenter);
}
