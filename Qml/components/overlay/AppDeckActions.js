.pragma library

function handleAppChosen(appData, commandRouter, executionGate) {
    if (!appData) {
        console.warn("[appdeck-launch] ignored: empty appData")
        return
    }
    console.log("[appdeck-launch] chosen name=" + String(appData.name || appData.display || "")
                + " desktopId=" + String(appData.desktopId || "")
                + " desktopFile=" + String(appData.desktopFile || "")
                + " executable=" + String(appData.executable || ""))
    if (typeof CursorController !== "undefined" && CursorController && CursorController.startBusy) {
        CursorController.startBusy(1300)
    }
    if (commandRouter && commandRouter.routeWithResult) {
        var routed = commandRouter.routeWithResult("app.entry", appData, "appdeck")
        console.log("[appdeck-launch] route result ok=" + String(routed && routed.ok)
                    + " error=" + String(routed && routed.error || "")
                    + " durationMs=" + String(routed && routed.durationMs || 0))
        return
    }
    if (commandRouter && commandRouter.route) {
        var ok = commandRouter.route("app.entry", appData, "appdeck")
        console.log("[appdeck-launch] route result ok=" + String(ok))
        return
    }
    if (executionGate && executionGate.launchEntryMap) {
        var launched = executionGate.launchEntryMap(appData, "appdeck")
        console.log("[appdeck-launch] gate result ok=" + String(launched))
        return
    }
    console.warn("[appdeck-launch] ignored: no router/gate available")
}

function _norm(v) {
    return String(v || "").trim().toLowerCase()
}

function _resolveDesktopFile(entry) {
    if (!entry) {
        return ""
    }

    var direct = String(entry.desktopFile || "").trim()
    if (direct.length > 0) {
        return direct
    }

    var desktopId = String(entry.desktopId || "").trim()
    var appId = String(entry.appId || "").trim()
    var executable = String(entry.executable || "").trim()
    var name = String(entry.name || entry.display || "").trim()

    // Some sources may already pass an absolute .desktop path in desktopId/appId.
    if (desktopId.length > 0 && desktopId.indexOf(".desktop") >= 0 && desktopId.indexOf("/") >= 0) {
        return desktopId
    }
    if (appId.length > 0 && appId.indexOf(".desktop") >= 0 && appId.indexOf("/") >= 0) {
        return appId
    }

    if (typeof AppModel === "undefined" || !AppModel || !AppModel.page) {
        return ""
    }

    var total = 0
    if (AppModel.countMatching) {
        total = Math.max(0, Number(AppModel.countMatching("") || 0))
    } else if (typeof AppModel.count !== "undefined") {
        total = Math.max(0, Number(AppModel.count || 0))
    }
    if (total <= 0) {
        return ""
    }

    var rows = AppModel.page(0, total, "") || []
    var did = _norm(desktopId)
    var aid = _norm(appId)
    var exe = _norm(executable)
    var nm = _norm(name)
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        if (!row) continue
        var rowDesktopFile = String(row.desktopFile || "").trim()
        if (rowDesktopFile.length <= 0) continue

        var rowDesktopId = _norm(row.desktopId)
        var rowExecutable = _norm(row.executable)
        var rowName = _norm(row.name || row.display)
        if ((did.length > 0 && rowDesktopId === did)
                || (aid.length > 0 && rowDesktopId === aid)
                || (exe.length > 0 && rowExecutable === exe)
                || (nm.length > 0 && rowName === nm)) {
            return rowDesktopFile
        }
    }
    return ""
}

function _candidateDesktopFiles(entry, resolvedDesktopFile) {
    var out = []
    function push(v) {
        var s = String(v || "").trim()
        if (s.length <= 0) {
            return
        }
        for (var i = 0; i < out.length; ++i) {
            if (out[i] === s) {
                return
            }
        }
        out.push(s)
    }
    function withDesktopSuffix(v) {
        var s = String(v || "").trim()
        if (s.length <= 0) {
            return ""
        }
        if (s.slice(-8).toLowerCase() === ".desktop") {
            return s
        }
        return s + ".desktop"
    }

    var desktopFile = String(resolvedDesktopFile || "").trim()
    var desktopId = String(entry && entry.desktopId || "").trim()
    var appId = String(entry && entry.appId || "").trim()
    var name = String(entry && (entry.name || entry.display) || "").trim()

    push(desktopFile)
    push(desktopId)
    push(appId)
    push(withDesktopSuffix(desktopId))
    push(withDesktopSuffix(appId))
    push(withDesktopSuffix(name))

    // Common desktop entry directories.
    var dirs = [
        "/usr/share/applications/",
        "/usr/local/share/applications/",
        "/var/lib/flatpak/exports/share/applications/",
        "/run/host/usr/share/applications/"
    ]
    for (var d = 0; d < dirs.length; ++d) {
        var dir = dirs[d]
        push(dir + withDesktopSuffix(desktopId))
        push(dir + withDesktopSuffix(appId))
        push(dir + withDesktopSuffix(name))
        if (desktopFile.length > 0 && desktopFile.indexOf("/") < 0) {
            push(dir + desktopFile)
        }
    }
    return out
}

