import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// AbstractFormDelegate replacement: base for custom rows inside a
// FormCard. Provides the row padding and a transparent hover
// background; subclass and override `contentItem`.
QQC.ItemDelegate {
    id: root

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    background: Rectangle {
        color: root.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
        radius: 4
    }
}
