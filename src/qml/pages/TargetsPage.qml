import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import app.lane.ui as LaneUi

LaneUi.ScrollableFormPage {
    id: page
    title: "Browsers & apps"

    property int rowHeight: 48
    property string customCommandError: ""
    property string searchText: ""
    property bool privateExpanded: false
    property bool customsExpanded: true

    function matchesSearch(name, discoveredName) {
        if (searchText.trim().length === 0) {
            return true
        }
        const q = searchText.trim().toLowerCase()
        return (name || "").toLowerCase().indexOf(q) !== -1
            || (discoveredName || "").toLowerCase().indexOf(q) !== -1
    }

    function matchCount(listModel) {
        var n = 0
        for (var i = 0; i < listModel.count; i++) {
            var e = listModel.get(i)
            if (matchesSearch(e.name, e.discoveredName)) {
                n++
            }
        }
        return n
    }

    function totalMatchCount() {
        return matchCount(allModel) + matchCount(privateModel)
    }

    Timer {
        id: renameTimer
        property string targetId
        property string newName
        interval: 1
        onTriggered: controller.renameTarget(targetId, newName)
    }

    ListModel { id: allModel }
    ListModel { id: privateModel }

    Component {
        id: targetDelegate
        Item {
            id: wrapper
            width: allList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            QQC.ItemDelegate {
                id: listItem
                width: wrapper.width
                height: wrapper.height
                horizontalPadding: LaneUi.Theme.horizontalPadding
                verticalPadding: 0
                contentItem: RowLayout {
                    spacing: LaneUi.Theme.largeSpacing
                    LaneUi.DragHandle {
                        // Filtered-out rows stay in allModel at their
                        // original index (only their visual height collapses
                        // to 0), so a drag computed against the full model
                        // while a search filter is active can persist an
                        // order different from what was visually dragged.
                        // Disabling the handle while filtered avoids that
                        // ambiguity outright; clearing search restores it.
                        enabled: page.searchText.trim().length === 0
                        listItem: listItem
                        flickable: page.flickable
                        ownerList: allList
                        onDragStarted: {
                            if (allList.dragId === "") {
                                allList.dragId = model.targetId
                            }
                        }
                        onDragFinished: (id, newIndex) => {
                            if (newIndex >= 0 && allList.dragId !== "") {
                                controller.moveTarget(allList.dragId, newIndex)
                            }
                            allList.dragId = ""
                        }
                    }
                    LaneUi.LaneIcon {
                        iconName: model.iconName
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.TextField {
                        id: renameField
                        text: model.name
                        placeholderText: model.discoveredName
                        font.pointSize: 13
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
                        HoverHandler { id: renameHover }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: renameHover.hovered && !renameField.activeFocus
                            color: Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.25)
                        }
                    }
                    // Kind badge keeps a flattened list legible: browsers,
                    // containers, web apps and custom apps share one column
                    // now, so the row still says what it is.
                    QQC.Label {
                        text: model.kind
                        font.pointSize: 12
                        opacity: 0.6
                        horizontalAlignment: Text.AlignLeft
                        Layout.preferredWidth: 72
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.Switch {
                        checked: !model.hidden
                        onToggled: controller.hideTarget(model.targetId, !checked)
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.Button {
                        visible: model.kind !== "app"
                        text: "Default"
                        flat: model.targetId !== controller.defaultTargetId
                        highlighted: model.targetId === controller.defaultTargetId
                        Accessible.name: model.targetId === controller.defaultTargetId
                            ? "Default (currently selected)" : "Set as default"
                        QQC.ToolTip.visible: hovered
                        QQC.ToolTip.text: model.targetId === controller.defaultTargetId
                            ? "This is the default target" : "Set as default target"
                        onClicked: controller.defaultTargetId = model.targetId
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.Button {
                        visible: model.kind === "app"
                        text: "Remove"
                        flat: true
                        onClicked: controller.removeCustomTarget(model.targetId)
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }
        }
    }

    Component {
        id: privateDelegate
        QQC.ItemDelegate {
            width: privateList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            horizontalPadding: LaneUi.Theme.horizontalPadding
            verticalPadding: 0
            contentItem: RowLayout {
                spacing: LaneUi.Theme.largeSpacing
                Item { width: 22 }
                LaneUi.LaneIcon {
                    iconName: model.iconName
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                }
                QQC.TextField {
                    id: renameField
                    text: model.name
                    placeholderText: model.discoveredName
                    font.pointSize: 13
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
                    HoverHandler { id: renameHover }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        visible: renameHover.hovered && !renameField.activeFocus
                        color: Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.25)
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
        allModel.clear()
        var all = controller.targetModel.orderableTargets()
        for (var i = 0; i < all.length; i++) {
            allModel.append(all[i])
        }
        privateModel.clear()
        var privates = controller.targetModel.incognitoTargets()
        for (var j = 0; j < privates.length; j++) {
            privateModel.append(privates[j])
        }
    }

    Component.onCompleted: syncModels()
    Connections {
        target: controller
        // Qt.callLater defers the rebuild off the signal call stack: a
        // drag's onDropped triggers moveTarget() → settingsChanged()
        // synchronously, and clear()+rebuild inside that callback would
        // destroy the delegate that is still executing it and eat the
        // moveDisplaced settle animation.
        function onSettingsChanged() { Qt.callLater(syncModels) }
    }

    LaneUi.FormHeader {
        title: "Search"
    }
    LaneUi.FormCard {
        LaneUi.AbstractFormDelegate {
            background: Item {}
            contentItem: QQC.TextField {
                id: searchField
                placeholderText: "Filter browsers, containers, apps…"
                text: page.searchText
                onTextChanged: page.searchText = text
                Accessible.role: Accessible.EditableText
                Accessible.name: "Filter browsers and apps"
            }
        }
    }

    QQC.Label {
        visible: page.searchText.trim().length > 0 && page.totalMatchCount() === 0
        text: "No matches"
        font.pointSize: 12
        opacity: 0.8
        Layout.leftMargin: LaneUi.Theme.largeSpacing
        Layout.rightMargin: LaneUi.Theme.largeSpacing
        Layout.bottomMargin: LaneUi.Theme.smallSpacing

        Accessible.role: Accessible.StaticText
        Accessible.name: "No matches"
    }

    LaneUi.FormHeader {
        title: "Default"
    }
    LaneUi.FormCard {
        LaneUi.FormComboBoxDelegate {
            text: "Fallback target"
            description: "Used when Lane does not ask and no rule matches"
            model: controller.targetNames
            currentIndex: Math.max(0, controller.targetIds.indexOf(controller.defaultTargetId))
            onActivated: controller.defaultTargetId = controller.targetIds[currentIndex]
        }
    }

    LaneUi.FormHeader {
        title: "Destinations (" + page.matchCount(allModel) + ")"
    }
    QQC.Label {
        text: "This is the picker's row order. Drag to rearrange any destination past any other; click a name to rename."
        font.pointSize: 12
        opacity: 0.8
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.leftMargin: LaneUi.Theme.largeSpacing
        Layout.rightMargin: LaneUi.Theme.largeSpacing
        Layout.bottomMargin: LaneUi.Theme.smallSpacing
    }
    LaneUi.FormCard {
        visible: page.matchCount(allModel) > 0 || page.searchText.length > 0
        ListView {
            id: allList
            model: allModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            moveDisplaced: Transition {
                YAnimator { duration: LaneUi.Theme.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: targetDelegate
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: LaneUi.Theme.largeSpacing
        Layout.leftMargin: LaneUi.Theme.largeSpacing
        Layout.rightMargin: LaneUi.Theme.largeSpacing
        spacing: LaneUi.Theme.smallSpacing
        QQC.ToolButton {
            icon.name: page.privateExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.privateExpanded = !page.privateExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.privateExpanded ? "Collapse private windows section" : "Expand private windows section"
        }
        QQC.Label {
            Layout.fillWidth: true
            text: "Private windows (" + page.matchCount(privateModel) + ")"
            font.pointSize: 15
            font.weight: Font.DemiBold
        }
    }
    LaneUi.FormCard {
        visible: (page.privateExpanded || page.searchText.length > 0) && page.matchCount(privateModel) > 0
        ListView {
            id: privateList
            model: privateModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            delegate: privateDelegate
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: LaneUi.Theme.largeSpacing
        Layout.leftMargin: LaneUi.Theme.largeSpacing
        Layout.rightMargin: LaneUi.Theme.largeSpacing
        spacing: LaneUi.Theme.smallSpacing
        QQC.ToolButton {
            icon.name: page.customsExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.customsExpanded = !page.customsExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.customsExpanded ? "Collapse custom apps section" : "Expand custom apps section"
        }
        QQC.Label {
            Layout.fillWidth: true
            text: "Custom apps"
            font.pointSize: 15
            font.weight: Font.DemiBold
        }
    }
    LaneUi.FormCard {
        visible: page.customsExpanded || page.searchText.length > 0
        LaneUi.FormTextFieldDelegate {
            id: customName
            label: "Name"
            placeholderText: "Work Slack"
        }
        LaneUi.FormTextFieldDelegate {
            id: customCommand
            label: "Command"
            placeholderText: "firefox -P work $url"
            onTextEdited: page.customCommandError = ""
        }
        LaneUi.FormButtonDelegate {
            text: "Add custom app"
            iconName: "list-add"
            onClicked: {
                const error = controller.addCustomTarget(customName.text, customCommand.text)
                if (error.length > 0) {
                    page.customCommandError = error
                    return
                }
                page.customCommandError = ""
                customName.text = ""
                customCommand.text = ""
            }
        }
        LaneUi.FormTextDelegate {
            visible: page.customCommandError.length > 0
            text: page.customCommandError
            description: ""
        }
    }
}