function handleAddToDock(appData, appDeckModelRef) {
    var dockModel = appDeckModelRef
    if (!dockModel && typeof AppDeckModel !== "undefined" && AppDeckModel) {
        dockModel = AppDeckModel
    }
    if (!appData) {
        console.warn("[appdeck] addToDock skipped: empty appData")
        return false
    }
    if (!dockModel || !dockModel.addDesktopEntry) {
        console.warn("[appdeck] addToDock skipped: AppDeckModel unavailable")
        return false
    }

    var resolvedDesktopFile = _resolveDesktopFile(appData)
    var candidates = _candidateDesktopFiles(appData, resolvedDesktopFile)
    if (candidates.length <= 0) {
        console.warn("[appdeck] addToDock: desktopFile unresolved for", JSON.stringify(appData))
        return false
    }

    var ok = false
    var used = ""
    for (var i = 0; i < candidates.length; ++i) {
        var candidate = String(candidates[i] || "").trim()
        if (candidate.length <= 0) continue
        if (dockModel.addDesktopEntry(candidate)) {
            ok = true
            used = candidate
            break
        }
    }
    if (!ok) {
        console.warn("[appdeck] addToDock failed; candidates=", JSON.stringify(candidates))
    } else {
        console.log("[appdeck] addToDock ok:", used)
    }
    return ok
}

function _tryShortcutModelAddToDesktop(appData, candidates, logPrefix, shortcutModelRef) {
    var model = shortcutModelRef
    if (!model && typeof ShortcutModel !== "undefined" && ShortcutModel) {
        model = ShortcutModel
    }
    if (!model) {
        console.warn(logPrefix + " failed: ShortcutModel unavailable")
        return false
    }

    var data = appData || ({})
    var list = candidates || []
    for (var i = 0; i < list.length; ++i) {
        var c = String(list[i] || "").trim()
        if (c.length <= 0) {
            continue
        }
        if (model.addDesktopShortcut && model.addDesktopShortcut(c)) {
            console.log(logPrefix + " ok via ShortcutModel.addDesktopShortcut:", c)
            return true
        }
    }

    var desktopId = String(data.desktopId || "").trim()
    if (desktopId.length > 0 && model.addDesktopShortcutById
            && model.addDesktopShortcutById(desktopId)) {
        console.log(logPrefix + " ok via ShortcutModel.addDesktopShortcutById:", desktopId)
        return true
    }

    var executable = String(data.executable || "").trim()
    if (executable.length > 0 && model.addDesktopShortcut
            && model.addDesktopShortcut(executable)) {
        console.log(logPrefix + " ok via ShortcutModel executable:", executable)
        return true
    }

    var name = String(data.name || data.display || "").trim()
    if (name.length > 0 && model.addDesktopShortcut
            && model.addDesktopShortcut(name)) {
        console.log(logPrefix + " ok via ShortcutModel name:", name)
        return true
    }

    if (executable.length > 0 && model.addAppShortcut
            && model.addAppShortcut(name || "Application",
                                    executable,
                                    data.iconName || "")) {
        console.log(logPrefix + " ok via ShortcutModel.addAppShortcut:", name || executable)
        return true
    }

    console.warn(logPrefix + " failed: no ShortcutModel strategy accepted; candidates=",
                 JSON.stringify(list))
    return false
}

function handleAddToDesktop(appData, desktopScene, desktopSurfaceRef, shortcutModelRef) {
    if (!appData) {
        console.warn("[appdeck] addToDesktop skipped: empty appData")
        return false
    }

    var resolved = _resolveDesktopFile(appData)
    var candidates = _candidateDesktopFiles(appData, resolved)
    var bestDesktopFile = (candidates.length > 0) ? candidates[0] : (appData.desktopFile || "")

    console.log("[appdeck] addToDesktop triggering for " + (appData.name || appData.display)
                + " resolved=" + resolved
                + " best=" + bestDesktopFile)

    // Try to find the first candidate that actually exists if possible, or just use the resolved one.
    // Since we don't have easy access to a stat function here without a lot of overhead,
    // we'll pass the most likely absolute path to addAppEntryToDesktop.
    var augmentedData = Object.assign({}, appData)
    if (bestDesktopFile.length > 0) {
        augmentedData.desktopFile = bestDesktopFile
    }
    augmentedData.desktopFileCandidates = candidates.slice(0)
    if (!augmentedData.source || String(augmentedData.source).trim().length <= 0) {
        augmentedData.source = "appdeck"
    }

    var desktopSurface = desktopSurfaceRef
    if (!desktopSurface && desktopScene && desktopScene.desktopFileManagerContent) {
        desktopSurface = desktopScene.desktopFileManagerContent
    }

    if (desktopSurface && desktopSurface.addAppEntryToDesktop) {
        console.log("[appdeck] addToDesktop using desktopFileManagerContent")
        var surfaceAdded = desktopSurface.addAppEntryToDesktop(augmentedData, -1, -1)
        if (surfaceAdded === true) {
            return true
        }
        console.warn("[appdeck] addToDesktop desktopFileManagerContent rejected payload; falling back")
    } else {
        console.warn("[appdeck] addToDesktop desktopFileManagerContent unavailable; falling back"
                     + " surface=" + (!!desktopSurface)
                     + " scene=" + (!!desktopScene))
    }

    console.log("[appdeck] addToDesktop falling back to ShortcutModel")
    var added = _tryShortcutModelAddToDesktop(augmentedData,
                                              candidates,
                                              "[appdeck] addToDesktop",
                                              shortcutModelRef)

    if (added && desktopScene && desktopScene.refreshShellShortcuts) {
        desktopScene.refreshShellShortcuts()
    }
    return added
}
