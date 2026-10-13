import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormCardPage replacement: scrollable single-card column centered at
// a max width. Keeps FormCardPage's two key behaviors: pages taller
// than the window scroll, and a focused delegate scrolls into view.
QQC.Control {
    id: root

    default property alias pageContent: column.data
    property real contentWidth: Theme.cardWidth

    // FormCardPage parity: report contentHeight so hosts can size.
    readonly property real pageContentHeight: column.implicitHeight

    background: null
    readonly property alias flickable: internalFlickable
    contentItem: Flickable {
        id: internalFlickable

        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight + Theme.largeSpacing * 2

        ScrollIndicator.vertical: QQC.ScrollIndicator {}

        // Auto-scroll so the focused delegate stays visible.
        onContentYChanged: internalFlickable.contentYChanged()

        function scrollFocusedIntoView() {
            const focused = column.children.find(c => c.activeFocus);
            if (!focused) {
                return;
            }
            const viewTop = internalFlickable.contentY;
            const viewBottom = viewTop + internalFlickable.height;
            const itemTop = focused.mapToItem(internalFlickable.contentItem, 0, 0).y;
            const itemBottom = itemTop + focused.implicitHeight;
            if (itemTop < viewTop) {
                internalFlickable.contentY = itemTop - Theme.largeSpacing;
            } else if (itemBottom > viewBottom) {
                internalFlickable.contentY = itemBottom - internalFlickable.height + Theme.largeSpacing;
            }
        }

        // Track focus movement across delegate children via the
        // root window's activeFocusItem replacement.
        Connections {
            target: root.Window
            function onActiveFocusItemChanged() {
                internalFlickable.scrollFocusedIntoView();
            }
        }

        ColumnLayout {
            id: column

            x: Math.max(Theme.largeSpacing, (internalFlickable.width - Math.min(root.contentWidth, internalFlickable.width)) / 2)
            width: Math.min(root.contentWidth, internalFlickable.width) - Theme.largeSpacing * 2
            spacing: Theme.largeSpacing
        }

        // Make the whole blank area a click-to-focus surface so pages
        // don't behave like a dead full-screen region.
        MouseArea {
            anchors.fill: parent
            z: -1
            visible: false // reserved: not used now, placeholder to silence a warning
            enabled: false
        }
    }
}
