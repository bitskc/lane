import QtQuick

// Rows must have unique targetId roles. The owner captures that id in
// onDragStarted; model indices and pointer coordinates never depend on
// the animated positions of displaced delegates.
Item {
    id: root

    implicitWidth: 24
    implicitHeight: 32

    property Flickable flickable
    required property Item listItem
    required property ListView ownerList
    property string dragId: ""
    property bool dragActive: false
    property int targetIndex: -1

    signal dragStarted()
    signal dragFinished(string id, int index)

    function updateDestination() {
        const point = ownerList.contentItem.mapFromItem(null, mouse.sceneX, mouse.sceneY)
        const rowHeight = listItem.height + ownerList.spacing
        const destination = Math.max(0, Math.min(ownerList.count - 1,
                                                Math.round((point.y - mouse.pointerOffset) / rowHeight)))
        if (destination !== mouse.currentIndex) {
            ownerList.model.move(mouse.currentIndex, destination, 1)
            mouse.currentIndex = destination
        }
        targetIndex = destination
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        preventStealing: true

        property real sceneX: 0
        property real sceneY: 0
        property real pointerOffset: 0
        property int initialIndex: -1
        property int currentIndex: -1

        onPressed: function (event) {
            root.dragStarted()
            root.dragId = root.ownerList.dragId
            for (let i = 0; i < root.ownerList.count; ++i) {
                if (root.ownerList.model.get(i).targetId === root.dragId) {
                    initialIndex = i
                    currentIndex = i
                    break
                }
            }
            root.targetIndex = currentIndex
            const scene = mapToItem(null, event.x, event.y)
            sceneX = scene.x
            sceneY = scene.y
            const point = root.ownerList.contentItem.mapFromItem(null, sceneX, sceneY)
            pointerOffset = point.y - initialIndex * (root.listItem.height + root.ownerList.spacing)
            root.dragActive = currentIndex >= 0
        }

        onPositionChanged: function (event) {
            if (!root.dragActive) return
            const scene = mapToItem(null, event.x, event.y)
            sceneX = scene.x
            sceneY = scene.y
            root.updateDestination()
        }

        onReleased: {
            root.dragActive = false
            if (root.targetIndex !== initialIndex && root.dragId !== "")
                root.dragFinished(root.dragId, root.targetIndex)
            root.ownerList.dragId = ""
            root.dragId = ""
            root.targetIndex = -1
            initialIndex = -1
            currentIndex = -1
        }

        onCanceled: {
            root.dragActive = false
            if (currentIndex >= 0 && currentIndex !== initialIndex)
                root.ownerList.model.move(currentIndex, initialIndex, 1)
            root.ownerList.dragId = ""
            root.dragId = ""
            root.targetIndex = -1
            initialIndex = -1
            currentIndex = -1
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.dragActive && root.flickable !== null
        onTriggered: {
            const view = root.flickable
            const point = view.mapFromItem(null, mouse.sceneX, mouse.sceneY)
            const maximum = Math.max(0, view.contentHeight - view.height)
            const step = point.y < 40 ? -6 : point.y > view.height - 40 ? 6 : 0
            const next = Math.max(0, Math.min(maximum, view.contentY + step))
            if (next !== view.contentY) {
                view.contentY = next
                root.updateDestination()
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2
        opacity: root.enabled ? 0.5 : 0.25
        Repeater {
            model: 3
            Rectangle {
                width: 14
                height: 2
                radius: 1
                color: root.listItem.palette.placeholderText
            }
        }
    }
}
