import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import "."

// FormSwitchDelegate replacement. Property parity: `text`,
// `description`, `checked`, `clickable` (default true = whole row
// toggles). Trailing switch mirrors FormCard's look.
QQC.ItemDelegate {
    id: root

    checkable: true
    property string description: ""

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    contentItem: RowLayout {
        spacing: Theme.largeSpacing

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

        QQC.Switch {
            checked: root.checked
            hoverEnabled: false
            focusPolicy: Qt.NoFocus
            Layout.alignment: Qt.AlignVCenter
            onToggled: {
                if (root.checked !== checked) root.toggle()
                root.toggled()
            }
        }
    }
}
