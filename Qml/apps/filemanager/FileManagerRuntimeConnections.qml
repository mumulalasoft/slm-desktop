import QtQuick 2.15

Item {
    id: root

    required property var hostRoot
    property var fileManagerApi: null

    Connections {
        target: root.fileManagerApi
        ignoreUnknownSignals: true

        function onFileBatchOperationFinished() {
            // Final batch notification is owned by BatchOperationIndicator to avoid timing races.
            root.hostRoot.updateBatchOverlayFromApi()
            if (root.hostRoot.fileModel && root.hostRoot.fileModel.refresh) {
                root.hostRoot.fileModel.refresh()
            }
        }

        function onBatchOperationStateChanged() {
            root.hostRoot.updateBatchOverlayFromApi()
        }

        function onOpenWithApplicationsReady(requestId, path, rows, error) {
            var rid = String(requestId || "")
            var requestedPath = String(path || "")
            if (rid.length > 0 && rid === root.hostRoot.propertiesOpenWithRequestId
                    && requestedPath === String(
                        root.hostRoot.propertiesOpenWithPath || "")) {
                root.hostRoot.propertiesOpenWithRequestId = ""
                root.hostRoot.propertiesOpenWithPath = ""
                var srcRows = rows || []
                var listRows = []
                var defaultId = ""
                for (var s = 0; s < srcRows.length; ++s) {
                    var srow = srcRows[s] || ({})
                    if (!!srow.defaultApp) {
                        listRows.push(srow)
                        defaultId = String((srow && srow.id) ? srow.id : "")
                        break
                    }
                }
                for (var r = 0; r < srcRows.length && listRows.length < 4; ++r) {
                    var rowR = srcRows[r] || ({})
                    var rowId = String((rowR && rowR.id) ? rowR.id : "")
                    if (rowId.length <= 0 || rowId === defaultId) {
                        continue
                    }
                    listRows.push(rowR)
                }
                root.hostRoot.propertiesOpenWithApps = listRows
                var defIdx = -1
                for (var i = 0; i < root.hostRoot.propertiesOpenWithApps.length; ++i) {
                    var row = root.hostRoot.propertiesOpenWithApps[i] || ({})
                    if (!!row.defaultApp) {
                        defIdx = i
                        break
                    }
                }
                if (defIdx < 0 && root.hostRoot.propertiesOpenWithApps.length > 0) {
                    defIdx = 0
                }
                root.hostRoot.propertiesOpenWithCurrentIndex = defIdx
                if (defIdx >= 0) {
                    var defRow = root.hostRoot.propertiesOpenWithApps[defIdx] || ({})
                    root.hostRoot.propertiesOpenWithName = String(
                                (defRow && defRow.name) ? defRow.name : "")
                } else if (String(error || "").length > 0) {
                    root.hostRoot.notifyResult("Open With", {
                                                   "ok": false,
                                                   "error": String(error)
                                               })
                }
                return
            }

            if (rid.length === 0 || rid !== root.hostRoot.contextOpenWithRequestId) {
                return
            }
            if (requestedPath !== String(root.hostRoot.contextOpenWithPath || "")) {
                return
            }
            root.hostRoot.contextOpenWithRequestId = ""
            root.hostRoot.contextOpenWithPath = ""
            root.hostRoot.contextOpenWithAllApps = root.hostRoot.sortOpenWithRows(
                        rows || [])
            root.hostRoot.contextOpenWithApps = root.hostRoot.contextRecommendedOpenWithEntries()
            if (root.hostRoot.contextOpenWithAllApps.length <= 0
                    && String(error || "").length > 0) {
                root.hostRoot.notifyResult("Open With", {
                                               "ok": false,
                                               "error": String(error)
                                           })
            }
        }

        function onOpenActionFinished(requestId, action, result) {
            var rid = String(requestId || "")
            if (rid.length === 0) {
                return
            }
            var pending = root.hostRoot.pendingOpenActions || {}
            var meta = pending[rid]
            if (!meta) {
                return
            }
            var title = String((meta && meta.title) ? meta.title : "Open")
            var refreshOpenWith = !!(meta && meta.refreshOpenWith)
            if (!result || !result.ok) {
                root.hostRoot.notifyResult(title, result || {
                                               "ok": false,
                                               "error": String(action
                                                               || "action-failed")
                                           })
            } else if (refreshOpenWith) {
                root.hostRoot.refreshContextOpenWithApps()
            }
            var next = {}
            var keys = Object.keys(pending)
            for (var i = 0; i < keys.length; ++i) {
                var k = keys[i]
                if (k === rid) {
                    continue
                }
                next[k] = pending[k]
            }
            root.hostRoot.pendingOpenActions = next
        }

        function onSlmContextActionsReady(requestId, actions, error) {
            var rid = String(requestId || "")
            if (rid.length <= 0 || rid !== String(root.hostRoot.contextSlmActionsRequestId || "")) {
                return
            }
            root.hostRoot.contextSlmActionsRequestId = ""
            root.hostRoot.contextSlmActions = actions || []
            if ((!actions || actions.length <= 0) && String(error || "").length > 0) {
                // keep silent for missing capability provider to avoid noisy UX when desktopd not running
                if (String(error).indexOf("capability-service-unavailable") < 0) {
                    root.hostRoot.notifyResult("Action", {
                                                   "ok": false,
                                                   "error": String(error)
                                               })
                }
            }
        }

        function onSlmCapabilityActionsReady(requestId, capability, actions, error) {
            var rid = String(requestId || "")
            var cap = String(capability || "")
            if (cap !== "Share") {
                return
            }
            if (rid.length <= 0 || rid !== String(root.hostRoot.contextSlmShareActionsRequestId || "")) {
                return
            }
            root.hostRoot.contextSlmShareActionsRequestId = ""
            root.hostRoot.contextSlmShareActions = actions || []
            if ((!actions || actions.length <= 0) && String(error || "").length > 0) {
                if (String(error).indexOf("capability-service-unavailable") < 0) {
                    root.hostRoot.notifyResult("Share", {
                                                   "ok": false,
                                                   "error": String(error)
                                               })
                }
            }
        }

        function onSlmContextActionInvoked(requestId, actionId, result) {
            var rid = String(requestId || "")
            if (rid.length <= 0) {
                return
            }
            var pending = root.hostRoot.pendingSlmActionRequests || {}
            var meta = pending[rid]
            if (!meta) {
                return
            }
            if (!result || !result.ok) {
                root.hostRoot.notifyResult("Action", result || {
                                               "ok": false,
                                               "error": String(actionId || "action-failed")
                                           })
            }
            var next = {}
            var keys = Object.keys(pending)
            for (var i = 0; i < keys.length; ++i) {
                var k = keys[i]
                if (k === rid) {
                    continue
                }
                next[k] = pending[k]
            }
            root.hostRoot.pendingSlmActionRequests = next
        }

        function onSlmCapabilityActionInvoked(requestId, capability, actionId, result) {
            var rid = String(requestId || "")
            var cap = String(capability || "")
            if (cap !== "Share" || rid.length <= 0) {
                return
            }
            var pending = root.hostRoot.pendingSlmActionRequests || {}
            var meta = pending[rid]
            if (!meta) {
                return
            }
            if (!result || !result.ok) {
                root.hostRoot.notifyResult("Share", result || {
                                               "ok": false,
                                               "error": String(actionId || "action-failed")
                                           })
            }
            var next = {}
            var keys = Object.keys(pending)
            for (var i = 0; i < keys.length; ++i) {
                var k = keys[i]
                if (k === rid) {
                    continue
                }
                next[k] = pending[k]
            }
            root.hostRoot.pendingSlmActionRequests = next
        }

        function onArchiveCompressFinished(archivePath, ok, error, result) {
            var payload = result || ({})
            if (!ok) {
                if (!payload.error && String(error || "").length > 0) {
                    payload.error = String(error)
                }
                root.hostRoot.notifyResult("Compress", payload)
                return
            }
            root.hostRoot.notifyResult("Compress", {
                                           "ok": true,
                                           "path": String(archivePath || "")
                                       })
            if (root.hostRoot.fileModel && root.hostRoot.fileModel.refresh) {
                root.hostRoot.fileModel.refresh()
            }
        }

        function onPortalFileChooserFinished(requestId, result) {
            var rid = String(requestId || "")
            if (rid.length <= 0 || rid !== String(
                        root.hostRoot.pendingPortalChooserRequestId || "")) {
                return
            }
            var action = String(root.hostRoot.pendingPortalChooserAction || "")
            var sources = root.hostRoot.pendingPortalChooserSources || []
            root.hostRoot.pendingPortalChooserRequestId = ""
            root.hostRoot.pendingPortalChooserAction = ""
            root.hostRoot.pendingPortalChooserSources = []
            var payload = result || ({})
            if (!payload || !payload.ok) {
                if (!!payload.canceled) {
                    return
                }
                root.hostRoot.notifyResult(action === "move" ? "Move" : "Copy",
                                           payload)
                return
            }
            var targetDir = String(payload.path || "")
            if (targetDir.length <= 0) {
                var paths = payload.paths || []
                if (paths.length > 0) {
                    targetDir = String(paths[0] || "")
                }
            }
            if (targetDir.length <= 0) {
                root.hostRoot.notifyResult(action === "move" ? "Move" : "Copy", {
                                               "ok": false,
                                               "error": "missing-destination"
                                           })
                return
            }
            if (!root.fileManagerApi) {
                return
            }
            var opRes
            if (action === "move") {
                opRes = root.fileManagerApi.startMovePaths
                        ? root.fileManagerApi.startMovePaths(
                            sources, targetDir, false)
                        : {
                            "ok": false,
                            "error": "async-move-api-unavailable"
                        }
            } else {
                opRes = root.fileManagerApi.startCopyPaths
                        ? root.fileManagerApi.startCopyPaths(
                            sources, targetDir, false)
                        : {
                            "ok": false,
                            "error": "async-copy-api-unavailable"
                        }
            }
            if (!opRes || !opRes.ok) {
                root.hostRoot.notifyResult(action === "move" ? "Move" : "Copy",
                                           opRes)
            }
        }
    }
}
