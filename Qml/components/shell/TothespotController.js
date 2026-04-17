.pragma library
.import "ShellUtils.js" as ShellUtils

function _appModel(shell) {
    if (shell && shell.appModelRef) {
        return shell.appModelRef
    }
    if (typeof AppModel !== "undefined" && AppModel) {
        return AppModel
    }
    return null
}

function _fileApi(shell) {
    if (shell && shell.fileManagerApiRef) {
        return shell.fileManagerApiRef
    }
    if (typeof FileManagerApi !== "undefined" && FileManagerApi) {
        return FileManagerApi
    }
    return null
}

function _tothespotService(shell) {
    if (shell && shell.tothespotServiceRef) {
        return shell.tothespotServiceRef
    }
    if (typeof TothespotService !== "undefined" && TothespotService) {
        return TothespotService
    }
    return null
}

function _globalMenuSearchRows(queryText, limit) {
    if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager) {
        return []
    }
    if (!GlobalMenuManager.available || !GlobalMenuManager.topLevelMenus || !GlobalMenuManager.menuItemsFor) {
        return []
    }
    var menus = GlobalMenuManager.topLevelMenus
    if (!menus || menus.length <= 0) {
        return []
    }
    var q = String(queryText || "").trim().toLowerCase()
    var maxRows = Number(limit || 24)
    if (maxRows <= 0) {
        maxRows = 24
    }
    var rows = []
    var activeAppId = String(GlobalMenuManager.activeAppId || "").trim()
    for (var mi = 0; mi < menus.length; ++mi) {
        if (rows.length >= maxRows) {
            break
        }
        var menu = menus[mi] || ({})
        var menuId = Number(menu.id || -1)
        var menuLabel = String(menu.label || "").trim()
        if (menuId <= 0 || menuLabel.length <= 0) {
            continue
        }
        var items = GlobalMenuManager.menuItemsFor(menuId)
        if (!items || items.length <= 0) {
            continue
        }
        for (var ii = 0; ii < items.length; ++ii) {
            if (rows.length >= maxRows) {
                break
            }
            var item = items[ii] || ({})
            var itemId = Number(item.id || -1)
            var itemLabel = String(item.label || "").trim()
            if (itemId <= 0 || itemLabel.length <= 0 || itemLabel === "-") {
                continue
            }
            var haystack = (menuLabel + " " + itemLabel + " " + activeAppId).toLowerCase()
            if (q.length > 0 && haystack.indexOf(q) < 0) {
                continue
            }
            rows.push({
                          "resultId": "",
                          "provider": "global_menu",
                          "type": "action",
                          "isAction": true,
                          "resultKind": "action",
                          "sectionTitle": "Actions",
                          "name": itemLabel,
                          "path": menuLabel,
                          "intent": menuLabel + " > " + itemLabel,
                          "actionId": "globalmenu:" + menuId + ":" + itemId,
                          "menuId": menuId,
                          "menuItemId": itemId,
                          "iconName": "view-list-symbolic",
                          "score": 170
                      })
        }
    }
    return rows
}

function _setSearchVisible(shell, visible) {
    if (!shell) {
        return
    }
    if (shell.setSearchVisible) {
        shell.setSearchVisible(visible)
        return
    }
    shell.tothespotVisible = !!visible
}

function refreshProfileMeta(shell) {
    var svc = _tothespotService(shell)
    if (!svc || !svc.activeSearchProfileMeta) {
        shell.tothespotProfileMeta = ({})
        return
    }
    var meta = svc.activeSearchProfileMeta()
    shell.tothespotProfileMeta = meta ? meta : ({})
}

function refreshTelemetryMeta(shell) {
    var svc = _tothespotService(shell)
    if (!svc || !svc.telemetryMeta) {
        shell.tothespotTelemetryMeta = ({})
        return
    }
    var meta = svc.telemetryMeta()
    shell.tothespotTelemetryMeta = meta ? meta : ({})
}

function refreshTelemetryLast(shell) {
    var svc = _tothespotService(shell)
    if (!svc || !svc.activationTelemetry) {
        shell.tothespotTelemetryLast = ({})
        return
    }
    var rows = svc.activationTelemetry(1)
    if (rows && rows.length > 0) {
        shell.tothespotTelemetryLast = rows[rows.length - 1]
    } else {
        shell.tothespotTelemetryLast = ({})
    }
}

