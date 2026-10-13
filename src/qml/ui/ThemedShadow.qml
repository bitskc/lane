import QtQuick
import QtQuick.Effects
import "."

// MultiEffect shadow wrapper replacing Kirigami.ShadowedRectangle's
// shadow. Defaults reproduce the picker card's exact shadow params:
// size 32, yOffset 10, alpha 0.42.
Rectangle {
    id: root

    color: Qt.rgba(0, 0, 0, 0)

    property alias sourceItem: sourceRect
    property color fillColor: root.palette.window
    property real radius: Theme.cornerRadius

    property color shadowColor: "#000000"
    property real shadowSize: 32
    property real shadowYOffset: 10
    property real shadowAlpha: 0.42

    Item {
        anchors.centerIn: parent
        width: parent.width
        height: parent.height

        Rectangle {
            id: sourceRect
            anchors.fill: parent
            radius: root.radius
            color: root.fillColor
        }

        // Margin needed so the blur isn't clipped by the source.
        MultiEffect {
            source: sourceRect
            anchors.fill: parent
            shadowEnabled: true
            shadowColor: Qt.rgba(root.shadowColor.r, root.shadowColor.g, root.shadowColor.b, root.shadowAlpha)
            shadowBlur: root.shadowSize / 2
            shadowVerticalOffset: root.shadowYOffset
            paddingRect: Qt.rect(-root.shadowSize, -root.shadowSize, root.shadowSize, root.shadowSize + 2 * root.shadowYOffset)
        }
    }
}
