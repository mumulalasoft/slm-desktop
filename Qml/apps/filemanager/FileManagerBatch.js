.pragma library

function syncDeepSearchBusyCursor(root, cursorController) {
    if (!cursorController) {
        return
    }
    var running = !!(root.fileModel && root.fileModel.deepSearchRunning)
    if (running) {
        if (cursorController.startBusy) {
            cursorController.startBusy(-1)
        }
    } else if (cursorController.stopBusy) {
        cursorController.stopBusy()
    }
}

function batchOperationLabel(typeValue) {
    var t = String(typeValue || "").toLowerCase()
    if (t === "copy")
        return "Copying"
    if (t === "move")
        return "Moving"
    if (t === "delete")
        return "Deleting"
    if (t === "trash")
        return "Moving to Trash"
    if (t === "restore")
        return "Restoring"
    if (t === "extract")
        return "Extracting"
    if (t === "compress")
        return "Compressing"
    return "Processing"
}

function updateBatchOverlayFromApi(root, batchOverlayPopup) {
    // Batch progress overlay is provided globally in Main.qml via GlobalProgressCenter.
    root.batchOverlayVisible = false
    if (batchOverlayPopup && batchOverlayPopup.opened) {
        batchOverlayPopup.close()
    }
}

function toggleBatchPauseResume(root, fileManagerApi) {
    if (!fileManagerApi || !fileManagerApi.batchOperationActive) {
        return
    }
    var op = String(fileManagerApi.batchOperationType || "").toLowerCase()
    if (op === "extract" || op === "compress") {
        return
    }
    if (!!fileManagerApi.batchOperationPaused) {
        if (fileManagerApi.resumeActiveBatchOperation) {
            var resumeRes = fileManagerApi.resumeActiveBatchOperation()
            if (resumeRes && !resumeRes.ok) {
                root.notifyResult("Resume", resumeRes)
            }
        }
        return
    }
    if (fileManagerApi.pauseActiveBatchOperation) {
        var pauseRes = fileManagerApi.pauseActiveBatchOperation()
        if (pauseRes && !pauseRes.ok) {
            root.notifyResult("Pause", pauseRes)
        }
    }
}

function cancelBatchOperation(root, fileManagerApi) {
    if (!fileManagerApi || !fileManagerApi.batchOperationActive
            || !fileManagerApi.cancelActiveBatchOperation) {
        return
    }
    var cancelRes = fileManagerApi.cancelActiveBatchOperation("keep-completed")
    if (cancelRes && !cancelRes.ok) {
        root.notifyResult("Cancel", cancelRes)
    }
}
