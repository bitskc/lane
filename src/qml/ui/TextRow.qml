import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormTextDelegate replacement: read-only row. Property parity:
// `text`, `description`. Not clickable by default; enable with
// `enabled: true` and use `clicked()`.
QQC.ItemDelegate {
    id: root

    checkable: false
    hoverEnabled: false
    enabled: false

    property string description: ""

    signal clicked()

    Layout.fillWidth: true
    horizontalPadding: Theme.horizontalPadding
    verticalPadding: Theme.verticalPadding

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

    TapHandler {
        enabled: root.enabled
        onTapped: root.clicked()
    }
}
