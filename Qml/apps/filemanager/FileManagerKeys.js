.pragma library

function hasModifier(modifiers, flag) {
    return (modifiers & flag) !== 0
}

function actionFor(event, keys, ctrlModifier, shiftModifier, altModifier) {
    var key = event.key
    var mods = event.modifiers

    if (key === keys.Up) return "moveUp"
    if (key === keys.Down) return "moveDown"
    if (key === keys.Left) return "moveLeft"
    if (key === keys.Right) return "moveRight"
    if (key === keys.Home) return "selectFirst"
    if (key === keys.End) return "selectLast"
    if (key === keys.Return || key === keys.Enter) return "activate"
    if (key === keys.Escape) return "escape"
    if (key === keys.F2) return "rename"
    if (key === keys.Backspace) return "goUp"
    if (key === keys.Delete && hasModifier(mods, shiftModifier)) return "deletePermanent"
    if (key === keys.Delete) return "trash"

    if (hasModifier(mods, ctrlModifier) && key === keys.C) return "copy"
    if (hasModifier(mods, ctrlModifier) && key === keys.X) return "cut"
    if (hasModifier(mods, ctrlModifier) && key === keys.V) return "paste"
    if (hasModifier(mods, ctrlModifier) && key === keys.K) return "focusSearch"
    if (hasModifier(mods, ctrlModifier) && key === keys.L) return "focusSearch"
    if (hasModifier(mods, ctrlModifier) && key === keys.F) return "focusSearch"
    if (hasModifier(mods, ctrlModifier) && key === keys.N) return "newFolder"
    if (hasModifier(mods, ctrlModifier) && key === keys.A) return "selectAll"
    if (hasModifier(mods, altModifier) && key === keys.Digit1) return "filterAny"
    if (hasModifier(mods, altModifier) && key === keys.Digit2) return "filterFile"
    if (hasModifier(mods, altModifier) && key === keys.Digit3) return "filterDir"

    return ""
}

function handleWindowKey(root, event, contentView, toolbarSearchField, quickPreviewDialog) {
    var editingText = root.isEditingTextInput()
    if (event.key === Qt.Key_Escape && quickPreviewDialog && quickPreviewDialog.visible) {
        quickPreviewDialog.close()
        return true
    }
    if (event.key === Qt.Key_Escape) {
        if (root.toolbarSearchExpanded) {
            if (String(root.toolbarSearchText || "").length > 0) {
                root.toolbarSearchText = ""
                if (toolbarSearchField) {
                    toolbarSearchField.text = ""
                }
            } else {
                root.closeToolbarSearch(false)
            }
            return true
        }
        if (root.selectedEntryIndexes && root.selectedEntryIndexes.length > 0) {
            root.clearSelection()
            return true
        }
    }

    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_F) {
        root.openToolbarSearch(true)
        return true
    }
    if (!editingText && !root.toolbarSearchExpanded
            && event.modifiers === Qt.NoModifier
            && event.key === Qt.Key_Slash) {
        root.openToolbarSearch(false)
        return true
    }
    if (!editingText && root.isPrimaryModifier(event.modifiers)
            && event.key === Qt.Key_R) {
        root.refreshCurrent()
        return true
    }
    if (!editingText && root.isPrimaryModifier(event.modifiers)
            && (event.modifiers & Qt.ShiftModifier)
            && event.key === Qt.Key_N) {
        root.createNewFolder()
        return true
    }
    if (!editingText && root.isPrimaryModifier(event.modifiers)
            && event.key === Qt.Key_T) {
        root.addTab("~", true)
        return true
    }
    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_W) {
        root.closeTab(root.activeTabIndex)
        return true
    }
    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_A) {
        root.selectAllVisibleEntries()
        return true
    }
    if (!editingText && root.isPrimaryModifier(event.modifiers)
            && event.key === Qt.Key_P) {
        if (root.canPrintSelection && root.requestPrintSelection
                && root.canPrintSelection()) {
            root.requestPrintSelection()
        }
        return true
    }

    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_1) {
        root.viewMode = "grid"
        return true
    }
    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_2) {
        root.viewMode = "list"
        return true
    }
    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_3) {
        root.viewMode = "columns"
        return true
    }

    if ((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Left) {
        root.navigateBack()
        return true
    }
    if ((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Right) {
        root.navigateForward()
        return true
    }
    if (root.isPrimaryModifier(event.modifiers) && event.key === Qt.Key_Up) {
        if (root.fileModel && root.fileModel.goUp) {
            root.fileModel.goUp()
        }
        return true
    }
    if (!editingText && !root.toolbarSearchExpanded
            && event.modifiers === Qt.NoModifier
            && event.key === Qt.Key_Backspace) {
        if (root.fileModel && root.fileModel.goUp) {
            root.fileModel.goUp()
        }
        return true
    }
    if (!editingText && event.key === Qt.Key_Space && !root.toolbarSearchExpanded) {
        root.toggleQuickPreview()
        return true
    }

    var noModifiers = event.modifiers === Qt.NoModifier
    if (!editingText && !root.toolbarSearchExpanded && noModifiers
            && event.text !== undefined) {
        var typed = String(event.text || "")
        if (typed.length === 1 && typed >= " " && typed !== "\u007f") {
            if (root.handleTypeToSelect(typed)) {
                return true
            }
        }
    }

    if (root.viewMode === "columns" && contentView && contentView.columnsHandleKey) {
        if (contentView.columnsHandleKey(event.key, event.modifiers)) {
            return true
        }
    }
    if (contentView && contentView.handleNavigationKey) {
        if (contentView.handleNavigationKey(event.key, event.modifiers)) {
            return true
        }
    }
    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
        if (root.selectedEntryIndex >= 0) {
            root.contextEntryIndex = root.selectedEntryIndex
            root.openContextEntry()
        }
        return true
    }
    if (event.key === Qt.Key_F2) {
        root.renameSelected()
        return true
    }
    if (root.recentView && event.key === Qt.Key_Delete
            && (event.modifiers & Qt.ShiftModifier)) {
        root.requestClearRecentFiles()
        return true
    }
    if (event.key === Qt.Key_Delete && (event.modifiers & Qt.ShiftModifier)) {
        root.deleteSelected(true)
        return true
    }
    if (event.key === Qt.Key_Delete) {
        root.deleteSelected(false)
        return true
    }
    if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C) {
        root.copySelected(false)
        return true
    }
    if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_X) {
        root.copySelected(true)
        return true
    }
    if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_V) {
        root.pasteIntoCurrent()
        return true
    }
    return false
}
