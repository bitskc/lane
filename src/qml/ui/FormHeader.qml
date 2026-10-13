import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC

// FormHeader replacement: a section label above a card. Slight left
// padding matches FormHeader's indented bold-text shape.
QQC.Label {
    id: root

    // Kirigami FormHeader parity: callers pass `title`.
    property alias title: root.text

    topPadding: Theme.smallSpacing * 2
    leftPadding: Theme.largeSpacing
    bottomPadding: Theme.smallSpacing
    Layout.leftMargin: Theme.smallSpacing

    font.pointSize: 14
    font.weight: Font.DemiBold
    color: root.palette.windowText
}