function _seedFallbackEmptyQuery(shell, resultsModel, maxSeed) {
    var limit = Number(maxSeed || 24)
    if (limit <= 0) {
        limit = 24
    }
    var fallbackSeed = [
        {
            "name": "Settings",
            "desktopId": "slm-settings.desktop",
            "executable": "slm-settings",
            "iconName": "preferences-system-symbolic"
        },
        {
            "name": "Files",
            "desktopId": "org.gnome.Nautilus.desktop",
            "executable": "nautilus",
            "iconName": "system-file-manager-symbolic"
        },
        {
            "name": "Terminal",
            "desktopId": "org.kde.konsole.desktop",
            "executable": "konsole",
            "iconName": "utilities-terminal-symbolic"
        }
    ]
    for (var fs = 0; fs < fallbackSeed.length && resultsModel.count < limit; ++fs) {
        var f = fallbackSeed[fs]
        resultsModel.append({
                                "resultId": "",
                                "provider": "apps",
                                "type": "app",
                                "isApp": true,
                                "isAction": false,
                                "resultKind": "app",
                                "sectionTitle": "Top Apps",
                                "name": String(f.name || "Application"),
                                "path": String(f.desktopId || f.executable || ""),
                                "absolutePath": "",
                                "iconName": String(f.iconName || "application-x-executable-symbolic"),
                                "isDir": false,
                                "desktopId": String(f.desktopId || ""),
                                "desktopFile": "",
                                "executable": String(f.executable || ""),
                                "iconSource": ""
                            })
    }
    if (resultsModel.count > 0) {
        shell.tothespotSelectedIndex = 0
        shell.tothespotProviderStats = ({
                                            "apps": Number(resultsModel.count || 0),
                                            "recent": 0,
                                            "seedSource": "fallback-static"
                                        })
        refreshPreview(shell, resultsModel)
    }
}

