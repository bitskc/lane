import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Browsers & apps"

    property int rowHeight: 48

    Timer {
        id: renameTimer
        property string targetId
        property string newName
        interval: 1
        onTriggered: controller.renameTarget(targetId, newName)
    }

    ListModel { id: browserModel }
    ListModel { id: pwaModel }
    ListModel { id: customModel }
    ListModel { id: privateModel }

    Component {
        id: browserDelegate
        QQC.ItemDelegate {
            id: listItem
            width: browserList.width
            height: page.rowHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.ListItemDragHandle {
                    listItem: listItem
                    listView: browserList
                    onMoveRequested: (oldIndex, newIndex) => {
                        if (browserList.dragId === "")
                            browserList.dragId = browserModel.get(oldIndex).targetId
                        browserModel.move(oldIndex, newIndex, 1)
                    }
                    onDropped: (oldIndex, newIndex) => {
                        if (newIndex >= 0 && browserList.dragId !== "") {
                            controller.moveTarget(browserList.dragId, newIndex)
                            browserList.dragId = ""
                        }
                    }
                }
                Kirigami.Icon {
                    source: model.iconName
                    implicitWidth: 22
                    implicitHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.TextField {
                    text: model.name
                    placeholderText: model.discoveredName
                    Layout.fillWidth: true
                    background: Item {}
                    verticalAlignment: TextInput.AlignVCenter
                    onEditingFinished: {
                        var id = model.targetId
                        var nm = text.trim()
                        if (nm !== model.name) {
                            renameTimer.targetId = id
                            renameTimer.newName = nm
                            renameTimer.start()
                        }
                    }
                    Keys.onEscapePressed: {
                        text = model.name
                        focus = false
                    }
                }
                QQC.Switch {
                    checked: !model.hidden
                    onToggled: controller.hideTarget(model.targetId, !checked)
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.Button {
                    text: "Default"
                    flat: true
                    onClicked: controller.defaultTargetId = model.targetId
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    Component {
        id: pwaDelegate
        QQC.ItemDelegate {
            id: listItem
            width: pwaList.width
            height: page.rowHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.ListItemDragHandle {
                    listItem: listItem
                    listView: pwaList
                    onMoveRequested: (oldIndex, newIndex) => {
                        if (pwaList.dragId === "")
                            pwaList.dragId = pwaModel.get(oldIndex).targetId
                        pwaModel.move(oldIndex, newIndex, 1)
                    }
                    onDropped: (oldIndex, newIndex) => {
                        if (newIndex >= 0 && pwaList.dragId !== "") {
                            controller.moveTarget(pwaList.dragId, newIndex)
                            pwaList.dragId = ""
                        }
                    }
                }
                Kirigami.Icon {
                    source: model.iconName
                    implicitWidth: 22
                    implicitHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.TextField {
                    text: model.name
                    placeholderText: model.discoveredName
                    Layout.fillWidth: true
                    background: Item {}
                    verticalAlignment: TextInput.AlignVCenter
                    onEditingFinished: {
                        var id = model.targetId
                        var nm = text.trim()
                        if (nm !== model.name) {
                            renameTimer.targetId = id
                            renameTimer.newName = nm
                            renameTimer.start()
                        }
                    }
                    Keys.onEscapePressed: {
                        text = model.name
                        focus = false
                    }
                }
                QQC.Switch {
                    checked: !model.hidden
                    onToggled: controller.hideTarget(model.targetId, !checked)
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    Component {
        id: customDelegate
        QQC.ItemDelegate {
            id: listItem
            width: customList.width
            height: page.rowHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.ListItemDragHandle {
                    listItem: listItem
                    listView: customList
                    onMoveRequested: (oldIndex, newIndex) => {
                        if (customList.dragId === "")
                            customList.dragId = customModel.get(oldIndex).targetId
                        customModel.move(oldIndex, newIndex, 1)
                    }
                    onDropped: (oldIndex, newIndex) => {
                        if (newIndex >= 0 && customList.dragId !== "") {
                            controller.moveTarget(customList.dragId, newIndex)
                            customList.dragId = ""
                        }
                    }
                }
                Kirigami.Icon {
                    source: model.iconName
                    implicitWidth: 22
                    implicitHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.TextField {
                    text: model.name
                    placeholderText: model.discoveredName
                    Layout.fillWidth: true
                    background: Item {}
                    verticalAlignment: TextInput.AlignVCenter
                    onEditingFinished: {
                        var id = model.targetId
                        var nm = text.trim()
                        if (nm !== model.name) {
                            renameTimer.targetId = id
                            renameTimer.newName = nm
                            renameTimer.start()
                        }
                    }
                    Keys.onEscapePressed: {
                        text = model.name
                        focus = false
                    }
                }
                QQC.Button {
                    text: "Remove"
                    flat: true
                    onClicked: controller.removeCustomTarget(model.targetId)
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    Component {
        id: privateDelegate
        QQC.ItemDelegate {
            width: privateList.width
            height: page.rowHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Item { width: Kirigami.Units.iconSizes.smallMedium }
                Kirigami.Icon {
                    source: model.iconName
                    implicitWidth: 22
                    implicitHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.TextField {
                    text: model.name
                    placeholderText: model.discoveredName
                    Layout.fillWidth: true
                    background: Item {}
                    verticalAlignment: TextInput.AlignVCenter
                    onEditingFinished: {
                        var id = model.targetId
                        var nm = text.trim()
                        if (nm !== model.name) {
                            renameTimer.targetId = id
                            renameTimer.newName = nm
                            renameTimer.start()
                        }
                    }
                    Keys.onEscapePressed: {
                        text = model.name
                        focus = false
                    }
                }
                QQC.Switch {
                    checked: !model.hidden
                    onToggled: controller.hideTarget(model.targetId, !checked)
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    function syncModels() {
        browserModel.clear()
        var browsers = controller.targetModel.targetsByKind("browser")
        for (var i = 0; i < browsers.length; i++) {
            if (!browsers[i].incognito)
                browserModel.append(browsers[i])
        }
        pwaModel.clear()
        var pwas = controller.targetModel.targetsByKind("pwa")
        for (var i = 0; i < pwas.length; i++)
            pwaModel.append(pwas[i])
        customModel.clear()
        var customs = controller.targetModel.targetsByKind("app")
        for (var i = 0; i < customs.length; i++)
            customModel.append(customs[i])
        privateModel.clear()
        var privates = controller.targetModel.incognitoTargets()
        for (var i = 0; i < privates.length; i++)
            privateModel.append(privates[i])
    }

    Component.onCompleted: syncModels()
    Connections {
        target: controller
        function onSettingsChanged() { syncModels() }
    }

    FormCard.FormHeader {
        title: "Default"
    }
    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            text: "Fallback target"
            description: "Used when Tern does not ask and no rule matches"
            model: controller.targetNames
            currentIndex: Math.max(0, controller.targetIds.indexOf(controller.defaultTargetId))
            onActivated: controller.defaultTargetId = controller.targetIds[currentIndex]
        }
    }

    FormCard.FormHeader {
        title: "Browsers"
    }
    QQC.Label {
        text: "Drag to reorder. Click a name to rename."
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
    }
    FormCard.FormCard {
        ListView {
            id: browserList
            model: browserModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: count * page.rowHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: Loader {
                width: browserList.width
                sourceComponent: browserDelegate
            }
        }
    }

    FormCard.FormHeader {
        title: "Installed web apps"
    }
    FormCard.FormCard {
        ListView {
            id: pwaList
            model: pwaModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: count * page.rowHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: Loader {
                width: pwaList.width
                sourceComponent: pwaDelegate
            }
        }
    }

    FormCard.FormHeader {
        title: "Private windows"
    }
    FormCard.FormCard {
        ListView {
            id: privateList
            model: privateModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: count * page.rowHeight
            delegate: Loader {
                width: privateList.width
                sourceComponent: privateDelegate
            }
        }
    }

    FormCard.FormHeader {
        title: "Custom apps"
    }
    FormCard.FormCard {
        ListView {
            id: customList
            model: customModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: count * page.rowHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: Loader {
                width: customList.width
                sourceComponent: customDelegate
            }
        }
        FormCard.FormTextFieldDelegate {
            id: customName
            label: "Name"
            placeholderText: "Work Slack"
        }
        FormCard.FormTextFieldDelegate {
            id: customCommand
            label: "Command"
            placeholderText: "firefox -P work $url"
        }
        FormCard.FormButtonDelegate {
            text: "Add custom app"
            icon.name: "list-add"
            onClicked: {
                controller.addCustomTarget(customName.text, customCommand.text)
                customName.text = ""
                customCommand.text = ""
            }
        }
    }
}
