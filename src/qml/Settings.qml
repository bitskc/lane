import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import app.lane.ui as LaneUi

QQC.ApplicationWindow {
    id: root
    title: "Lane"
    minimumWidth: 880
    minimumHeight: 560
    width: 920
    height: 660
    visible: true
    color: root.palette.window

    function selectPage(index) {
        nav.currentIndex = index
    }
    Shortcut { sequence: StandardKey.Quit; onActivated: controller.requestQuit() }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Qt.rgba(root.palette.alternateBase.r, root.palette.alternateBase.g, root.palette.alternateBase.b, 0.55)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 18

                RowLayout {
                    spacing: 10
                    LaneUi.LaneIcon {
                        iconName: "app.lane.Lane"
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                    }
                    Column {
                        QQC.Label {
                            text: "Lane"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        QQC.Label {
                            text: "v" + controller.appVersion
                            font.pixelSize: 11
                            opacity: 0.5
                        }
                    }
                }

                ListView {
                    id: nav
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0
                    model: ListModel {
                        ListElement { label: "Overview"; iconName: "help-about" }
                        ListElement { label: "Browsers & apps"; iconName: "internet-web-browser" }
                        ListElement { label: "Rules"; iconName: "view-filter" }
                        ListElement { label: "Preferences"; iconName: "settings-configure" }
                    }
                    delegate: Rectangle {
                        id: navItem
                        required property int index
                        required property string label
                        required property string iconName
                        width: ListView.view.width
                        height: 36
                        radius: 8
                        color: ListView.isCurrentItem
                               ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.22)
                               : "transparent"

                        Accessible.role: Accessible.ListItem
                        Accessible.name: label
                        Accessible.selected: ListView.isCurrentItem

                        MouseArea {
                            anchors.fill: parent
                            onClicked: nav.currentIndex = index
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8
                            LaneUi.LaneIcon {
                                iconName: navItem.iconName
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            QQC.Label {
                                text: label
                                font.pixelSize: 13
                                font.weight: navItem.ListView.isCurrentItem ? Font.DemiBold : Font.Normal
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            color: Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.08)
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: nav.currentIndex
            OverviewPage {}
            TargetsPage {}
            RulesPage {}
            PreferencesPage {}
        }
    }
}