function refreshResults(shell, resultsModel, forceReload) {
    var force = !!forceReload
    var appModel = _appModel(shell)
    var fileApi = _fileApi(shell)
    var svc = _tothespotService(shell)
    var generation = Number(shell.tothespotQueryGeneration || 0)
    resultsModel.clear()
    shell.tothespotSelectedIndex = -1
    shell.tothespotProviderStats = ({})
    var q = String(shell.tothespotQuery || "").trim()
    var statCache = ({})
    function cachedStat(pathValue) {
        var p = String(pathValue || "")
        if (p.length === 0) {
            return ({})
        }
        if (statCache[p] !== undefined) {
            return statCache[p]
        }
        var st = (fileApi && fileApi.statPath) ? fileApi.statPath(p) : ({})
        statCache[p] = st
        return st
    }
    function appEntryKey(desktopIdValue, desktopFileValue, executableValue, nameValue) {
        var did = String(desktopIdValue || "").toLowerCase()
        if (did.length > 0) {
            return "id:" + did
        }
        var dfile = String(desktopFileValue || "").toLowerCase()
        if (dfile.length > 0) {
            return "desktop:" + dfile
        }
        var exe = String(executableValue || "").toLowerCase()
        if (exe.length > 0) {
            return "exe:" + exe
        }
        return "name:" + String(nameValue || "").toLowerCase()
    }
    function parseIsoMs(isoValue) {
        var iso = String(isoValue || "")
        if (iso.length <= 0) {
            return 0
        }
        var ms = Date.parse(iso)
        return isNaN(ms) ? 0 : Number(ms)
    }
    if (q.length === 0) {
        var maxSeed = 24
        var seen = ({})
        var appSeedCount = 0
        var recentSeedCount = 0

        try {
            if (appModel && appModel.frequentApps) {
                var top = appModel.frequentApps(8)
                for (var ai = 0; ai < top.length; ++ai) {
                    var app = top[ai]
                    if (!app) {
                        continue
                    }
                    var did = String(app.desktopId || "")
                    var dfile = String(app.desktopFile || "")
                    var exe = String(app.executable || "")
                    var appKey = "app:" + appEntryKey(did, dfile, exe, String(app.name || ""))
                    if (appKey === "app:" || seen[appKey]) {
                        continue
                    }
                    seen[appKey] = true
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "apps",
                                            "type": "app",
                                            "isApp": true,
                                            "isAction": false,
                                            "resultKind": "app",
                                            "sectionTitle": "Top Apps",
                                            "name": String(app.name || "Application"),
                                            "path": did.length > 0 ? did : exe,
                                            "absolutePath": "",
                                            "iconName": String(app.iconName || "application-x-executable-symbolic"),
                                            "isDir": false,
                                            "desktopId": did,
                                            "desktopFile": dfile,
                                            "executable": exe,
                                            "iconSource": String(app.iconSource || "")
                                        })
                    appSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eTopApps) {
            console.log("[slm-tothespot] empty-query top-apps seed failed:", String(eTopApps))
        }

        // Fallback when frequent app telemetry is still empty: seed from normal app page.
        try {
            if (appSeedCount <= 0
                    && resultsModel.count < maxSeed
                    && appModel
                    && appModel.page) {
                var pageRows = appModel.page(0, Math.max(8, maxSeed), "")
                for (var pi = 0; pi < pageRows.length; ++pi) {
                    var prow = pageRows[pi]
                    if (!prow) {
                        continue
                    }
                    var pdid = String(prow.desktopId || "")
                    var pdfile = String(prow.desktopFile || "")
                    var pexe = String(prow.executable || "")
                    var pkey = "app:" + appEntryKey(pdid, pdfile, pexe, String(prow.name || ""))
                    if (pkey === "app:" || seen[pkey]) {
                        continue
                    }
                    seen[pkey] = true
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "apps",
                                            "type": "app",
                                            "isApp": true,
                                            "isAction": false,
                                            "resultKind": "app",
                                            "sectionTitle": "Top Apps",
                                            "name": String(prow.name || "Application"),
                                            "path": pdid.length > 0 ? pdid : pexe,
                                            "absolutePath": "",
                                            "iconName": String(prow.iconName || "application-x-executable-symbolic"),
                                            "isDir": false,
                                            "desktopId": pdid,
                                            "desktopFile": pdfile,
                                            "executable": pexe,
                                            "iconSource": String(prow.iconSource || "")
                                        })
                    appSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eAppPage) {
            console.log("[slm-tothespot] empty-query app-page seed failed:", String(eAppPage))
        }

        try {
            if (resultsModel.count < maxSeed
                    && fileApi
                    && fileApi.recentFiles) {
                var left = Math.max(0, maxSeed - resultsModel.count)
                var recents = fileApi.recentFiles(Math.max(10, left))
                for (var ri = 0; ri < recents.length; ++ri) {
                    var rec = recents[ri]
                    if (!rec) {
                        continue
                    }
                    var rp = String(rec.path || "")
                    if (rp.length === 0) {
                        continue
                    }
                    var fkey = "file:" + rp.toLowerCase()
                    if (seen[fkey]) {
                        continue
                    }
                    seen[fkey] = true
                    var fileName = rp
                    var slashPos = Math.max(fileName.lastIndexOf("/"), fileName.lastIndexOf("\\"))
                    if (slashPos >= 0 && slashPos + 1 < fileName.length) {
                        fileName = fileName.substring(slashPos + 1)
                    }
                    var stat = cachedStat(rp)
                    var rowIsDir = !!(stat && stat.ok && stat.isDir)
                    var rowMime = (stat && stat.ok) ? String(stat.mimeType || "") : ""
                    var rowFallbackIcon = (stat && stat.ok) ? String(stat.iconName || "") : ""
                    resultsModel.append({
                                            "resultId": "",
                                            "provider": "recent",
                                            "type": "path",
                                            "isApp": false,
                                            "isAction": false,
                                            "resultKind": rowIsDir ? "folder" : "file",
                                            "sectionTitle": "Recent Files",
                                            "name": fileName,
                                            "path": rp,
                                            "absolutePath": rp,
                                            "iconName": ShellUtils.tothespotIconForMime(rowMime, rowIsDir, rowFallbackIcon),
                                            "isDir": rowIsDir,
                                            "desktopId": "",
                                            "desktopFile": "",
                                            "executable": "",
                                            "iconSource": ""
                                        })
                    recentSeedCount++
                    if (resultsModel.count >= maxSeed) {
                        break
                    }
                }
            }
        } catch (eRecent) {
            console.log("[slm-tothespot] empty-query recents seed failed:", String(eRecent))
        }

        if (resultsModel.count > 0) {
            console.log("[slm-tothespot] seed-empty-query apps=", appSeedCount,
                        "recent=", recentSeedCount, "rows=", resultsModel.count)
            shell.tothespotSelectedIndex = 0
            shell.tothespotProviderStats = ({
                                                "apps": Number(appSeedCount || 0),
                                                "recent": Number(recentSeedCount || 0),
                                                "seedSource": "local-cache"
                                            })
            refreshPreview(shell, resultsModel)
            return
        }
        // Guaranteed non-empty fallback so empty-query UI never looks broken even
        // before telemetry/recent providers warm up.
        _seedFallbackEmptyQuery(shell, resultsModel, maxSeed)
        if (resultsModel.count > 0) {
            console.log("[slm-tothespot] seed-fallback rows=", resultsModel.count,
                        "appModel=", !!appModel, "fileApi=", !!fileApi, "service=", !!svc)
            return
        }
        console.log("[slm-tothespot] seed-empty-query-none appModel=", !!appModel,
                    "fileApi=", !!fileApi, "service=", !!svc)
        // If local seed sources are unavailable/empty, continue and query backend
        // with empty text so frequent apps + recent files can still be shown.
    }
    if (q.length > 0 && q.length < 2) {
        shell.tothespotLastLoadedQuery = ""
        shell.tothespotAppliedGeneration = generation
        shell.tothespotPreviewData = ({})
        shell.tothespotProviderStats = ({})
        return
    }
    if (!force
            && q === String(shell.tothespotLastLoadedQuery || "")
            && shell.tothespotAppliedGeneration === generation) {
        return
    }
    if (!svc || !svc.query) {
        return
    }
    var topAppKeys = ({})
    if (appModel && appModel.frequentApps) {
        var topForBoost = appModel.frequentApps(24)
        for (var ti = 0; ti < topForBoost.length; ++ti) {
            var ta = topForBoost[ti]
            if (!ta) {
                continue
            }
            var tkDid = String(ta.desktopId || "").toLowerCase()
            var tkDfile = String(ta.desktopFile || "").toLowerCase()
            var tkExe = String(ta.executable || "").toLowerCase()
            if (tkDid.length > 0) topAppKeys["id:" + tkDid] = true
            if (tkDfile.length > 0) topAppKeys["desktop:" + tkDfile] = true
            if (tkExe.length > 0) topAppKeys["exe:" + tkExe] = true
        }
    }
    var recentPathInfo = ({})
    if (fileApi && fileApi.recentFiles) {
        var recentForBoost = fileApi.recentFiles(120)
        for (var rbi = 0; rbi < recentForBoost.length; ++rbi) {
            var rr = recentForBoost[rbi]
            if (!rr) {
                continue
            }
            var rPath = String(rr.path || "").toLowerCase()
            if (rPath.length > 0) {
                recentPathInfo[rPath] = {
                    "recent": true,
                    "lastOpenedMs": parseIsoMs(rr.lastOpened)
                }
            }
        }
    }
    var rows = svc.query(q, {}, 24)
    var globalMenuRows = _globalMenuSearchRows(q, 40)
    if (globalMenuRows.length > 0) {
        rows = rows.concat(globalMenuRows)
    }
    if (generation !== Number(shell.tothespotQueryGeneration || 0)) {
        return
    }
    var groupedApps = []
    var groupedActions = []
    var groupedSettings = []
    var groupedRecent = []
    var groupedFolders = []
    var groupedFiles = []
    var dedupedRows = ({})
    function providerPriority(providerValue) {
        var pv = String(providerValue || "")
        if (pv === "global_menu") return 4
        if (pv === "apps") return 3
        if (pv === "recent") return 2
        return 1
    }
    var nowMs = Date.now()
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        if (!row) {
            continue
        }
        var rowType = String(row.type || "path")
        var rowProvider = String(row.provider || "")
        var rowAbsPath = String(row.absolutePath || row.path || "")
        var rowIsApp = rowType === "app" || rowProvider === "apps"
        var rowIsAction = rowType === "action"
                          || rowProvider === "slm_actions"
                          || rowProvider === "global_menu"
        var rowIsSettings = rowType === "settings"
                            || rowType === "module"
                            || rowType === "setting"
                            || rowProvider === "settings"
        var normalizedIsDir = !!row.isDir
        var normalizedIconName = String(row.iconName || "text-x-generic")
        if (!rowIsApp && !rowIsAction && rowAbsPath.length > 0) {
            var st2 = cachedStat(rowAbsPath)
            if (st2 && st2.ok) {
                var stMime = String(st2.mimeType || "")
                var stIsDir = !!st2.isDir
                var stFallbackIcon = String(st2.iconName || normalizedIconName)
                normalizedIsDir = stIsDir
                normalizedIconName = ShellUtils.tothespotIconForMime(stMime, stIsDir, stFallbackIcon)
            }
        }
        var baseRank = Number(row.score || 0)
        var boostRank = 0
        if (rowIsApp) {
            var kDid = String(row.desktopId || "").toLowerCase()
            var kDfile = String(row.desktopFile || "").toLowerCase()
            var kExe = String(row.executable || "").toLowerCase()
            if ((kDid.length > 0 && topAppKeys["id:" + kDid])
                    || (kDfile.length > 0 && topAppKeys["desktop:" + kDfile])
                    || (kExe.length > 0 && topAppKeys["exe:" + kExe])) {
                boostRank += 35
            }
        } else {
            var pKey = rowAbsPath.toLowerCase()
            var rMeta = (pKey.length > 0 && recentPathInfo[pKey]) ? recentPathInfo[pKey] : null
            if (rMeta) {
                var openedMs = Number(rMeta.lastOpenedMs || 0)
                if (openedMs > 0 && nowMs > openedMs) {
                    var ageDays = Math.max(0, (nowMs - openedMs) / 86400000.0)
                    var decay = Math.exp((-1.0 * ageDays) / 7.0)
                    boostRank += Math.round(32 * decay)
                } else {
                    boostRank += 10
                }
            }
        }
        var finalRank = baseRank + boostRank
        var sectionTitle = "Files"
        if (rowIsApp) {
            sectionTitle = "Top Apps"
        } else if (rowIsAction) {
            sectionTitle = "Actions"
        } else if (rowIsSettings) {
            sectionTitle = "Settings"
        } else if (rowProvider === "recent" || rowProvider === "recent_files") {
            sectionTitle = "Recent Files"
        } else if (normalizedIsDir) {
            sectionTitle = "Folders"
        }
        var synthesizedDeepLink = String(row.deepLink || "").trim()
        if (synthesizedDeepLink.length <= 0 && rowIsSettings) {
            var moduleId = String(row.moduleId || "").trim()
            var settingId = String(row.settingId || "").trim()
            var resultIdRaw = String(row.resultId || "").trim()
            if (moduleId.length <= 0 || settingId.length <= 0) {
                if (resultIdRaw.indexOf("settings:setting:") === 0) {
                    var payload = resultIdRaw.substring(String("settings:setting:").length)
                    var splitPos = payload.indexOf("/")
                    if (splitPos > 0) {
                        moduleId = payload.substring(0, splitPos)
                        settingId = payload.substring(splitPos + 1)
                    }
                } else if (resultIdRaw.indexOf("settings:module:") === 0) {
                    moduleId = resultIdRaw.substring(String("settings:module:").length)
                }
            }
            if (moduleId.length > 0 && settingId.length > 0) {
                synthesizedDeepLink = "settings://" + moduleId + "/" + settingId
            } else if (moduleId.length > 0) {
                synthesizedDeepLink = "settings://" + moduleId
            }
        }
        var itemRow = {
            "resultId": String(row.resultId || ""),
            "provider": rowProvider,
            "type": rowType,
            "isApp": rowIsApp,
            "isAction": rowIsAction,
            "resultKind": rowIsApp ? "app"
                                   : (rowIsAction ? "action"
                                                  : (rowIsSettings ? "settings"
                                                                   : (normalizedIsDir ? "folder" : "file"))),
            "name": String(row.name || ""),
            "path": String(row.path || rowAbsPath),
            "absolutePath": rowAbsPath,
            "deepLink": synthesizedDeepLink,
            "intent": String(row.intent || row.path || ""),
            "actionId": String(row.actionId || ""),
            "menuId": Number(row.menuId || -1),
            "menuItemId": Number(row.menuItemId || -1),
            "iconName": normalizedIconName,
            "isDir": normalizedIsDir,
            "desktopId": String(row.desktopId || ""),
            "desktopFile": String(row.desktopFile || ""),
            "executable": String(row.executable || ""),
            "iconSource": String(row.iconSource || ""),
            "_rank": finalRank,
            "_providerPriority": providerPriority(rowProvider),
            "sectionTitle": sectionTitle
        }
        if (sectionTitle === "Actions") {
            itemRow.isAction = true
            itemRow.isApp = false
            itemRow.type = "action"
            itemRow.provider = "slm_actions"
            itemRow.resultKind = "action"
        }
        var dedupeKey = ""
        if (rowIsApp) {
            dedupeKey = "app:" + appEntryKey(itemRow.desktopId,
                                             itemRow.desktopFile,
                                             itemRow.executable,
                                             itemRow.name)
        } else if (rowIsAction || itemRow.sectionTitle === "Actions") {
            var actionNameKey = String(itemRow.name || "").trim().toLowerCase()
            var actionIntentKey = String(itemRow.intent || itemRow.path || "").trim().toLowerCase()
            if (actionIntentKey.length <= 0) {
                actionIntentKey = String(itemRow.actionId || "").trim().toLowerCase()
            }
            dedupeKey = "action:" + actionNameKey + "|" + actionIntentKey
        } else {
            dedupeKey = "path:" + String(itemRow.absolutePath || itemRow.path || "").toLowerCase()
        }
        if (dedupeKey.length <= 5) {
            dedupeKey = "raw:" + String(itemRow.name || "") + "|" + String(itemRow.path || "")
        }
        var existing = dedupedRows[dedupeKey]
        if (!existing) {
            dedupedRows[dedupeKey] = itemRow
        } else {
            var existingRank = Number(existing._rank || 0)
            var existingPri = Number(existing._providerPriority || 0)
            if (finalRank > existingRank
                    || (finalRank === existingRank
                        && Number(itemRow._providerPriority || 0) > existingPri)) {
                dedupedRows[dedupeKey] = itemRow
            }
        }
    }
    for (var dedupeKeyIt in dedupedRows) {
        var deduped = dedupedRows[dedupeKeyIt]
        if (!deduped) {
            continue
        }
        if (deduped.sectionTitle === "Top Apps") {
            groupedApps.push(deduped)
        } else if (deduped.sectionTitle === "Actions") {
            groupedActions.push(deduped)
        } else if (deduped.sectionTitle === "Settings") {
            groupedSettings.push(deduped)
        } else if (deduped.sectionTitle === "Recent Files") {
            groupedRecent.push(deduped)
        } else if (deduped.sectionTitle === "Folders") {
            groupedFolders.push(deduped)
        } else {
            groupedFiles.push(deduped)
        }
    }
    groupedApps.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedActions.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedSettings.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedRecent.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedFolders.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    groupedFiles.sort(function(a, b) { return Number(b._rank || 0) - Number(a._rank || 0) })
    var orderedCandidates = []
    for (var ai2 = 0; ai2 < groupedApps.length; ++ai2) orderedCandidates.push(groupedApps[ai2])
    for (var ac2 = 0; ac2 < groupedActions.length; ++ac2) orderedCandidates.push(groupedActions[ac2])
    for (var st2 = 0; st2 < groupedSettings.length; ++st2) orderedCandidates.push(groupedSettings[st2])
    for (var ri2 = 0; ri2 < groupedRecent.length; ++ri2) orderedCandidates.push(groupedRecent[ri2])
    for (var fi2 = 0; fi2 < groupedFolders.length; ++fi2) orderedCandidates.push(groupedFolders[fi2])
    for (var si2 = 0; si2 < groupedFiles.length; ++si2) orderedCandidates.push(groupedFiles[si2])

    var totalCandidates = orderedCandidates.length
    var providerCap = Math.max(1, Math.floor(totalCandidates * 0.60))
    var providerCounts = ({})
    var skippedByCap = []
    for (var ci = 0; ci < orderedCandidates.length; ++ci) {
        var cand = orderedCandidates[ci]
        if (!cand) continue
        if (String(cand.sectionTitle || "") === "Actions") {
            cand.isAction = true
            cand.isApp = false
            cand.type = "action"
            cand.provider = "slm_actions"
            cand.resultKind = "action"
        }
        var p = String(cand.provider || "unknown")
        var c = Number(providerCounts[p] || 0)
        if (c >= providerCap) {
            skippedByCap.push(cand)
            continue
        }
        providerCounts[p] = c + 1
        resultsModel.append(cand)
    }
    if (resultsModel.count < Math.min(10, totalCandidates)) {
        for (var si3 = 0; si3 < skippedByCap.length; ++si3) {
            if (String(skippedByCap[si3].sectionTitle || "") === "Actions") {
                skippedByCap[si3].isAction = true
                skippedByCap[si3].isApp = false
                skippedByCap[si3].type = "action"
                skippedByCap[si3].provider = "slm_actions"
                skippedByCap[si3].resultKind = "action"
            }
            resultsModel.append(skippedByCap[si3])
        }
    }
    providerCounts["seedSource"] = "provider-query"
    shell.tothespotProviderStats = providerCounts
    if (resultsModel.count > 0) {
        shell.tothespotSelectedIndex = 0
    }
    shell.tothespotLastLoadedQuery = q
    shell.tothespotAppliedGeneration = generation
    refreshPreview(shell, resultsModel)
}

