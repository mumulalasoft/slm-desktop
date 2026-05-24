.pragma library

function handleAppChosen(appData) {
    if (!appData) {
        return
    }
    if (typeof CursorController !== "undefined" && CursorController && CursorController.startBusy) {
        CursorController.startBusy(1300)
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        AppCommandRouter.route("app.entry", appData, "apphub")
        return
    }
    if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchEntryMap) {
        AppExecutionGate.launchEntryMap(appData, "apphub")
    }
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
        console.warn("[apphub] addToDock skipped: empty appData")
        return false
    }
    if (!dockModel || !dockModel.addDesktopEntry) {
        console.warn("[apphub] addToDock skipped: AppDeckModel unavailable")
        return false
    }

    var resolvedDesktopFile = _resolveDesktopFile(appData)
    var candidates = _candidateDesktopFiles(appData, resolvedDesktopFile)
    if (candidates.length <= 0) {
        console.warn("[apphub] addToDock: desktopFile unresolved for", JSON.stringify(appData))
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
        console.warn("[apphub] addToDock failed; candidates=", JSON.stringify(candidates))
    } else {
        console.log("[apphub] addToDock ok:", used)
    }
    return ok
}

function handleAddToDesktop(appData, desktopScene) {
    if (!appData) {
        return
    }

    var resolved = _resolveDesktopFile(appData)
    var candidates = _candidateDesktopFiles(appData, resolved)
    var bestDesktopFile = (candidates.length > 0) ? candidates[0] : (appData.desktopFile || "")

    var augmentedData = Object.assign({}, appData)
    if (bestDesktopFile.length > 0) {
        augmentedData.desktopFile = bestDesktopFile
    }

    if (desktopScene
            && desktopScene.desktopFileManagerContent
            && desktopScene.desktopFileManagerContent.addAppEntryToDesktop) {
        desktopScene.desktopFileManagerContent.addAppEntryToDesktop(augmentedData, -1, -1)
        return
    }

    if (typeof ShortcutModel === "undefined" || !ShortcutModel) {
        return
    }

    var added = false
    // Use candidates for ShortcutModel too
    for (var i = 0; i < candidates.length; ++i) {
        var c = candidates[i]
        if (ShortcutModel.addDesktopShortcut && ShortcutModel.addDesktopShortcut(c)) {
            added = true
            break
        }
    }

    if (!added && augmentedData.desktopId && augmentedData.desktopId.length > 0 && ShortcutModel.addDesktopShortcutById) {
        added = ShortcutModel.addDesktopShortcutById(augmentedData.desktopId)
    }
    if (!added && augmentedData.executable && augmentedData.executable.length > 0 && ShortcutModel.addDesktopShortcut) {
        added = ShortcutModel.addDesktopShortcut(augmentedData.executable)
    }
    if (!added && augmentedData.name && augmentedData.name.length > 0 && ShortcutModel.addDesktopShortcut) {
        added = ShortcutModel.addDesktopShortcut(augmentedData.name)
    }
    if (!added && augmentedData.executable && augmentedData.executable.length > 0 && ShortcutModel.addAppShortcut) {
        added = ShortcutModel.addAppShortcut(augmentedData.name || "Application",
                                             augmentedData.executable,
                                             augmentedData.iconName || "")
    }

    if (added && desktopScene && desktopScene.refreshShellShortcuts) {
        desktopScene.refreshShellShortcuts()
    }
}
