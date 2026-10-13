import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormComboBoxDelegate replacement. Property parity: `text`,
// `description`, `model`, `currentIndex`, `currentValue`, `editable`,
// `status`/`statusMessage` (accepted but not rendered beyond a hint),
// signal `activated(index)`.
QQC.ItemDelegate {
    id: root

    checkable: false
    property string description: ""
    property var status: null
    property string statusMessage: ""

    property alias model: comboBox.model
    property alias currentIndex: comboBox.currentIndex
    property alias currentText: comboBox.currentText
    property alias currentValue: comboBox.currentValue
    property alias editText: comboBox.editText
    property alias editable: comboBox.editable
    property alias textRole: comboBox.textRole
    property alias valueRole: comboBox.valueRole
    property alias count: comboBox.count
    property alias comboBox: comboBox

    signal activated(int index)

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
            QQC.Label {
                visible: root.statusMessage !== ""
                text: root.statusMessage
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root.status === QQCEnums.Information ? root.palette.link
                    : root.status === QQCEnums.Error ? "#db241c" : root.palette.windowText
                font.pointSize: 12
            }
        }

        QQC.ComboBox {
            id: comboBox
            Layout.preferredWidth: 260
            onActivated: (index) => root.activated(index)
        }
    }
}