function refreshPreview(shell, resultsModel) {
    if (shell.tothespotSelectedIndex < 0 || shell.tothespotSelectedIndex >= resultsModel.count) {
        shell.tothespotPreviewData = ({})
        return
    }
    var item = resultsModel.get(shell.tothespotSelectedIndex)
    if (!item) {
        shell.tothespotPreviewData = ({})
        return
    }
    var kind = String(item.resultKind || "")
    var sectionTitle = String(item.sectionTitle || "")
    var isApp = kind === "app"
                || !!item.isApp
                || String(item.type || "") === "app"
                || String(item.provider || "") === "apps"
    var isAction = sectionTitle === "Actions"
                   || kind === "action"
                   || !!item.isAction
                   || String(item.type || "") === "action"
                   || String(item.provider || "") === "slm_actions"
                   || String(item.provider || "") === "global_menu"
    var isSettings = kind === "settings"
                     || String(item.type || "") === "settings"
                     || String(item.type || "") === "module"
                     || String(item.type || "") === "setting"
                     || String(item.provider || "") === "settings"
    if (isApp) {
        shell.tothespotPreviewData = {
            "kind": "app",
            "name": String(item.name || "Application"),
            "iconName": String(item.iconName || "application-x-executable-symbolic"),
            "desktopId": String(item.desktopId || ""),
            "desktopFile": String(item.desktopFile || ""),
            "executable": String(item.executable || "")
        }
        return
    }
    if (isAction) {
        shell.tothespotPreviewData = {
            "kind": "action",
            "name": String(item.name || "Action"),
            "intent": String(item.intent || item.path || ""),
            "iconName": String(item.iconName || "system-run-symbolic"),
            "actionId": String(item.actionId || ""),
            "menuId": Number(item.menuId || -1),
            "menuItemId": Number(item.menuItemId || -1),
            "desktopId": String(item.desktopId || ""),
            "desktopFile": String(item.desktopFile || "")
        }
        return
    }
    if (isSettings) {
        shell.tothespotPreviewData = {
            "kind": "settings",
            "name": String(item.name || "Setting"),
            "path": String(item.path || ""),
            "deepLink": String(item.deepLink || ""),
            "iconName": String(item.iconName || "preferences-system-symbolic")
        }
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length <= 0 || typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.statPath) {
        shell.tothespotPreviewData = ({})
        return
    }
    var st = FileManagerApi.statPath(p)
    shell.tothespotPreviewData = {
        "kind": "path",
        "name": String(item.name || ""),
        "path": p,
        "isDir": !!(st && st.ok && st.isDir),
        "mimeType": (st && st.ok) ? String(st.mimeType || "") : "",
        "size": (st && st.ok) ? Number(st.size || 0) : 0,
        "modified": (st && st.ok) ? String(st.modified || st.lastModified || "") : ""
    }
}

