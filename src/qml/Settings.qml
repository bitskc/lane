import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC.ApplicationWindow {
    id: root
    title: "Tern"
    minimumWidth: 880
    minimumHeight: 560
    width: 920
    height: 660
    visible: true
    color: Kirigami.Theme.backgroundColor

    function selectPage(index) {
        nav.currentIndex = index
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Qt.rgba(Kirigami.Theme.alternateBackgroundColor.r, Kirigami.Theme.alternateBackgroundColor.g, Kirigami.Theme.alternateBackgroundColor.b, 0.55)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 18

                RowLayout {
                    spacing: 10
                    Kirigami.Icon {
                        source: "app.tern.Tern"
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                    }
                    Column {
                        QQC.Label {
                            text: "Tern"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        QQC.Label {
                            text: "Link router"
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
                        required property int index
                        required property string label
                        required property string iconName
                        width: ListView.view.width
                        height: 36
                        radius: 8
                        color: ListView.isCurrentItem
                               ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.22)
                               : "transparent"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: nav.currentIndex = index
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8
                            Kirigami.Icon {
                                source: iconName
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            QQC.Label {
                                text: label
                                font.pixelSize: 13
                                font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal
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
            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
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
