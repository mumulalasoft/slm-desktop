.pragma library

function handleAppChosen(appData) {
    if (!appData) {
        return
    }
    if (typeof CursorController !== "undefined" && CursorController && CursorController.startBusy) {
        CursorController.startBusy(1300)
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        AppCommandRouter.route("app.entry", appData, "launchpad")
        return
    }
    if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchEntryMap) {
        AppExecutionGate.launchEntryMap(appData, "launchpad")
    }
}

function handleAddToDock(appData) {
    if (appData && appData.desktopFile && appData.desktopFile.length > 0
            && typeof DockModel !== "undefined" && DockModel && DockModel.addDesktopEntry) {
        DockModel.addDesktopEntry(appData.desktopFile)
    }
}

function handleAddToDesktop(appData, desktopScene) {
    if (!appData || typeof ShortcutModel === "undefined" || !ShortcutModel) {
        return
    }

    var added = false
    if (appData.desktopFile && appData.desktopFile.length > 0 && ShortcutModel.addDesktopShortcut) {
        added = ShortcutModel.addDesktopShortcut(appData.desktopFile)
    }
    if (!added && appData.desktopId && appData.desktopId.length > 0 && ShortcutModel.addDesktopShortcutById) {
        added = ShortcutModel.addDesktopShortcutById(appData.desktopId)
    }
    if (!added && appData.executable && appData.executable.length > 0 && ShortcutModel.addDesktopShortcut) {
        added = ShortcutModel.addDesktopShortcut(appData.executable)
    }
    if (!added && appData.name && appData.name.length > 0 && ShortcutModel.addDesktopShortcut) {
        added = ShortcutModel.addDesktopShortcut(appData.name)
    }
    if (!added && appData.executable && appData.executable.length > 0 && ShortcutModel.addAppShortcut) {
        added = ShortcutModel.addAppShortcut(appData.name || "Application",
                                             appData.executable,
                                             appData.iconName || "")
    }

    if (added && desktopScene && desktopScene.refreshShellShortcuts) {
        desktopScene.refreshShellShortcuts()
    }
}
