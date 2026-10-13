import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormRadioDelegate replacement. Property parity: `text`,
// `description`, `checked`, `autoExclusive`.
QQC.RadioButton {
    id: root

    property string description: ""

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    indicator: RadioButton {
        checked: root.checked
        enabled: false
        opacity: 1.0
    }
    contentItem: ColumnLayout {
        spacing: 2
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
}
