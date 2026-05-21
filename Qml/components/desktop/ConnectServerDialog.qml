import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Popup {
    id: root

    property alias host: hostField.text
    property alias port: portSpin.value
    property alias share: shareField.text
    property alias folder: folderField.text
    property alias domain: domainField.text
    property alias user: userField.text
    property alias password: passwordField.text
    
    property int typeIndex: 4
    property var types: [{
        label: "Public FTP",
        scheme: "ftp",
        port: 21
    }, {
        label: "SFTP",
        scheme: "sftp",
        port: 22
    }, {
        label: "SSH",
        scheme: "ssh",
        port: 22
    }, {
        label: "FTP with Login",
        scheme: "ftp",
        port: 21
    }, {
        label: "Windows Share",
        scheme: "smb",
        port: 445
    }]
    
    property bool busy: false
    property string error: ""

    signal connectRequested(var serverDetails)

    readonly property var currentType: types[Math.max(0, typeIndex)] || types[0]
    readonly property string currentScheme: String((currentType && currentType.scheme) ? currentType.scheme : "smb")
    readonly property bool isWindowsShare: currentScheme === "smb"
    readonly property bool isPublicFtp: typeIndex === 0
    readonly property bool showUserDetails: !isPublicFtp
    
    readonly property color fieldColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
    readonly property color fieldFocusColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color dividerColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.10)

    function openConnectServer() {
        open()
        Qt.callLater(function() {
            hostField.forceActiveFocus()
        })
    }

    modal: true
    focus: true
    parent: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    width: 488
    height: showUserDetails ? (isWindowsShare ? 420 : 384) : 342
    x: Math.round((Overlay.overlay.width - width) * 0.5)
    y: Math.round((Overlay.overlay.height - height) * 0.34)
    padding: 0
    onClosed: {
        busy = false
        error = ""
    }

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: Theme.darkMode ? Qt.rgba(0.14, 0.14, 0.15, 1.0) : Qt.rgba(0.96, 0.96, 0.96, 1.0)
        border.width: Theme.borderWidthThin
        border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.14)
        antialiasing: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 32
            radius: parent.radius
            color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.50)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 31
            height: 1
            color: root.dividerColor
        }
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            anchors.topMargin: 48
            anchors.bottomMargin: 50
            spacing: 8

            DSStyle.Label {
                Layout.fillWidth: true
                text: "Server Details"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("body")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 9
                rowSpacing: 6

                DSStyle.Label {
                    text: "Type:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                DSStyle.ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    model: types
                    textRole: "label"
                    currentIndex: typeIndex
                    onActivated: function(index) {
                        typeIndex = index
                        port = Number(types[Math.max(0, index)].port || 21)
                        error = ""
                    }
                }

                DSStyle.Label {
                    text: "Server:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    TextField {
                        id: hostField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        enabled: !busy
                        placeholderText: "Server name or IP address"
                        selectByMouse: true
                        color: Theme.color("textPrimary")
                        placeholderTextColor: Theme.color("accent")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("body")
                        leftPadding: 8
                        rightPadding: 8
                        verticalAlignment: TextInput.AlignVCenter
                        background: Rectangle {
                            radius: Theme.radiusSmPlus
                            color: hostField.activeFocus ? root.fieldFocusColor : root.fieldColor
                            border.width: Theme.borderWidthNone
                        }
                        onTextChanged: if (error.length > 0) error = ""
                        Keys.onReturnPressed: root.connectRequested(root.getDetails())
                        Keys.onEnterPressed: root.connectRequested(root.getDetails())
                    }

                    DSStyle.Label {
                        text: "Port:"
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("body")
                        Layout.preferredWidth: 38
                        horizontalAlignment: Text.AlignRight
                    }

                    SpinBox {
                        id: portSpin
                        enabled: !busy
                        from: 1
                        to: 65535
                        editable: true
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 28
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("body")
                        background: Rectangle {
                            radius: Theme.radiusSmPlus
                            color: root.fieldColor
                            border.width: Theme.borderWidthNone
                        }
                    }
                }

                DSStyle.Label {
                    visible: root.isWindowsShare
                    text: "Share:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    id: shareField
                    visible: root.isWindowsShare
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    placeholderText: "Name of share on server (Optional)"
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    placeholderTextColor: Theme.color("accent")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: if (error.length > 0) error = ""
                    Keys.onReturnPressed: root.connectRequested(root.getDetails())
                    Keys.onEnterPressed: root.connectRequested(root.getDetails())
                }

                DSStyle.Label {
                    text: "Folder:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    id: folderField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    placeholderText: "/"
                    text: "/"
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: if (error.length > 0) error = ""
                    Keys.onReturnPressed: root.connectRequested(root.getDetails())
                    Keys.onEnterPressed: root.connectRequested(root.getDetails())
                }
            }

            DSStyle.Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                visible: root.showUserDetails
                text: "User Details"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("body")
            }

            GridLayout {
                Layout.fillWidth: true
                visible: root.showUserDetails
                columns: 2
                columnSpacing: 9
                rowSpacing: 6

                DSStyle.Label {
                    visible: root.isWindowsShare
                    text: "Domain name:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    id: domainField
                    visible: root.isWindowsShare
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    text: "WORKGROUP"
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                }

                DSStyle.Label {
                    text: "User name:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    id: userField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    placeholderText: "Optional"
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    placeholderTextColor: Theme.color("textSecondary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    Keys.onReturnPressed: root.connectRequested(root.getDetails())
                    Keys.onEnterPressed: root.connectRequested(root.getDetails())
                }

                DSStyle.Label {
                    text: "Password:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !busy
                    echoMode: TextInput.Password
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    Keys.onReturnPressed: root.connectRequested(root.getDetails())
                    Keys.onEnterPressed: root.connectRequested(root.getDetails())
                }
            }

            DSStyle.Label {
                Layout.fillWidth: true
                visible: error.length > 0
                text: error
                color: Theme.color("error")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WrapAnywhere
                lineHeightMode: Text.ProportionalHeight
                lineHeight: Theme.lineHeight("normal")
            }
        }

        RowLayout {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 7

            DSStyle.Button {
                text: "Cancel"
                enabled: !busy
                onClicked: root.close()
            }

            DSStyle.Button {
                text: busy ? "Connecting..." : "Connect"
                enabled: !busy && String(host || "").trim().length > 0
                onClicked: root.connectRequested(root.getDetails())
            }
        }
    }

    function getDetails() {
        return {
            type: types[typeIndex],
            host: host,
            port: port,
            share: share,
            folder: folder,
            domain: domain,
            user: user,
            password: password
        }
    }
}
