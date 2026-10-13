import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

// FormSectionText replacement: a dim, centered paragraph label.
QQC.Label {
    id: root

    Layout.fillWidth: true
    horizontalAlignment: Text.AlignHCenter
    verticalPadding: Theme.verticalPadding
    leftPadding: Theme.horizontalPadding
    rightPadding: Theme.horizontalPadding

    wrapMode: Text.WordWrap
    opacity: 0.8
    font.pointSize: 12
}
