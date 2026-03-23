.pragma library
.import "ScreenshotFlow.js" as ScreenshotFlow
.import "ScreenshotSaveController.js" as ScreenshotSaveController

function beginAreaSelection(shell) {
    if (!shell) {
        return
    }
    shell.areaShotSelecting = true
    shell.areaShotFromScreenshotTool = false
    shell.areaShotOutputPath = ""
    shell.areaShotStartX = 0
    shell.areaShotStartY = 0
    shell.areaShotEndX = 0
    shell.areaShotEndY = 0
}

function cancelAreaSelection(shell) {
    if (!shell) {
        return
    }
    shell.areaShotSelecting = false
    shell.areaShotFromScreenshotTool = false
    shell.areaShotOutputPath = ""
}

function showResultNotification(shell, ok, path, errorText) {
    if (!shell) {
        return
    }
    if (typeof NotificationManager === "undefined" || !NotificationManager || !NotificationManager.Notify) {
        return
    }
    var summary = ok ? "Screenshot saved" : "Screenshot failed"
    var code = ok ? "" : ScreenshotFlow.normalizeErrorCode(errorText)
    shell.screenshotLastErrorCode = code
    var body = ok ? String(path || "") : ScreenshotFlow.errorMessage(code, String(errorText || ""))
    ScreenshotSaveController.log(shell, "notify", {
                            "ok": !!ok,
                            "path": String(path || ""),
                            "errorCode": code,
                            "rawError": String(errorText || "")
                        })
    NotificationManager.Notify("Slm Desktop",
                               0,
                               "camera-photo-symbolic",
                               summary,
                               body,
                               [],
                               ok ? ({
                                         "requestId": String(shell.screenshotRequestId || "")
                                     }) : ({
                                               "requestId": String(shell.screenshotRequestId || ""),
                                               "errorCode": code,
                                               "rawError": String(errorText || "")
                                           }),
                               3500)
}

function commitAreaSelection(shell) {
    if (!shell) {
        return
    }
    var x = Math.round(Math.min(shell.areaShotStartX, shell.areaShotEndX))
    var y = Math.round(Math.min(shell.areaShotStartY, shell.areaShotEndY))
    var w = Math.round(Math.abs(shell.areaShotEndX - shell.areaShotStartX))
    var h = Math.round(Math.abs(shell.areaShotEndY - shell.areaShotStartY))
    shell.areaShotSelecting = false
    if (w < 8 || h < 8) {
        shell.areaShotFromScreenshotTool = false
        shell.areaShotOutputPath = ""
        return
    }
    if (typeof CursorController !== "undefined" && CursorController && CursorController.startBusy) {
        CursorController.startBusy(800)
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        var result = AppCommandRouter.routeWithResult(
                    "screenshot.area",
                    {
                        "x": x,
                        "y": y,
                        "width": w,
                        "height": h,
                        "outputPath": shell.areaShotFromScreenshotTool ? shell.areaShotOutputPath : ""
                    },
                    "area-selector")
        var payload = result && result.payload ? result.payload : {}
        if (shell.areaShotFromScreenshotTool && !!(result && result.ok)) {
            ScreenshotSaveController.openSaveDialog(shell,
                                                    String(payload.path || shell.areaShotOutputPath),
                                                    shell.screenshotRequestId)
        } else {
            showResultNotification(shell,
                                   !!(result && result.ok),
                                   (payload.path || ""),
                                   (payload.error || result.error || ""))
        }
    }
    shell.areaShotFromScreenshotTool = false
    shell.areaShotOutputPath = ""
}

function performCapture(shell, modeValue) {
    if (!shell) {
        return
    }
    var mode = String(modeValue || "screen").toLowerCase()
    var outPath = ScreenshotSaveController.tempPath()
    ScreenshotSaveController.log(shell, "capture-start", {
                            "mode": mode,
                            "outputPath": outPath
                        })
    if (mode === "area") {
        shell.areaShotFromScreenshotTool = true
        shell.areaShotOutputPath = outPath
        beginAreaSelection(shell)
        return
    }
    if (mode === "window") {
        var geom = null
        if (typeof CompositorStateModel !== "undefined" && CompositorStateModel
                && CompositorStateModel.windowCount && CompositorStateModel.windowAt) {
            var count = CompositorStateModel.windowCount()
            var fallback = null
            for (var i = 0; i < count; ++i) {
                var w = CompositorStateModel.windowAt(i)
                if (!w || w.mapped === false || w.minimized === true) {
                    continue
                }
                if (!fallback) {
                    fallback = w
                }
                if (w.focused === true) {
                    geom = w
                    break
                }
            }
            if (!geom) {
                geom = fallback
            }
        }
        if (!geom || Number(geom.width || 0) <= 0 || Number(geom.height || 0) <= 0) {
            showResultNotification(shell, false, "", "window-not-found")
            return
        }
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            var winResult = AppCommandRouter.routeWithResult(
                        "screenshot.window",
                        {
                            "x": Math.round(Number(geom.x || 0)),
                            "y": Math.round(Number(geom.y || 0)),
                            "width": Math.round(Number(geom.width || 0)),
                            "height": Math.round(Number(geom.height || 0)),
                            "outputPath": outPath
                        },
                        "topbar.screenshot.window")
            var winPayload = winResult && winResult.payload ? winResult.payload : {}
            if (!!(winResult && winResult.ok)) {
                ScreenshotSaveController.openSaveDialog(shell,
                                                        String(winPayload.path || outPath),
                                                        shell.screenshotRequestId)
            } else {
                showResultNotification(shell, false, "",
                                       (winPayload.error || winResult.error || ""))
            }
        }
        return
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        var result = AppCommandRouter.routeWithResult(
                    "screenshot.fullscreen",
                    { "outputPath": outPath },
                    "topbar.screenshot")
        var payload = result && result.payload ? result.payload : {}
        if (!!(result && result.ok)) {
            ScreenshotSaveController.openSaveDialog(shell,
                                                    String(payload.path || outPath),
                                                    shell.screenshotRequestId)
        } else {
            showResultNotification(shell, false, "", (payload.error || result.error || ""))
        }
    }
}

function startFromTopBar(shell, modeValue, delaySecValue, grabPointerValue, concealTextValue) {
    if (!shell) {
        return
    }
    ScreenshotSaveController.beginRequest(shell, "topbar")
    shell.pendingScreenshotMode = String(modeValue || "screen")
    shell.pendingScreenshotDelaySec = Math.max(0, Number(delaySecValue || 0))
    if (typeof grabPointerValue !== "undefined" || typeof concealTextValue !== "undefined") {
        // Reserved for future backend options; keep API stable from UI.
    }
    if (shell.pendingScreenshotDelaySec <= 0) {
        performCapture(shell, shell.pendingScreenshotMode)
        return
    }
    if (shell.screenshotDelayTimer && shell.screenshotDelayTimer.restart) {
        shell.screenshotDelayTimer.restart()
    }
}
