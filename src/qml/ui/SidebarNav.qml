import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// Keyboard-reachable sidebar navigation. The old Settings sidebar was a
// ListView of plain Rectangles with MouseAreas: the view never took
// focus, no key moved the selection, and the delegates exposed no
// focusable control to assistive tech. This control keeps that look
// (36px rows, 8px radius, icon + label, translucent highlight) on top of
// focusable ItemDelegates so Tab reaches it, Up/Down move the current
// item, Enter/Space and click activate, and every entry reports
// Accessible.role/.name/.selected. It deliberately knows nothing about
// `controller`: bind a model of {label, iconName} and read currentIndex
// (or the activated signal) from the caller.
ListView {
    id: nav

    // Raised when an entry is activated by click, Enter or Space. The
    // index already moved for arrow keys (ListView key navigation), so
    // callers that only mirror selection can bind currentIndex alone.
    signal activated(int index)

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    activeFocusOnTab: true
    keyNavigationEnabled: true
    highlightFollowsCurrentItem: true
    currentIndex: 0

    function activate(index) {
        if (index < 0 || index >= nav.count) {
            return
        }
        nav.currentIndex = index
        nav.activated(index)
    }

    Keys.onReturnPressed: nav.activate(nav.currentIndex)
    Keys.onEnterPressed: nav.activate(nav.currentIndex)
    Keys.onSpacePressed: nav.activate(nav.currentIndex)

    delegate: QQC.ItemDelegate {
        id: navItem
        padding: 0
        focusPolicy: Qt.StrongFocus
        required property int index
        required property string label
        required property string iconName

        width: nav.width
        height: 36
        highlighted: navItem.ListView.isCurrentItem
        onClicked: nav.activate(navItem.index)

        Accessible.role: Accessible.ListItem
        Accessible.name: navItem.label
        Accessible.selected: navItem.ListView.isCurrentItem

        background: Rectangle {
            radius: 8
            color: navItem.highlighted
                   ? Qt.rgba(navItem.palette.highlight.r, navItem.palette.highlight.g,
                             navItem.palette.highlight.b, 0.22)
                   : "transparent"
            // Visible focus indication: a ring on the entry that holds
            // keyboard focus (delegate focus) or on the current entry
            // while the view itself has focus (arrow-key navigation).
            border.width: navItem.visualFocus || (navItem.highlighted && nav.activeFocus) ? 1 : 0
            border.color: navItem.palette.highlight
        }

        contentItem: RowLayout {
            spacing: 8
            LaneIcon {
                iconName: navItem.iconName
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignVCenter
            }
            QQC.Label {
                text: navItem.label
                font.pixelSize: 13
                font.weight: navItem.highlighted ? Font.DemiBold : Font.Normal
                Layout.fillWidth: true
                Layout.rightMargin: 8
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