function showActionNotification(summary, body, iconName, timeoutMs) {
    if (typeof NotificationManager === "undefined" || !NotificationManager || !NotificationManager.Notify) {
        return
    }
    NotificationManager.Notify("Slm Desktop",
                               0,
                               String(iconName || "system-search-symbolic"),
                               String(summary || ""),
                               String(body || ""),
                               [],
                               {
                                   "source": "tothespot"
                               },
                               Math.max(800, Number(timeoutMs || 1600)))
}

function focusFromMenu(shell, tothespotWindow) {
    if (!shell) {
        return
    }
    _setSearchVisible(shell, true)
    Qt.callLater(function() {
        if (tothespotWindow && tothespotWindow.focusSearchField) {
            tothespotWindow.focusSearchField()
        }
    })
}

function activateResult(shell, resultsModel, indexValue) {
    var idx = Number(indexValue)
    if (idx < 0 || idx >= resultsModel.count) {
        return
    }
    var item = resultsModel.get(idx)
    if (!item) {
        return
    }
    var rid = String(item.resultId || "")
    var provider = String(item.provider || "")
    if (provider === "global_menu") {
        var menuId = Number(item.menuId || -1)
        var menuItemId = Number(item.menuItemId || -1)
        if (menuId > 0 && menuItemId > 0
                && typeof GlobalMenuManager !== "undefined"
                && GlobalMenuManager
                && GlobalMenuManager.activateMenuItem) {
            _setSearchVisible(shell, false)
            GlobalMenuManager.activateMenuItem(menuId, menuItemId)
            var nowIso = (new Date()).toISOString()
            shell.tothespotTelemetryLast = ({
                                                "when": nowIso,
                                                "provider": "global_menu",
                                                "action": "invoke",
                                                "resultType": "action",
                                                "name": String(item.name || ""),
                                                "intent": String(item.intent || ""),
                                                "menuId": menuId,
                                                "menuItemId": menuItemId
                                            })
            var prevPs = shell.tothespotProviderStats ? shell.tothespotProviderStats : ({})
            var nextPs = ({})
            for (var pKey in prevPs) {
                nextPs[pKey] = prevPs[pKey]
            }
            var ga = Number(nextPs.globalMenuActivations || 0)
            nextPs.globalMenuActivations = ga + 1
            shell.tothespotProviderStats = nextPs
            return
        }
    }
    if (rid.length > 0
            && typeof TothespotService !== "undefined"
            && TothespotService
            && TothespotService.activateResult) {
        if (provider === "clipboard" && TothespotService.resolveClipboardResult) {
            _setSearchVisible(shell, false)
            var resolveReply = TothespotService.resolveClipboardResult(rid, {
                                                                           "query": String(shell.tothespotQuery || ""),
                                                                           "provider": provider,
                                                                           "scope": "tothespot",
                                                                           "source_app": "org.slm.tothespot",
                                                                           "text": String(shell.tothespotQuery || ""),
                                                                           "initiatedByUserGesture": true,
                                                                           "initiatedFromOfficialUI": true,
                                                                           "recopyBeforePaste": true
                                                                       })
            var resolveOk = !!(resolveReply && resolveReply.ok)
            if (!resolveOk) {
                var err = String((resolveReply && resolveReply.error) ? resolveReply.error : "resolve-failed")
                showActionNotification("Clipboard resolve failed",
                                       err,
                                       "dialog-error-symbolic",
                                       2200)
            } else if (shell.tothespotNotifyClipboardResolveSuccess) {
                var resolvedTitle = String((resolveReply && resolveReply.title)
                                           ? resolveReply.title : (item.name || "Clipboard item"))
                showActionNotification("Clipboard item ready",
                                       resolvedTitle,
                                       "edit-paste-symbolic",
                                       1400)
            }
            refreshTelemetryLast(shell)
            return
        }
        var resultType = String(item.type || "")
        if (resultType.length <= 0) {
            resultType = !!item.isDir ? "folder" : "file"
        } else if (resultType === "path") {
            resultType = !!item.isDir ? "folder" : "file"
        }
        var action = "open"
        if (resultType === "app" || provider === "apps") {
            action = "launch"
        } else if (resultType === "action" || provider === "slm_actions") {
            action = "invoke"
        }
        var resolvedPath = String(item.absolutePath || item.path || "")
        var contextUris = []
        if (resolvedPath.length > 0) {
            contextUris = [resolvedPath]
        }
        _setSearchVisible(shell, false)
        TothespotService.activateResult(rid, {
                                            "query": String(shell.tothespotQuery || ""),
                                            "provider": provider,
                                            "result_type": resultType,
                                            "action": action,
                                            "scope": "tothespot",
                                            "source_app": "org.slm.tothespot",
                                            "text": String(shell.tothespotQuery || ""),
                                            "initiatedByUserGesture": true,
                                            "initiatedFromOfficialUI": true,
                                            "selection_count": contextUris.length,
                                            "uris": contextUris
                                        })
        refreshTelemetryLast(shell)
        return
    }
    if (String(item.type || "") === "app") {
        _setSearchVisible(shell, false)
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.entry", {
                                       "type": "desktop",
                                       "name": String(item.name || ""),
                                       "desktopId": String(item.desktopId || ""),
                                       "desktopFile": String(item.desktopFile || ""),
                                       "executable": String(item.executable || ""),
                                       "iconName": String(item.iconName || ""),
                                       "iconSource": String(item.iconSource || "")
                                   }, "tothespot")
        }
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length === 0) {
        return
    }
    _setSearchVisible(shell, false)
    if (!!item.isDir) {
        ShellUtils.openDetachedFileManager(shell, p)
        return
    }
    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
        AppCommandRouter.route("filemanager.open", { "target": p }, "tothespot")
    }
}

