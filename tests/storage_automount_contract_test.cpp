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

QString functionSlice(const QString &text, const QString &signature, const QString &nextSignature)
{
    const int begin = text.indexOf(signature);
    if (begin < 0) {
        return QString();
    }
    int end = -1;
    if (!nextSignature.isEmpty()) {
        end = text.indexOf(nextSignature, begin + signature.size());
    }
    if (end < 0) {
        end = text.size();
    }
    return text.mid(begin, end - begin);
}

} // namespace

class StorageAutomountContractTest : public QObject
{
    Q_OBJECT

private slots:
    void mountRuntime_doesNotUseShellCommands()
    {
        const QString fmPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/apps/filemanager/filemanagerapi_storage.cpp");
        const QString devicesPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/daemon/devicesd/devicesmanager.cpp");

        const QString fmText = readTextFile(fmPath);
        const QString devicesText = readTextFile(devicesPath);
        QVERIFY2(!fmText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(fmPath)));
        QVERIFY2(!devicesText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(devicesPath)));

        QVERIFY(!fmText.contains(QStringLiteral("udisksctl")));
        QVERIFY(!fmText.contains(QStringLiteral("gio mount")));
        QVERIFY(!fmText.contains(QStringLiteral("QProcess")));

        const QString mountBlock = functionSlice(devicesText,
                                                 QStringLiteral("QVariantMap DevicesManager::Mount("),
                                                 QStringLiteral("QVariantMap DevicesManager::Eject("));
        const QString ejectBlock = functionSlice(devicesText,
                                                 QStringLiteral("QVariantMap DevicesManager::Eject("),
                                                 QStringLiteral("QVariantMap DevicesManager::Unlock("));
        QVERIFY2(!mountBlock.isEmpty(), "failed to locate DevicesManager::Mount");
        QVERIFY2(!ejectBlock.isEmpty(), "failed to locate DevicesManager::Eject");

        QVERIFY(!mountBlock.contains(QStringLiteral("runCommand(")));
        QVERIFY(!mountBlock.contains(QStringLiteral("udisksctl")));
        QVERIFY(!mountBlock.contains(QStringLiteral("QProcess")));
        QVERIFY(!ejectBlock.contains(QStringLiteral("runCommand(")));
        QVERIFY(!ejectBlock.contains(QStringLiteral("udisksctl")));
        QVERIFY(!ejectBlock.contains(QStringLiteral("QProcess")));
    }

    void notifier_grouping_isPerDeviceNotPerPartition()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storageattachnotifier.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("deviceGroupKey")));
        QVERIFY(text.contains(QStringLiteral("prevCount > 0 || group.visibleCount <= 0")));
        QVERIFY(text.contains(QStringLiteral("m_lastNotifiedMsByGroup")));
    }

    void notifier_openAction_singleOpenForMultiVolume()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storageattachnotifier.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("if (payload.volumes.isEmpty())")));
        QVERIFY(text.contains(QStringLiteral("for (const VolumeRow &volume : payload.volumes)")));
        QVERIFY(text.contains(QStringLiteral("if (volume.mounted)")));
        QVERIFY(text.contains(QStringLiteral("openVolume(payload.volumes.first())")));
    }

    void systemPartitions_hiddenByDefaultPolicy()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storagemanager.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("looksSystemPartitionStorage")));
        QVERIFY(text.contains(QStringLiteral("if (isSystem) {")));
        QVERIFY(text.contains(QStringLiteral("d.visible = false")));
        QVERIFY(text.contains(QStringLiteral("d.action = QStringLiteral(\"ignore\")")));
    }

    void policyFallbackChain_supportsSerialPartitionKey()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storagemanager.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("resolveSerialForDeviceStorage")));
        QVERIFY(text.contains(QStringLiteral("partitionIndexFromDeviceStorage")));
        QVERIFY(text.contains(QStringLiteral("serial-index:%1#%2")));
        QVERIFY(text.contains(QStringLiteral("copyBool(QStringLiteral(\"automount\"))")));
        QVERIFY(text.contains(QStringLiteral("patch.contains(QStringLiteral(\"action\"))")));
        QVERIFY(text.contains(QStringLiteral("action == QStringLiteral(\"ignore\")")));
        QVERIFY(text.contains(QStringLiteral("policyScopeFromPolicyKeyStorage")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"policyScope\")")));
        QVERIFY(text.contains(QStringLiteral("promoteLegacyPolicyKeyToSerialIndexStorage")));
        QVERIFY(text.contains(QStringLiteral("pruneLegacyPolicyKeysForSerialIndexStorage")));
        QVERIFY(text.contains(QStringLiteral("SLM_STORAGE_POLICY_PRUNE_LEGACY_KEYS")));
        QVERIFY(text.contains(QStringLiteral("StoragePolicyStore::savePolicyMap(policyMap")));
        QVERIFY(text.contains(QStringLiteral("policyPromotionCount")));
        QVERIFY(text.contains(QStringLiteral("policyLegacyPruneCount")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"uuid:\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"partuuid:\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"policyInput\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"policyOutput\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"is_removable\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"auto_open\")")));
    }

    void policyStore_migratesCanonicalKeysToSchemaV3()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storagepolicystore.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("kStoragePolicySchemaVersion = 3")));
        QVERIFY(text.contains(QStringLiteral("canonicalizePolicyKey")));
        QVERIFY(text.contains(QStringLiteral("migratePolicyEntriesForSchema")));
        QVERIFY(text.contains(QStringLiteral("prefix == QStringLiteral(\"serial-index\")")));
        QVERIFY(text.contains(QStringLiteral("in.contains(QStringLiteral(\"action\"))")));
        QVERIFY(text.contains(QStringLiteral("storage policy migration telemetry")));
        QVERIFY(text.contains(QStringLiteral("canonicalCollisions=")));
        QVERIFY(text.contains(QStringLiteral("saveAttempted=")));
    }

    void policyApi_exposedViaStorageServiceAndFileManagerApi()
    {
        const QString serviceHeaderPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/daemon/devicesd/storageservice.h");
        const QString serviceSourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/daemon/devicesd/storageservice.cpp");
        const QString fmHeaderPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/apps/filemanager/include/filemanagerapi.h");
        const QString fmSourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/apps/filemanager/filemanagerapi_storage.cpp");

        const QString serviceHeader = readTextFile(serviceHeaderPath);
        const QString serviceSource = readTextFile(serviceSourcePath);
        const QString fmHeader = readTextFile(fmHeaderPath);
        const QString fmSource = readTextFile(fmSourcePath);

        QVERIFY2(!serviceHeader.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(serviceHeaderPath)));
        QVERIFY2(!serviceSource.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(serviceSourcePath)));
        QVERIFY2(!fmHeader.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(fmHeaderPath)));
        QVERIFY2(!fmSource.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(fmSourcePath)));

        QVERIFY(serviceHeader.contains(QStringLiteral("StoragePolicyForPath")));
        QVERIFY(serviceHeader.contains(QStringLiteral("SetStoragePolicyForPath")));
        QVERIFY(serviceSource.contains(QStringLiteral("storage-policy-read")));
        QVERIFY(serviceSource.contains(QStringLiteral("storage-policy-write")));
        QVERIFY(fmHeader.contains(QStringLiteral("storagePolicyForPath")));
        QVERIFY(fmHeader.contains(QStringLiteral("setStoragePolicyForPath")));
        QVERIFY(fmSource.contains(QStringLiteral("SetStoragePolicyForPath")));
        QVERIFY(fmSource.contains(QStringLiteral("StoragePolicyForPath")));
        QVERIFY(fmSource.contains(QStringLiteral("callStorageServiceStorage(QStringLiteral(\"GetStorageLocations\")")));
        QVERIFY(fmSource.contains(QStringLiteral("callStorageServiceStorage(QStringLiteral(\"Mount\")")));
        QVERIFY(fmSource.contains(QStringLiteral("callStorageServiceStorage(QStringLiteral(\"Eject\")")));
        QVERIFY(fmSource.contains(QStringLiteral("callDevicesServiceStorage(QStringLiteral(\"GetStorageLocations\")")));
    }

    void propertiesDialog_mountBehaviorUiWired()
    {
        const QString contentPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerPropertiesDialogContent.qml");
        const QString opsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerOps.js");
        const QString windowPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerWindow.qml");
        const QString contentText = readTextFile(contentPath);
        const QString opsText = readTextFile(opsPath);
        const QString windowText = readTextFile(windowPath);

        QVERIFY2(!contentText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(contentPath)));
        QVERIFY2(!opsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(opsPath)));
        QVERIFY2(!windowText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(windowPath)));

        QVERIFY(contentText.contains(QStringLiteral("text: \"Mount Behavior\"")));
        QVERIFY(contentText.contains(QStringLiteral("Memuat kebijakan mount")));
        QVERIFY(contentText.contains(QStringLiteral("hostRoot.applyPropertiesStorageScope(\"device\")")));
        QVERIFY(contentText.contains(QStringLiteral("text: \"Tanya setiap kali\"")));
        QVERIFY(contentText.contains(QStringLiteral("text: \"Jangan mount otomatis\"")));
        QVERIFY(contentText.contains(QStringLiteral("\"action\": \"ignore\"")));
        QVERIFY(contentText.contains(QStringLiteral("\"action\": \"ask\"")));
        QVERIFY(contentText.contains(QStringLiteral("\"read_only\"")));
        QVERIFY(contentText.contains(QStringLiteral("Aktifkan mount otomatis")));
        QVERIFY(contentText.contains(QStringLiteral("mountPolicyErrorText")));
        QVERIFY(contentText.contains(QStringLiteral("Drive terkunci. Buka kunci lalu coba lagi.")));
        QVERIFY(contentText.contains(QStringLiteral("Drive sedang digunakan. Tutup file yang masih dipakai lalu coba lagi.")));
        QVERIFY(contentText.contains(QStringLiteral("Drive tidak dikenali.")));
        QVERIFY(contentText.contains(QStringLiteral("Akses ke drive ditolak.")));
        QVERIFY(contentText.contains(QStringLiteral("mountPolicyCanEdit")));
        QVERIFY(contentText.contains(QStringLiteral("mountPolicyAutoOpenEnabled")));
        QVERIFY(contentText.contains(QStringLiteral("mountPolicyExecEnabled")));
        QVERIFY(contentText.contains(QStringLiteral("Mode read-only aktif")));
        QVERIFY(opsText.contains(QStringLiteral("loadPropertiesStoragePolicy")));
        QVERIFY(windowText.contains(QStringLiteral("setStoragePolicyForPath")));
        QVERIFY(windowText.contains(QStringLiteral("propertiesStoragePolicyScope")));
        QVERIFY(windowText.contains(QStringLiteral("propertiesStorageAction")));
        QVERIFY(windowText.contains(QStringLiteral("\"action\"")));
    }

    void runtimeSmokeTest_existsForHardwareValidation()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/tests/storage_runtime_smoke_test.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION")));
        QVERIFY(text.contains(QStringLiteral("SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED")));
        QVERIFY(text.contains(QStringLiteral("GetStorageLocations")));
    }

    void sidebarStorageUi_nonTechnicalCopyAndLoadingState()
    {
        const QString opsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarOps.js");
        const QString panePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarPane.qml");
        const QString menuPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarContextMenu.qml");
        const QString contextOpsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarContextOps.js");
        const QString windowPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerWindow.qml");

        const QString opsText = readTextFile(opsPath);
        const QString paneText = readTextFile(panePath);
        const QString menuText = readTextFile(menuPath);
        const QString contextOpsText = readTextFile(contextOpsPath);
        const QString windowText = readTextFile(windowPath);

        QVERIFY2(!opsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(opsPath)));
        QVERIFY2(!paneText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(panePath)));
        QVERIFY2(!menuText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(menuPath)));
        QVERIFY2(!contextOpsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(contextOpsPath)));
        QVERIFY2(!windowText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(windowPath)));

        QVERIFY(opsText.contains(QStringLiteral("appendSidebarSection(sidebarModel, \"Devices\")")));
        QVERIFY(opsText.contains(QStringLiteral("\"rowType\": \"storage-group\"")));
        QVERIFY(opsText.contains(QStringLiteral("\"label\": \"Memindai drive...\"")));
        QVERIFY(menuText.contains(QStringLiteral("? \"Open Drive\"")));
        QVERIFY(menuText.contains(QStringLiteral("text: \"Eject\"")));
        QVERIFY(contextOpsText.contains(QStringLiteral("notifyResult(\"Open Drive\"")));
        QVERIFY(contextOpsText.contains(QStringLiteral("notifyResult(\"Eject\"")));
        QVERIFY(paneText.contains(QStringLiteral("rowType === \"storage-group\"")));
        QVERIFY(paneText.contains(QStringLiteral("required property int depth")));
        QVERIFY(windowText.contains(QStringLiteral("storageSidebarSettleMs")));
        QVERIFY(windowText.contains(QStringLiteral("id: storageSettleTimer")));
        QVERIFY(windowText.contains(QStringLiteral("rebuildSidebarItems(root.storagePendingSidebarRows || [])")));
    }

    void specialStateMessaging_storageErrorsAreNonTechnical()
    {
        const QString actionsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerWindowActions.js");
        const QString pathOpsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerPathOps.js");
        const QString apiConnectionsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerApiConnections.qml");
        const QString actionsText = readTextFile(actionsPath);
        const QString pathOpsText = readTextFile(pathOpsPath);
        const QString apiConnectionsText = readTextFile(apiConnectionsPath);

        QVERIFY2(!actionsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(actionsPath)));
        QVERIFY2(!pathOpsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(pathOpsPath)));
        QVERIFY2(!apiConnectionsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(apiConnectionsPath)));

        QVERIFY(actionsText.contains(QStringLiteral("function isStorageAction")));
        QVERIFY(actionsText.contains(QStringLiteral("function nonTechnicalStorageError")));
        QVERIFY(actionsText.contains(QStringLiteral("Drive terkunci")));
        QVERIFY(actionsText.contains(QStringLiteral("Drive sedang digunakan")));
        QVERIFY(actionsText.contains(QStringLiteral("Drive tidak dikenali")));
        QVERIFY(actionsText.contains(QStringLiteral("sanitizeStorageRaw")));
        QVERIFY(actionsText.contains(QStringLiteral("Layanan drive tidak tersedia.")));
        QVERIFY(actionsText.contains(QStringLiteral("Drive tidak ditemukan.")));
        QVERIFY(actionsText.contains(QStringLiteral("Operasi dibatalkan.")));
        QVERIFY(pathOpsText.contains(QStringLiteral("notifyResult(\"Open Drive\"")));
        QVERIFY(apiConnectionsText.contains(QStringLiteral("function onStorageUnmountFinished(devicePath, ok, error)")));
        QVERIFY(apiConnectionsText.contains(QStringLiteral("notifyResult(\"Eject\"")));
    }

    // Regression guard: mount → open → browse flow must remain fully wired.
    // Any break in this chain would silently prevent "Open Drive" from navigating
    // after async mount completes.
    void mountOpenBrowse_flowIsFullyWired()
    {
        const QString contextOpsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarContextOps.js");
        const QString apiConnectionsPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerApiConnections.qml");
        const QString contextMenuPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerSidebarContextMenu.qml");
        const QString windowPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerWindow.qml");

        const QString contextOpsText = readTextFile(contextOpsPath);
        const QString apiConnectionsText = readTextFile(apiConnectionsPath);
        const QString contextMenuText = readTextFile(contextMenuPath);
        const QString windowText = readTextFile(windowPath);

        QVERIFY2(!contextOpsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(contextOpsPath)));
        QVERIFY2(!apiConnectionsText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(apiConnectionsPath)));
        QVERIFY2(!contextMenuText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(contextMenuPath)));
        QVERIFY2(!windowText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(windowPath)));

        // Step 1: context ops must set pendingMountDevice BEFORE calling startMountStorageDevice
        // so the async completion handler can match the right device.
        const QString mountFn = functionSlice(contextOpsText,
                                              QStringLiteral("function sidebarContextMountDevice("),
                                              QStringLiteral("\nfunction "));
        QVERIFY2(!mountFn.isEmpty(), "sidebarContextMountDevice not found in FileManagerSidebarContextOps.js");
        const int pendingSet  = mountFn.indexOf(QStringLiteral("pendingMountDevice = dev"));
        const int mountCall   = mountFn.indexOf(QStringLiteral("startMountStorageDevice(dev)"));
        QVERIFY2(pendingSet >= 0,  "pendingMountDevice must be set before startMountStorageDevice");
        QVERIFY2(mountCall >= 0,   "startMountStorageDevice(dev) must be called");
        QVERIFY2(pendingSet < mountCall, "pendingMountDevice must be set BEFORE startMountStorageDevice call");

        // Step 1b: error path must clear pendingMountDevice and surface notifyResult
        QVERIFY(mountFn.contains(QStringLiteral("pendingMountDevice = \"\"")));
        QVERIFY(mountFn.contains(QStringLiteral("notifyResult(\"Open Drive\"")));

        // Step 2: storageMountFinished handler must navigate on success and clear pending state
        const QString mountFinishedFn = functionSlice(apiConnectionsText,
                                                      QStringLiteral("function onStorageMountFinished("),
                                                      QStringLiteral("function on"));
        QVERIFY2(!mountFinishedFn.isEmpty(), "onStorageMountFinished not found in FileManagerApiConnections.qml");
        QVERIFY(mountFinishedFn.contains(QStringLiteral("pendingMountDevice")));
        QVERIFY(mountFinishedFn.contains(QStringLiteral("resolveMountedPathForDevice(")));
        QVERIFY(mountFinishedFn.contains(QStringLiteral("openPath(")));
        // Must clear pending before navigating (prevents double-open on storageLocationsUpdated)
        const int pendingClear = mountFinishedFn.indexOf(QStringLiteral("pendingMountDevice = \"\""));
        const int openPathCall = mountFinishedFn.indexOf(QStringLiteral("openPath("));
        QVERIFY2(pendingClear >= 0, "pendingMountDevice must be cleared in onStorageMountFinished");
        QVERIFY2(openPathCall >= 0, "openPath must be called in onStorageMountFinished on success");
        QVERIFY2(pendingClear < openPathCall, "pendingMountDevice must be cleared BEFORE openPath call");
        // Error path: must also clear pending and call notifyResult
        QVERIFY(mountFinishedFn.contains(QStringLiteral("notifyResult(\"Open Drive\"")));

        // Step 3: storageLocationsUpdated fallback must handle deferred navigation
        // (covers the race where storageMountFinished fires before the sidebar row is ready)
        const QString locationsUpdatedFn = functionSlice(apiConnectionsText,
                                                         QStringLiteral("function onStorageLocationsUpdated("),
                                                         QStringLiteral("function on"));
        QVERIFY2(!locationsUpdatedFn.isEmpty(), "onStorageLocationsUpdated not found");
        QVERIFY(locationsUpdatedFn.contains(QStringLiteral("pendingMountDevice")));
        QVERIFY(locationsUpdatedFn.contains(QStringLiteral("resolveMountedPathForDevice(")));
        QVERIFY(locationsUpdatedFn.contains(QStringLiteral("openPath(")));

        // Step 4: context menu must capture a device snapshot at menu-open time so that
        // a sidebar update between right-click and "Open Drive" click cannot corrupt the target.
        QVERIFY(contextMenuText.contains(QStringLiteral("property string contextDeviceSnapshot")));
        QVERIFY(contextMenuText.contains(QStringLiteral("contextDeviceSnapshot = String(hostRoot.sidebarContextDevice")));
        QVERIFY(contextMenuText.contains(QStringLiteral("sidebarContextMountDevice(root.contextDeviceSnapshot)")));

        // Step 5: FileManagerWindow must expose sidebarContextMountDevice wrapper
        // so the context menu can call back into the ops layer.
        QVERIFY(windowText.contains(QStringLiteral("function sidebarContextMountDevice(")));
        QVERIFY(windowText.contains(QStringLiteral("FileManagerSidebarContextOps.sidebarContextMountDevice(")));
    }
};

QTEST_MAIN(StorageAutomountContractTest)
#include "storage_automount_contract_test.moc"
