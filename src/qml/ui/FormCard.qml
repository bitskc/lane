import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormCard replacement: a rounded grouped-card container that stacks
// its delegates vertically with no spacing between rows. Call sites
// can pass delegates as direct children; default property appends to
// the internal column like FormCard does.
QQC.Control {
    id: root

    default property alias contentData: column.data

    readonly property real contentHeight: column.implicitHeight

    horizontalPadding: 0
    verticalPadding: 0
    topPadding: 0
    bottomPadding: 0

    background: Rectangle {
        radius: Theme.cornerRadius
        color: root.palette.window
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.08)
    }

    contentItem: ColumnLayout {
        id: column
        spacing: 0
    }
}
