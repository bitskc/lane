import QtQuick
import QtQuick.Layouts

// FormDelegateSeparator replacement: a thin inset line between
// delegates inside a FormCard. Matches FormCard's inset (left margin
// aligned with text, not the card edge).
Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.leftMargin: root.horizontalInset
    Layout.rightMargin: 0
    implicitHeight: 1

    property real horizontalInset: Theme.horizontalPadding

    color: Qt.rgba(0, 0, 0, 0.08)
}
