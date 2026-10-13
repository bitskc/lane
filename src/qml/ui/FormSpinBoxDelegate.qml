import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormSpinBoxDelegate replacement. Property parity: `label`,
// `description`, `from`, `to`, `value`, `stepSize`, `editable`,
// `textFromValue`/`valueFromText`/`validator`, signal
// `valueModified(value)`.
QQC.ItemDelegate {
    id: root

    checkable: false
    property alias label: root.text
    property string description: ""
    property var status: null
    property string statusMessage: ""

    property alias from: spinBox.from
    property alias to: spinBox.to
    property alias value: spinBox.value
    property alias stepSize: spinBox.stepSize
    property alias editable: spinBox.editable
    property alias textFromValue: spinBox.textFromValue
    property alias valueFromText: spinBox.valueFromText
    property alias validator: spinBox.validator
    property alias spinBox: spinBox

    signal valueModified(int value)

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    contentItem: RowLayout {
        spacing: Theme.largeSpacing

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true

            QQC.Label {
                text: root.label
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

        QQC.SpinBox {
            id: spinBox
            Layout.preferredWidth: 160
            onValueModified: root.valueModified(value)
        }
    }
}
