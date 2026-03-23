import QtQuick 2.15

Item {
    id: root

    property bool active: false
    property var chooserApi: null

    function hasApi() {
        return !!(chooserApi
                  && chooserApi.portalChooserInputFocused
                  && chooserApi.portalChooserMoveSelection
                  && chooserApi.portalChooserSelectBoundary
                  && chooserApi.portalChooserSelectAllEntries
                  && chooserApi.portalChooserInvertSelection
                  && chooserApi.portalChooserClearSelectionAndPreview
                  && chooserApi.portalChooserClearWhenHasSelection
                  && chooserApi.portalChooserToggleCurrentSelection)
    }

    Shortcut {
        sequence: "Down"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(1, "single")
        }
    }

    Shortcut {
        sequence: "Shift+Down"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(1, "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+Down"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(1, "add")
        }
    }

    Shortcut {
        sequence: "Up"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-1, "single")
        }
    }

    Shortcut {
        sequence: "Shift+Up"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-1, "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+Up"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-1, "add")
        }
    }

    Shortcut {
        sequence: "Home"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(false, "single")
        }
    }

    Shortcut {
        sequence: "Shift+Home"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(false, "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+Home"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(false, "add")
        }
    }

    Shortcut {
        sequence: "End"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(true, "single")
        }
    }

    Shortcut {
        sequence: "Shift+End"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(true, "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+End"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserSelectBoundary(true, "add")
        }
    }

    Shortcut {
        sequence: "PgDown"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(root.chooserApi.portalChooserPageStep(), "single")
        }
    }

    Shortcut {
        sequence: "Shift+PgDown"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(root.chooserApi.portalChooserPageStep(), "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+PgDown"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(root.chooserApi.portalChooserPageStep(), "add")
        }
    }

    Shortcut {
        sequence: "PgUp"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-root.chooserApi.portalChooserPageStep(), "single")
        }
    }

    Shortcut {
        sequence: "Shift+PgUp"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-root.chooserApi.portalChooserPageStep(), "range")
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+PgUp"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserMoveSelection(-root.chooserApi.portalChooserPageStep(), "add")
        }
    }

    Shortcut {
        sequence: "Ctrl+A"
        enabled: root.active
        onActivated: {
            if (!root.hasApi()) {
                return
            }
            root.chooserApi.portalChooserSelectAllEntries()
        }
    }

    Shortcut {
        sequence: "Ctrl+I"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserInvertSelection()
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+A"
        enabled: root.active
        onActivated: {
            if (!root.hasApi() || root.chooserApi.portalChooserInputFocused()) {
                return
            }
            root.chooserApi.portalChooserClearSelectionAndPreview()
        }
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: root.active
        onActivated: {
            if (!root.hasApi()) {
                return
            }
            root.chooserApi.portalChooserClearWhenHasSelection()
        }
    }

    Shortcut {
        sequence: "Delete"
        enabled: root.active
        onActivated: {
            if (!root.hasApi()) {
                return
            }
            root.chooserApi.portalChooserClearWhenHasSelection()
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: root.active
        onActivated: {
            if (!root.hasApi()) {
                return
            }
            root.chooserApi.portalChooserToggleCurrentSelection(false)
        }
    }

    Shortcut {
        sequence: "Ctrl+Space"
        enabled: root.active
        onActivated: {
            if (!root.hasApi()) {
                return
            }
            if (!root.chooserApi.portalChooserAllowMultiple) {
                return
            }
            root.chooserApi.portalChooserToggleCurrentSelection(true)
        }
    }
}
