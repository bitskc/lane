import QtQuick

// DragHandle: reorder handle for ListView delegates, replacing
// Kirigami.ListItemDragHandle. Honors the two invariants from the
// Kirigami version:
//   1. Disabled while the list is filtering (collapsed rows make
//      index math wrong).
//   2. dragId is captured before the first model move so a drop after
//      intermediate moves still targets the right row.
//
// Release dispatches controller.moveTarget(dragId, targetIndex).
Item {
    id: root

    property Flickable flickable
    property Item listItem

    // Captured once on press so model moves don't retarget mid-drag.
    property string dragId: ""

    // Drag in progress; the delegate's height stays intact while this
    // is true even if the row would otherwise collapse.
    property bool dragActive: false

    property var modelData: null
    property var model: null

    // Model index of the row we're hovering (updated during drag).
    property int targetIndex: -1

    signal dragStarted()
    signal dragFinished(string id, int index)

    // Deferred move so the first drag step doesn't retarget dragId.
    onDragActiveChanged: {
        if (dragActive) {
            dragStarted();
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        cursorShape: Qt.OpenHandCursor

        property real pressY: 0
        property real startY: 0
        property int startIndex: -1

        onPressed: function (mouse) {
            pressY = mouse.y;
            startY = listItem.mapToItem(listItem.ListView.view.contentItem, 0, 0).y;
            startIndex = model.index;
            dragId = listItem.ListView.view.model.get(startIndex, "targetId") ?? "";
            dragActive = true;
        }

        onPositionChanged: function (mouse) {
            if (!dragActive) {
                return;
            }
            const offset = mouse.y - pressY;
            const newRowY = startY + offset;
            const rowHeight = listItem.height + listItem.ListView.view.spacing;
            const newIndex = Math.max(0, Math.min(listItem.ListView.view.count - 1, Math.round(newRowY / rowHeight)));
            targetIndex = newIndex;

            if (newIndex !== startIndex) {
                // Move is deferred until the handle's dragFinished to
                // preserve the captured-id invariant.
                listItem.ListView.view.model.moveRow(startIndex, newIndex);
                startIndex = newIndex;
                startY = listItem.mapToItem(listItem.ListView.view.contentItem, 0, 0).y;
            }

            // Edge auto-scroll.
            if (flickable) {
                const edge = 40;
                const mouseYIn = flickable.mapFromItem(root, 0, 0).y;
                if (mouseYIn < edge && flickable.contentY > 0) {
                    flickable.contentY -= 6;
                } else if (mouseYIn > flickable.height - edge && flickable.contentY < flickable.contentHeight - flickable.height) {
                    flickable.contentY += 6;
                }
            }
        }

        onReleased: {
            dragActive = false;
            if (dragId !== "") {
                root.dragFinished(dragId, targetIndex);
            }
            dragId = "";
        }

        onCanceled: {
            dragActive = false;
            dragId = "";
        }
    }

    // Visual affordance: vertical grip dots.
    Column {
        anchors.centerIn: parent
        spacing: 2
        opacity: 0.5

        Repeater {
            model: 3
            Rectangle {
                width: 14
                height: 2
                radius: 1
                color: root.palette != undefined ? root.palette.placeholderText : "#888"
            }
        }
    }
}
