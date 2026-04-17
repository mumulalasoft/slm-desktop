import QtQuick 2.15

Item {
    id: root

    required property var hostRoot
    property var fileManagerApi: null
    property var connectServerDialogRef: null

    Connections {
        target: root.fileManagerApi
        ignoreUnknownSignals: true

        function onStorageLocationsUpdated(rows) {
            if (root.hostRoot.handleStorageLocationsUpdated) {
                root.hostRoot.handleStorageLocationsUpdated(rows || [])
            } else {
                root.hostRoot.rebuildSidebarItems()
            }
            if (String(root.hostRoot.pendingMountDevice || "").length > 0
                    && root.hostRoot.resolveMountedPathForDevice) {
                var deferredPath = root.hostRoot.resolveMountedPathForDevice(
                            String(root.hostRoot.pendingMountDevice || ""), "")
                if (String(deferredPath || "").length > 0
                        && String(deferredPath || "").indexOf("/dev/") !== 0) {
                    root.hostRoot.pendingMountDevice = ""
                    root.hostRoot.openPath(String(deferredPath || ""))
                }
            }
        }

        function onStorageMountFinished(devicePath, ok, mountedPath, error) {
            root.hostRoot.rebuildSidebarItems()
            var dev = String(devicePath || "")
            if (root.hostRoot.pendingMountDevice.length > 0
                    && dev === root.hostRoot.pendingMountDevice) {
                if (!!ok) {
                    var resolvedPath = root.hostRoot.resolveMountedPathForDevice(
                                dev, String(mountedPath || ""))
                    if (String(resolvedPath || "").length > 0
                            && String(resolvedPath || "").indexOf("/dev/") !== 0) {
                        root.hostRoot.pendingMountDevice = ""
                        root.hostRoot.openPath(String(resolvedPath || ""))
                    }
                } else if (!ok) {
                    root.hostRoot.pendingMountDevice = ""
                    root.hostRoot.notifyResult("Open Drive", {
                                                   "ok": false,
                                                   "error": String(
                                                                error
                                                                || "mount-failed")
                                               })
                }
            }
        }

        function onStorageUnmountFinished(devicePath, ok, error) {
            root.hostRoot.rebuildSidebarItems()
            if (!ok) {
                root.hostRoot.notifyResult("Eject", {
                                               "ok": false,
                                               "error": String(error || "eject-failed"),
                                               "devicePath": String(devicePath || "")
                                           })
            }
        }

        function onConnectServerFinished(serverUri, ok, mountedPath, error) {
            var uri = String(serverUri || "")
            if (root.hostRoot.pendingConnectServerUri.length <= 0
                    || uri !== root.hostRoot.pendingConnectServerUri) {
                return
            }
            root.hostRoot.pendingConnectServerUri = ""
            root.hostRoot.connectServerBusy = false
            if (!!ok) {
                if (root.connectServerDialogRef
                        && root.connectServerDialogRef.close) {
                    root.connectServerDialogRef.close()
                }
                if (String(mountedPath || "").length > 0) {
                    root.hostRoot.openPath(String(mountedPath || ""))
                }
            } else {
                root.hostRoot.connectServerError = String(
                            error || "connect-failed")
                root.hostRoot.notifyResult("Connect Server", {
                                               "ok": false,
                                               "error": String(
                                                            error
                                                            || "connect-failed")
                                           })
            }
        }

        function onSlmContextActionsReady(requestId, actions, error) {
            var rid = String(requestId || "")
            var expected = String(root.hostRoot.contextSlmActionsRequestId || "")
            if (expected.length > 0 && rid !== expected) {
                return
            }
            if (String(error || "").length > 0) {
                root.hostRoot.contextSlmActions = []
                return
            }
            root.hostRoot.contextSlmActions = actions || []
        }

        function onSlmContextActionInvoked(requestId, actionId, result) {
            var rid = String(requestId || "")
            var pending = root.hostRoot.pendingSlmActionRequests || ({})
            var meta = pending[rid]
            if (!meta) {
                return
            }
            var next = {}
            var keys = Object.keys(pending)
            for (var i = 0; i < keys.length; ++i) {
                if (keys[i] !== rid) {
                    next[keys[i]] = pending[keys[i]]
                }
            }
            root.hostRoot.pendingSlmActionRequests = next
            var title = String((meta && meta.title) ? meta.title : "Action")
            root.hostRoot.notifyResult(title, result || {
                                           "ok": false,
                                           "error": "invoke-failed"
                                       })
            if (result && result.ok && root.hostRoot.fileModel
                    && root.hostRoot.fileModel.refresh) {
                root.hostRoot.fileModel.refresh()
            }
        }

        function onSlmCapabilityActionsReady(requestId, capability, actions, error) {
            if (String(capability || "") !== "Share") {
                return
            }
            var rid = String(requestId || "")
            var expected = String(root.hostRoot.contextSlmShareActionsRequestId || "")
            if (expected.length > 0 && rid !== expected) {
                return
            }
            if (String(error || "").length > 0) {
                root.hostRoot.contextSlmShareActions = []
                return
            }
            root.hostRoot.contextSlmShareActions = actions || []
        }

        function onSlmDragDropActionResolved(requestId, action, error) {
            var rid = String(requestId || "")
            var pending = root.hostRoot.pendingSlmDragDropRequests || ({})
            var meta = pending[rid]
            if (!meta) {
                return
            }
            var next = {}
            var keys = Object.keys(pending)
            for (var i = 0; i < keys.length; ++i) {
                if (keys[i] !== rid) {
                    next[keys[i]] = pending[keys[i]]
                }
            }
            root.hostRoot.pendingSlmDragDropRequests = next

            if (action && action.id) {
                root.hostRoot.invokeSlmCapabilityAction("DragDrop", action.id, meta.sourceUris)
            } else {
                var sourcePaths = meta.sourceUris
                var targetPath = meta.targetUri
                var copyMode = meta.copyMode
                var res = {}
                if (copyMode) {
                    res = root.fileManagerApi.startCopyPaths(sourcePaths, targetPath, false)
                } else {
                    res = root.fileManagerApi.startMovePaths(sourcePaths, targetPath, false)
                }
                root.hostRoot.notifyResult(copyMode ? "Copy" : "Move", res)
            }
        }
    }
}
