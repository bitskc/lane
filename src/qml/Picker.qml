import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    title: "Tern"
    width: Screen.width
    height: Screen.height

    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityExclusive
    LayerShell.Window.scope: "tern-picker"
    LayerShell.Window.exclusionZone: -1

    readonly property int maxRows: 6
    readonly property int rowHeight: 48

    onVisibleChanged: {
        if (visible) {
            width = Screen.width
            height = Screen.height
            filterField.text = ""
            list.currentIndex = 0
            filterField.forceActiveFocus()
        }
    }

    Shortcut { sequence: "Escape"; onActivated: controller.cancelPicker() }
    Shortcut { sequence: "Ctrl+C"; onActivated: controller.copyCurrent() }
    Shortcut { sequence: "Return"; onActivated: controller.pick(list.currentIndex) }
    Shortcut { sequence: "Enter"; onActivated: controller.pick(list.currentIndex) }
    Shortcut { sequence: "Down"; onActivated: list.incrementCurrentIndex() }
    Shortcut { sequence: "Up"; onActivated: list.decrementCurrentIndex() }
    Shortcut { sequence: "Alt+A"; onActivated: controller.alwaysForHost = !controller.alwaysForHost }
    Shortcut {
        sequence: ","
        enabled: filterField.text.length === 0 && controller.destinationIndex < controller.destinationLadder.length - 1
        onActivated: controller.destinationIndex = controller.destinationIndex + 1
    }
    Shortcut {
        sequence: "."
        enabled: filterField.text.length === 0 && controller.destinationIndex > 0
        onActivated: controller.destinationIndex = controller.destinationIndex - 1
    }
    Repeater {
        model: 9
        Shortcut {
            sequence: String(index + 1)
            enabled: filterField.text.length === 0
            onActivated: controller.pick(index)
        }
    }

    Rectangle {
        id: dim
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.38)
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        MouseArea {
            anchors.fill: parent
            onClicked: if (controller.closeOnFocusLoss) controller.cancelPicker()
        }
    }

    Kirigami.ShadowedRectangle {
        id: card
        width: 440
        height: content.implicitHeight + 32
        anchors.centerIn: parent
        radius: 16
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.97)
        borderWidth: 1
        borderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        shadow.size: 32
        shadow.yOffset: 10
        shadow.color: Qt.rgba(0, 0, 0, 0.42)
        scale: root.visible ? 1 : 0.98
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

        Column {
            id: content
            width: parent.width - 32
            x: 16
            y: 16
            spacing: 10

            RowLayout {
                width: parent.width
                spacing: 10
                Kirigami.Icon {
                    source: controller.currentSecure ? "lock" : "internet-services"
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    color: controller.currentSecure ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                }
                Column {
                    Layout.fillWidth: true
                    spacing: 1
                    QQC.Label {
                        width: parent.width
                        text: controller.currentHost.length ? controller.currentHost : "Open link"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    QQC.Label {
                        width: parent.width
                        visible: controller.showUrl && controller.currentPrettyUrl.length > 0
                        text: controller.currentPrettyUrl
                        opacity: 0.55
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 36
                radius: 10
                color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
                QQC.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    text: "Filter"
                    opacity: 0.4
                    font.pixelSize: 13
                    visible: filterField.text.length === 0
                }
                QQC.TextField {
                    id: filterField
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    background: Item {}
                    onTextChanged: {
                        controller.pickerModel.setFilter(text)
                        list.currentIndex = 0
                    }
                    Keys.onDownPressed: list.incrementCurrentIndex()
                    Keys.onUpPressed: list.decrementCurrentIndex()
                }
            }

            ListView {
                id: list
                width: parent.width
                height: Math.min(root.maxRows, Math.max(1, count)) * root.rowHeight
                model: controller.pickerModel
                clip: true
                spacing: 0
                currentIndex: 0
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 80
                delegate: Rectangle {
                    required property int index
                    required property string name
                    required property string subtitle
                    required property string iconName
                    required property string shortcut
                    required property bool suggested
                    required property string kind

                    width: ListView.view.width
                    height: root.rowHeight
                    radius: 10
                    color: ListView.isCurrentItem
                           ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.36)
                           : "transparent"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: list.currentIndex = index
                        onClicked: controller.pick(index)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10
                        Kirigami.Icon {
                            source: iconName
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 0
                            QQC.Label {
                                width: parent.width
                                text: name
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            QQC.Label {
                                width: parent.width
                                text: kind === "pwa" ? "App" : (kind === "action" ? "Action" : subtitle)
                                font.pixelSize: 11
                                opacity: 0.5
                                elide: Text.ElideRight
                            }
                        }
                        Rectangle {
                            visible: shortcut.length > 0 && filterField.text.length === 0
                            width: 20
                            height: 20
                            radius: 5
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                            QQC.Label {
                                anchors.centerIn: parent
                                text: shortcut
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                opacity: 0.75
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
            }

            RowLayout {
                width: parent.width
                QQC.CheckBox {
                    id: alwaysBox
                    checked: controller.alwaysForHost
                    onToggled: controller.alwaysForHost = checked
                    text: controller.currentDestinationKey.length > 0
                          ? "Always for " + controller.currentDestinationKey
                          : "Always for this site"
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 2
                    QQC.ToolButton {
                        text: "‹"
                        enabled: controller.destinationIndex > 0
                        onClicked: controller.destinationIndex = controller.destinationIndex - 1
                        flat: true
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                    }
                    QQC.Label {
                        text: controller.currentDestinationKey
                        font.pixelSize: 10
                        opacity: 0.5
                        elide: Text.ElideRight
                        Layout.preferredWidth: 120
                    }
                    QQC.ToolButton {
                        text: "›"
                        enabled: controller.destinationIndex < controller.destinationLadder.length - 1
                        onClicked: controller.destinationIndex = controller.destinationIndex + 1
                        flat: true
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                    }
                }
                QQC.Label {
                    text: "esc"
                    font.pixelSize: 10
                    font.family: "monospace"
                    opacity: 0.4
                }
            }
        }
    }
}
