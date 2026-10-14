import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormCardPage replacement: scrollable single-card column centered at
// a max width. Keeps FormCardPage's two key behaviors: pages taller
// than the window scroll, and a focused delegate scrolls into view.
QQC.Control {
    id: root

    // Kirigami.Page parity: pages declare a title for the sidebar/header.
    property string title
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

        QQC.ScrollIndicator.vertical: QQC.ScrollIndicator {}

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

            // One width, centered: capped at contentWidth, never wider
            // than the viewport minus a gutter on each side.
            readonly property real usable: Math.min(root.contentWidth, internalFlickable.width - Theme.largeSpacing * 2)
            width: usable
            x: (internalFlickable.width - usable) / 2
            y: Theme.largeSpacing
            spacing: Theme.largeSpacing
        }
    }
}
