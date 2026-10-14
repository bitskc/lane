import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormTextFieldDelegate replacement. Property parity: `label`,
// `description`, `placeholderText`, `maximumLength`, `validator`,
// `status`/`statusMessage`, trailing `trailing`. Signals `accepted()`,
// `editingFinished()`, `textEdited(text)`. Exposes the TextField as
// `textField`.
QQC.Control {
    id: root

    property string label: ""
    property alias text: textField.text
    property string description: ""
    property var status: null
    property string statusMessage: ""

    property alias placeholderText: textField.placeholderText
    property alias maximumLength: textField.maximumLength
    property alias validator: textField.validator
    property alias selectByMouse: textField.selectByMouse
    property alias readOnly: textField.readOnly
    property alias echoMode: textField.echoMode
    property alias inputMethodHints: textField.inputMethodHints
    property alias acceptableInput: textField.acceptableInput
    property alias cursorPosition: textField.cursorPosition
    property alias fieldActiveFocus: textField.activeFocus
    property alias selectedText: textField.selectedText
    property alias selectionStart: textField.selectionStart
    property alias selectionEnd: textField.selectionEnd
    property alias textField: textField

    signal accepted()
    signal editingFinished()
    signal textEdited(string text)

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

    focusPolicy: Qt.NoFocus

    contentItem: RowLayout {
        spacing: Theme.largeSpacing

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            TapHandler { onTapped: textField.forceActiveFocus() }

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
            QQC.Label {
                visible: root.statusMessage !== ""
                text: root.statusMessage
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root.status === 2 ? "#db241c" : root.palette.windowText
                font.pointSize: 12
            }
        }

        QQC.TextField {
            id: textField
            Layout.preferredWidth: 260
            selectByMouse: true
            Accessible.name: root.label

            onAccepted: root.accepted()
            onEditingFinished: root.editingFinished()
            onTextEdited: root.textEdited(text)
        }
    }
}