function activateContextAction(shell, resultsModel, indexValue, action) {
    var idx = Number(indexValue)
    var act = String(action || "")
    if (idx < 0 || idx >= resultsModel.count) {
        return
    }
    var itemCurrent = resultsModel.get(idx)
    var isSettings = !!itemCurrent
            && (String(itemCurrent.provider || "") === "settings"
                || String(itemCurrent.type || "") === "settings"
                || String(itemCurrent.type || "") === "module"
                || String(itemCurrent.type || "") === "setting"
                || String(itemCurrent.resultKind || "") === "settings")
    if ((act === "openSetting" || (act === "open" && isSettings))) {
        activateResult(shell, resultsModel, idx)
        return
    }
    if (act === "copyDeepLink") {
        if (!itemCurrent) {
            return
        }
        var deepLink = String(itemCurrent.deepLink || "")
        if (deepLink.length <= 0) {
            var rid = String(itemCurrent.resultId || "")
            if (rid.startsWith("settings:setting:")) {
                var payload = rid.substring(String("settings:setting:").length)
                deepLink = "settings://" + payload
            } else if (rid.startsWith("settings:module:")) {
                deepLink = "settings://" + rid.substring(String("settings:module:").length)
            }
        }
        if (deepLink.length <= 0) {
            return
        }
        var copied = false
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
            var copiedRes = FileManagerApi.copyTextToClipboard(deepLink)
            copied = !!(copiedRes && copiedRes.ok)
        }
        showActionNotification(copied ? "Deep link copied" : "Failed to copy",
                               copied ? deepLink : "",
                               copied ? "edit-copy-symbolic" : "dialog-error-symbolic",
                               copied ? 1600 : 2200)
        return
    }
    if (act === "launch" || act === "open") {
        activateResult(shell, resultsModel, idx)
        return
    }
    if (act === "copyPath" || act === "copyName" || act === "copyUri") {
        var itemCopy = resultsModel.get(idx)
        if (!itemCopy || !!itemCopy.isApp) {
            return
        }
        var sourcePath = String(itemCopy.absolutePath || itemCopy.path || "")
        if (sourcePath.length <= 0) {
            return
        }
        var valueToCopy = sourcePath
        var summaryText = "Path copied"
        if (act === "copyName") {
            var slashPos = Math.max(sourcePath.lastIndexOf("/"), sourcePath.lastIndexOf("\\"))
            valueToCopy = slashPos >= 0 ? sourcePath.substring(slashPos + 1) : sourcePath
            summaryText = "Name copied"
        } else if (act === "copyUri") {
            valueToCopy = "file://" + encodeURIComponent(sourcePath).replace(/%2F/g, "/")
            summaryText = "URI copied"
        }
        var copyOk = false
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
            var copyRes = FileManagerApi.copyTextToClipboard(valueToCopy)
            copyOk = !!(copyRes && copyRes.ok)
        }
        showActionNotification(copyOk ? summaryText : "Failed to copy",
                               copyOk ? valueToCopy : "",
                               copyOk ? "edit-copy-symbolic" : "dialog-error-symbolic",
                               copyOk ? 1600 : 2200)
        return
    }
    if (act !== "openContainingFolder") {
        if (act !== "properties") {
            return
        }
        var itemProps = resultsModel.get(idx)
        if (!itemProps) {
            return
        }
        if (isSettings) {
            activateResult(shell, resultsModel, idx)
            return
        }
        var propPath = ""
        if (!!itemProps.isApp) {
            propPath = String(itemProps.desktopFile || itemProps.executable || "")
        } else {
            propPath = String(itemProps.absolutePath || itemProps.path || "")
        }
        if (propPath.length <= 0) {
            return
        }
        var propParent = propPath
        var slashPosProps = Math.max(propPath.lastIndexOf("/"), propPath.lastIndexOf("\\"))
        if (slashPosProps > 0) {
            propParent = propPath.substring(0, slashPosProps)
        }
        _setSearchVisible(shell, false)
        ShellUtils.openDetachedFileManager(shell, propParent)
        ShellUtils.requestDetachedProperties(shell, propPath)
        return
    }

    var item = resultsModel.get(idx)
    if (!item) {
        return
    }
    var p = String(item.absolutePath || item.path || "")
    if (p.length === 0) {
        return
    }
    var lastSlash = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"))
    var parentDir = (lastSlash > 0) ? p.substring(0, lastSlash) : "~"
    _setSearchVisible(shell, false)
    ShellUtils.openDetachedFileManager(shell, parentDir)
}
