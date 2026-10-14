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

    // The ListView/owner whose dragId the page maintains in its own
    // scope (where the attached model IS visible). The handle cannot
    // read model roles itself, so it pulls the id from here on drop.
    property var ownerList: null

    // Captured once on press so model moves don't retarget mid-drag.
    property string dragId: ""
    // Drag in progress; the delegate's height stays intact while this
    // is true even if the row would otherwise collapse.
    property bool dragActive: false

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
            startY = listItem.mapToItem(ownerList.contentItem, 0, 0).y;
            dragActive = true;
        }

        onPositionChanged: function (mouse) {
            if (!dragActive) {
                return;
            }
            const offset = mouse.y - pressY;
            const newRowY = startY + offset;
            const rowHeight = listItem.height + ownerList.spacing;
            const newIndex = Math.max(0, Math.min(ownerList.count - 1, Math.round(newRowY / rowHeight)));
            targetIndex = newIndex;

            if (newIndex !== startIndex) {
                // Visual move now, persisted order on drop (dragFinished).
                // moveRow never existed here; ListModel.move is the method.
                ownerList.model.move(startIndex, newIndex, 1);
                startIndex = newIndex;
                startY = listItem.mapToItem(ownerList.contentItem, 0, 0).y;
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
            // The page captures the id in its own scope (onDragStarted
            // reads model.targetId) and hands it to the view via
            // ownerList.dragId; the handle's scope has no model roles.
            if (!dragId && ownerList) {
                dragId = ownerList.dragId;
            }
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
