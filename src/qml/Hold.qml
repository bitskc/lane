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
    LayerShell.Window.scope: "tern-hold"
    LayerShell.Window.exclusionZone: -1

    readonly property real progress: controller.holdProgress

    Shortcut { sequence: "Escape"; onActivated: controller.cancelHold() }
    Shortcut { sequence: "Space"; onActivated: controller.cancelHold() }
    Shortcut { sequence: "Return"; onActivated: controller.confirmHold() }
    Shortcut { sequence: "Enter"; onActivated: controller.confirmHold() }

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

    Kirigami.ShadowedRectangle {
        id: card
        width: 360
        height: content.implicitHeight + 32
        anchors.centerIn: parent
        radius: 16
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.97)
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        shadow.size: 32
        shadow.yOffset: 10
        shadow.color: Qt.rgba(0, 0, 0, 0.42)
        scale: root.visible ? 1 : 0.96
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

        Column {
            id: content
            width: parent.width - 32
            x: 16
            y: 16
            spacing: 12

            RowLayout {
                width: parent.width
                spacing: 10
                Kirigami.Icon {
                    source: "launch"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    color: Kirigami.Theme.highlightColor
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
                color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
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
