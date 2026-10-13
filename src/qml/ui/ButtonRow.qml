import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormButtonDelegate replacement. Property parity: `text`,
// `description`, `icon.name`, `arrowVisible`, signal `clicked()`.
QQC.ItemDelegate {
    id: root

    checkable: false
    property string description: ""
    property string iconName: ""
    property bool arrowVisible: false

    signal clicked()

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    contentItem: RowLayout {
        spacing: Theme.largeSpacing

        LaneIcon {
            visible: root.iconName !== ""
            iconName: root.iconName
            Layout.preferredWidth: Theme.iconSizeSmallMedium
            Layout.preferredHeight: Theme.iconSizeSmallMedium
        }

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true

            QQC.Label {
                text: root.text
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pointSize: 14
            }
            QQC.Label {
                visible: root.description !== ""
                text: root.description
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pointSize: 12
            }
        }

        Text {
            visible: root.arrowVisible
            text: "\u203a"
            opacity: 0.6
            font.pointSize: 18
        }
    }

    onClicked: root.clicked()
}
