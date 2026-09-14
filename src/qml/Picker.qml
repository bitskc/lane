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
    title: "Lane"
    width: Screen.width
    height: Screen.height

    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityExclusive
    LayerShell.Window.scope: "lane-picker"
    LayerShell.Window.exclusionZone: -1

    readonly property int maxRows: 8
    readonly property int rowHeight: 48
    readonly property int sectionHeaderHeight: 26

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
    Shortcut { sequence: "Return"; onActivated: controller.pick(list.currentIndex) }
    Shortcut { sequence: "Enter"; onActivated: controller.pick(list.currentIndex) }
    Shortcut { sequence: "Down"; onActivated: list.incrementCurrentIndex() }
    Shortcut { sequence: "Up"; onActivated: list.decrementCurrentIndex() }
    Shortcut { sequence: "Alt+A"; onActivated: controller.alwaysForHost = !controller.alwaysForHost }

    Rectangle {
        id: dim
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.12)
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
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        shadow.size: 32
        shadow.yOffset: 10
        shadow.color: Qt.rgba(0, 0, 0, 0.42)
        scale: root.visible ? 1 : 0.98
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

        Accessible.role: Accessible.Dialog
        Accessible.name: "Open link"
        Accessible.description: controller.currentHost.length > 0
                                 ? "Choose where to open " + controller.currentHost
                                 : "Choose where to open this link"

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
                    // A focused TextField claims plain character keys (digits,
                    // comma, period) and Ctrl+C at the shortcut-override stage
                    // before a sibling Shortcut{} ever sees them, so those keys
                    // are handled here instead. Return/Enter/Escape/Up/Down/
                    // Alt+A are not claimed that way and stay as Shortcut{}
                    // above.
                    Keys.onPressed: (event) => {
                        if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_C) {
                            controller.copyCurrent()
                            event.accepted = true
                            return
                        }
                        if (filterField.text.length > 0 || event.modifiers !== Qt.NoModifier) {
                            return
                        }
                        if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                            const row = event.key - Qt.Key_1
                            if (row < root.maxRows) {
                                controller.pick(row)
                                event.accepted = true
                            }
                            return
                        }
                        if (event.key === Qt.Key_Comma) {
                            if (controller.destinationIndex < controller.destinationLadder.length - 1) {
                                controller.destinationIndex = controller.destinationIndex + 1
                                event.accepted = true
                            }
                        } else if (event.key === Qt.Key_Period) {
                            if (controller.destinationIndex > 0) {
                                controller.destinationIndex = controller.destinationIndex - 1
                                event.accepted = true
                            }
                        }
                    }

                    Accessible.role: Accessible.EditableText
                    Accessible.name: "Filter destinations"
                    Accessible.description: "Type to filter the list of destinations; number keys 1 through " + root.maxRows + " open a row directly when empty"
                }
            }

            ListView {
                id: list
                width: parent.width
                height: Math.min(root.maxRows, Math.max(1, count)) * root.rowHeight
                        + (count > 0 ? controller.pickerModel.sectionCount : 0) * root.sectionHeaderHeight
                model: controller.pickerModel
                clip: true
                spacing: 0
                currentIndex: 0
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 80

                Accessible.role: Accessible.List
                Accessible.name: "Destinations"

                section.property: "section"
                section.criteria: ViewSection.FullString
                section.delegate: Rectangle {
                    required property string section
                    width: ListView.view.width
                    height: root.sectionHeaderHeight
                    color: "transparent"
                    QQC.Label {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: section
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        opacity: 0.5
                    }
                }

                delegate: Rectangle {
                    required property int index
                    required property string name
                    required property string subtitle
                    required property string iconName
                    required property string shortcut
                    required property bool suggested
                    required property string kind
                    required property string colorName

                    width: ListView.view.width
                    height: root.rowHeight
                    radius: 10
                    color: ListView.isCurrentItem
                           ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.36)
                           : "transparent"

                    Accessible.role: Accessible.ListItem
                    Accessible.name: name + (shortcut.length > 0 && filterField.text.length === 0 ? ", shortcut " + shortcut : "")
                    Accessible.description: kind === "pwa" ? "App" : (kind === "action" ? "Action" : subtitle)

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
                        Rectangle {
                            // Container rows carry a color from the browser's
                            // own container definition; an unmapped color name
                            // (containerColor() returning an invalid QColor)
                            // falls back to a neutral dot rather than black,
                            // and this is never the only way a container row
                            // is told apart (its kind/subtitle text still says
                            // so too).
                            visible: kind === "container"
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: colorName.length > 0
                                   ? colorName
                                   : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.35)
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

            ColumnLayout {
                width: parent.width
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    QQC.CheckBox {
                        id: alwaysBox
                        checked: controller.alwaysForHost
                        onToggled: controller.alwaysForHost = checked
                        text: {
                            const key = controller.currentDestinationKey
                            const target = list.currentItem ? list.currentItem.name : ""
                            if (key.length === 0)
                                return "Always for this site"
                            return target.length > 0
                                   ? "Always for " + key + " in " + target
                                   : "Always for " + key
                        }
                        font.pixelSize: 12
                        // fillWidth lets the label elide instead of pushing
                        // the ladder group past the card edge when the
                        // destination key and target name are both long.
                        Layout.fillWidth: true
                    }
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
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Item {
                        id: copyHint
                        Layout.preferredWidth: copyRow.implicitWidth
                        Layout.preferredHeight: 16

                        Accessible.role: Accessible.Button
                        Accessible.name: "Copy link"
                        Accessible.description: "Copy the current link to the clipboard, shortcut Control C"

                        RowLayout {
                            id: copyRow
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Kirigami.Icon {
                                source: "edit-copy"
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                opacity: copyHover.hovered ? 0.7 : 0.4
                            }
                            QQC.Label {
                                text: "^C"
                                font.pixelSize: 10
                                font.family: "monospace"
                                opacity: copyHover.hovered ? 0.7 : 0.4
                            }
                        }
                        HoverHandler {
                            id: copyHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: controller.copyCurrent()
                        }
                        QQC.ToolTip.visible: copyHover.hovered
                        QQC.ToolTip.text: "Copy link"
                    }
                    QQC.Label {
                        text: "esc"
                        font.pixelSize: 10
                        font.family: "monospace"
                        opacity: 0.4
                    }
                    Item { Layout.fillWidth: true }
                    // Ladder keys: "," narrows the destination (adds a path
                    // segment), "." widens it (back toward the bare host) --
                    // see the Keys.onPressed handler above.
                    QQC.Label {
                        text: ". widen"
                        font.pixelSize: 10
                        font.family: "monospace"
                        opacity: 0.4
                    }
                    QQC.Label {
                        text: ", narrow"
                        font.pixelSize: 10
                        font.family: "monospace"
                        opacity: 0.4
                    }
                }
            }
        }
    }
}
