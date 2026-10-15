import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Window
import app.lane.ui as LaneUi

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    title: "Lane"
    width: Screen.width
    height: Screen.height

    readonly property real progress: controller.holdProgress

    Shortcut { sequence: "Escape"; onActivated: controller.cancelHold() }
    Shortcut { sequence: "Space"; onActivated: controller.cancelHold() }
    Shortcut { sequence: "Return"; onActivated: controller.confirmHold() }
    Shortcut { sequence: "Enter"; onActivated: controller.confirmHold() }
    Shortcut { sequence: StandardKey.Quit; onActivated: controller.requestQuit() }

    Rectangle {
        id: dim
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.08)
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
        MouseArea {
            anchors.fill: parent
            onClicked: controller.cancelHold()
        }
    }

    LaneUi.ThemedShadow {
        id: card
        width: 360
        height: content.implicitHeight + 32
        anchors.centerIn: parent
        radius: 16
        fillColor: Qt.rgba(root.palette.window.r, root.palette.window.g, root.palette.window.b, 0.97)
        shadowColor: "#000000"
        shadowSize: 32
        shadowYOffset: 10
        shadowAlpha: 0.42
        scale: root.visible ? 1 : 0.96
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

        Accessible.role: Accessible.AlertMessage
        Accessible.name: "Opening in " + controller.holdTargetName
        Accessible.description: "Press Enter to open now, or Escape or Space to pick a different destination instead"

        Rectangle {
            anchors.fill: parent
            radius: 16
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.08)
        }

        Column {
            id: content
            width: parent.width - 32
            x: 16
            y: 16
            spacing: 12

            RowLayout {
                width: parent.width
                spacing: 10
                LaneUi.LaneIcon {
                    iconName: "launch"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
                Column {
                    Layout.fillWidth: true
                    spacing: 1
                    QQC.Label {
                        width: parent.width
                        text: "Opening in " + controller.holdTargetName
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    QQC.Label {
                        width: parent.width
                        text: controller.holdDestinationKey.length > 0 ? controller.holdDestinationKey : controller.currentPrettyUrl
                        opacity: 0.55
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        visible: text.length > 0
                    }
                }
            }

            QQC.ProgressBar {
                id: bar
                width: parent.width
                from: 0
                to: 1
                value: root.progress
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.08)
            }

            RowLayout {
                width: parent.width
                spacing: 6
                QQC.Label {
                    text: "enter"
                    font.pixelSize: 10
                    font.family: "monospace"
                    opacity: 0.5
                }
                QQC.Label {
                    text: "now"
                    font.pixelSize: 10
                    opacity: 0.4
                }
                Item { Layout.fillWidth: true }
                QQC.Label {
                    text: "esc / space"
                    font.pixelSize: 10
                    font.family: "monospace"
                    opacity: 0.5
                }
                QQC.Label {
                    text: "pick instead"
                    font.pixelSize: 10
                    opacity: 0.4
                }
            }
        }
    }
}
