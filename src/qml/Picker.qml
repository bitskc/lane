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

    onVisibleChanged: {
        if (visible) {
            width = Screen.width
            height = Screen.height
            filterField.text = ""
            list.currentIndex = 0
            filterField.forceActiveFocus()
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: controller.cancelPicker()
    }
    Shortcut {
        sequence: "Ctrl+C"
        onActivated: controller.copyCurrent()
    }
    Shortcut {
        sequence: "Return"
        onActivated: controller.pick(list.currentIndex)
    }
    Shortcut {
        sequence: "Enter"
        onActivated: controller.pick(list.currentIndex)
    }
    Shortcut {
        sequence: "Down"
        onActivated: list.incrementCurrentIndex()
    }
    Shortcut {
        sequence: "Up"
        onActivated: list.decrementCurrentIndex()
    }
    Shortcut {
        sequence: "Alt+A"
        onActivated: controller.alwaysForHost = !controller.alwaysForHost
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
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (controller.closeOnFocusLoss)
                    controller.cancelPicker()
            }
        }
    }

    Rectangle {
        id: card
        readonly property int chrome: 36 + 52 + 12 + 40 + 12 + 12 + 40
        readonly property int rows: Math.max(1, controller.pickerModel.count)
        readonly property int listWanted: Math.min(rows * 56, 360)
        width: Math.min(520, root.width - 48)
        height: Math.min(chrome + listWanted, root.height - 80)
        anchors.centerIn: parent
        radius: 18
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.94)
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        border.width: 1

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Kirigami.Icon {
                    source: "app.tern.Tern"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    QQC.Label {
                        text: controller.currentHost.length ? controller.currentHost : "Open link"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    QQC.Label {
                        visible: controller.showUrl
                        text: controller.currentPrettyUrl
                        opacity: 0.65
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }

            Kirigami.SearchField {
                id: filterField
                Layout.fillWidth: true
                placeholderText: "Filter browsers, profiles, apps"
                onTextChanged: controller.pickerModel.setFilter(text)
                KeyNavigation.down: list
            }

            QQC.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 52
                clip: true
                ListView {
                    id: list
                    model: controller.pickerModel
                    spacing: 4
                    currentIndex: 0
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Rectangle {
                        required property int index
                        required property string name
                        required property string subtitle
                        required property string iconName
                        required property string shortcut
                        required property bool suggested
                        required property string kind

                        width: ListView.view.width
                        height: 52
                        radius: 12
                        color: {
                            if (ListView.isCurrentItem)
                                return Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.28)
                            if (suggested)
                                return Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.10)
                            return "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: list.currentIndex = index
                            onClicked: controller.pick(index)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 12
                            spacing: 12
                            Kirigami.Icon {
                                source: iconName
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                QQC.Label {
                                    text: name
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                QQC.Label {
                                    text: kind === "pwa" ? "Installed app" : subtitle
                                    font.pixelSize: 11
                                    opacity: 0.6
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            Rectangle {
                                visible: shortcut.length > 0
                                width: 22
                                height: 22
                                radius: 6
                                color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                                QQC.Label {
                                    anchors.centerIn: parent
                                    text: shortcut
                                    font.pixelSize: 11
                                    opacity: 0.8
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                QQC.Switch {
                    id: alwaysSwitch
                    checked: controller.alwaysForHost
                    onToggled: controller.alwaysForHost = checked
                    text: controller.currentHost.length ? "Always for " + controller.currentHost : "Always for this site"
                }
                Item { Layout.fillWidth: true }
                QQC.Label {
                    text: "Esc cancel  ·  1–9 select"
                    opacity: 0.45
                    font.pixelSize: 11
                }
            }
        }
    }
}
