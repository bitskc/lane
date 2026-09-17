import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Browsers & apps"

    property int rowHeight: 48
    property string customCommandError: ""
    property string searchText: ""
    property bool browsersExpanded: true
    property bool containersExpanded: true
    property bool pwasExpanded: true
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
        return matchCount(browserModel) + matchCount(containerModel)
            + matchCount(pwaModel) + matchCount(privateModel) + matchCount(customModel)
    }

    Timer {
        id: renameTimer
        property string targetId
        property string newName
        interval: 1
        onTriggered: controller.renameTarget(targetId, newName)
    }

    ListModel { id: browserModel }
    ListModel { id: containerModel }
    ListModel { id: pwaModel }
    ListModel { id: customModel }
    ListModel { id: privateModel }

    Component {
        id: browserDelegate
        Item {
            id: wrapper
            width: browserList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            QQC.ItemDelegate {
                id: listItem
                width: wrapper.width
                height: wrapper.height
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.ListItemDragHandle {
                        listItem: listItem
                        listView: browserList
                        // Filtered-out rows stay in browserModel at their
                        // original index (only their visual height collapses
                        // to 0, see the ListView below), so a drag computed
                        // against the full model while a search filter is
                        // active can persist an order different from what was
                        // visually dragged. Disabling the handle while
                        // filtered avoids that ambiguity outright; clearing
                        // the search box restores dragging.
                        enabled: page.searchText.trim().length === 0
                        onMoveRequested: (oldIndex, newIndex) => {
                            if (browserList.dragId === "")
                                browserList.dragId = browserModel.get(oldIndex).targetId
                            browserModel.move(oldIndex, newIndex, 1)
                        }
                        onDropped: (oldIndex, newIndex) => {
                            if (newIndex >= 0 && browserList.dragId !== "") {
                                controller.moveTarget(browserList.dragId, newIndex)
                            }
                            browserList.dragId = ""
                        }
                    }
                    Kirigami.Icon {
                        source: model.iconName
                        implicitWidth: 22
                        implicitHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.TextField {
                        id: renameField
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
                        HoverHandler { id: renameHover }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: renameHover.hovered && !renameField.activeFocus
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
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
    }

    Component {
        id: containerDelegate
        Item {
            id: wrapper
            width: containerList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            QQC.ItemDelegate {
                id: listItem
                width: wrapper.width
                height: wrapper.height
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.ListItemDragHandle {
                        listItem: listItem
                        listView: containerList
                        enabled: page.searchText.trim().length === 0
                        onMoveRequested: (oldIndex, newIndex) => {
                            if (containerList.dragId === "")
                                containerList.dragId = containerModel.get(oldIndex).targetId
                            containerModel.move(oldIndex, newIndex, 1)
                        }
                        onDropped: (oldIndex, newIndex) => {
                            if (newIndex >= 0 && containerList.dragId !== "") {
                                controller.moveTarget(containerList.dragId, newIndex)
                            }
                            containerList.dragId = ""
                        }
                    }
                    Kirigami.Icon {
                        source: model.iconName
                        implicitWidth: 22
                        implicitHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.TextField {
                        id: renameField
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
                        HoverHandler { id: renameHover }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: renameHover.hovered && !renameField.activeFocus
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
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
    }

    Component {
        id: pwaDelegate
        Item {
            id: wrapper
            width: pwaList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            QQC.ItemDelegate {
                id: listItem
                width: wrapper.width
                height: wrapper.height
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.ListItemDragHandle {
                        listItem: listItem
                        listView: pwaList
                        enabled: page.searchText.trim().length === 0
                        onMoveRequested: (oldIndex, newIndex) => {
                            if (pwaList.dragId === "")
                                pwaList.dragId = pwaModel.get(oldIndex).targetId
                            pwaModel.move(oldIndex, newIndex, 1)
                        }
                        onDropped: (oldIndex, newIndex) => {
                            if (newIndex >= 0 && pwaList.dragId !== "") {
                                controller.moveTarget(pwaList.dragId, newIndex)
                            }
                            pwaList.dragId = ""
                        }
                    }
                    Kirigami.Icon {
                        source: model.iconName
                        implicitWidth: 22
                        implicitHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.TextField {
                        id: renameField
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
                        HoverHandler { id: renameHover }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: renameHover.hovered && !renameField.activeFocus
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
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
    }

    Component {
        id: customDelegate
        Item {
            id: wrapper
            width: customList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
            QQC.ItemDelegate {
                id: listItem
                width: wrapper.width
                height: wrapper.height
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.ListItemDragHandle {
                        listItem: listItem
                        listView: customList
                        enabled: page.searchText.trim().length === 0
                        onMoveRequested: (oldIndex, newIndex) => {
                            if (customList.dragId === "")
                                customList.dragId = customModel.get(oldIndex).targetId
                            customModel.move(oldIndex, newIndex, 1)
                        }
                        onDropped: (oldIndex, newIndex) => {
                            if (newIndex >= 0 && customList.dragId !== "") {
                                controller.moveTarget(customList.dragId, newIndex)
                            }
                            customList.dragId = ""
                        }
                    }
                    Kirigami.Icon {
                        source: model.iconName
                        implicitWidth: 22
                        implicitHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                    }
                    QQC.TextField {
                        id: renameField
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
                        HoverHandler { id: renameHover }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: renameHover.hovered && !renameField.activeFocus
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
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
    }

    Component {
        id: privateDelegate
        QQC.ItemDelegate {
            width: privateList.width
            height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
            visible: height > 0
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
                    id: renameField
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
                    HoverHandler { id: renameHover }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        visible: renameHover.hovered && !renameField.activeFocus
                        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
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
        containerModel.clear()
        var containers = controller.targetModel.targetsByKind("container")
        for (var i = 0; i < containers.length; i++)
            containerModel.append(containers[i])
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

    function hasGeckoBrowsers() {
        var browsers = controller.targetModel.targetsByKind("browser")
        for (var i = 0; i < browsers.length; i++) {
            if (!browsers[i].incognito && browsers[i].engine === "gecko")
                return true
        }
        return false
    }

    Component.onCompleted: syncModels()
    Connections {
        target: controller
        function onSettingsChanged() { syncModels() }
    }

    FormCard.FormHeader {
        title: "Search"
    }
    FormCard.FormCard {
        FormCard.AbstractFormDelegate {
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
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing

        Accessible.role: Accessible.StaticText
        Accessible.name: "No matches"
    }

    FormCard.FormHeader {
        title: "Default"
    }
    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            text: "Fallback target"
            description: "Used when Lane does not ask and no rule matches"
            model: controller.targetNames
            currentIndex: Math.max(0, controller.targetIds.indexOf(controller.defaultTargetId))
            onActivated: controller.defaultTargetId = controller.targetIds[currentIndex]
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        QQC.ToolButton {
            icon.name: page.browsersExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.browsersExpanded = !page.browsersExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.browsersExpanded ? "Collapse browsers section" : "Expand browsers section"
        }
        Kirigami.Heading {
            level: 4
            Layout.fillWidth: true
            text: "Browsers (" + page.matchCount(browserModel) + ")"
        }
    }
    QQC.Label {
        visible: page.browsersExpanded || page.searchText.length > 0
        text: "Drag to reorder. Click a name to rename."
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
    }
    FormCard.FormCard {
        visible: (page.browsersExpanded || page.searchText.length > 0) && page.matchCount(browserModel) > 0
        ListView {
            id: browserList
            model: browserModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: browserDelegate
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        QQC.ToolButton {
            icon.name: page.containersExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.containersExpanded = !page.containersExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.containersExpanded ? "Collapse containers section" : "Expand containers section"
        }
        Kirigami.Heading {
            level: 4
            Layout.fillWidth: true
            text: "Containers (" + page.matchCount(containerModel) + ")"
        }
    }
    QQC.Label {
        visible: page.containersExpanded || page.searchText.length > 0
        text: "Opens the link in that Firefox or Zen container. Needs a container protocol extension in the browser (Open URL in Container, or Default Container Handler)."
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
    }
    FormCard.FormCard {
        visible: (page.containersExpanded || page.searchText.length > 0) && page.matchCount(containerModel) > 0
        ListView {
            id: containerList
            model: containerModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: containerDelegate
        }
    }
    QQC.Label {
        visible: containerModel.count === 0 && page.hasGeckoBrowsers() && page.searchText.length === 0
        text: "No containers found. Zen and Firefox write them to containers.json in the profile folder."
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        QQC.ToolButton {
            icon.name: page.pwasExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.pwasExpanded = !page.pwasExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.pwasExpanded ? "Collapse web apps section" : "Expand web apps section"
        }
        Kirigami.Heading {
            level: 4
            Layout.fillWidth: true
            text: "Installed web apps (" + page.matchCount(pwaModel) + ")"
        }
    }
    FormCard.FormCard {
        visible: (page.pwasExpanded || page.searchText.length > 0) && page.matchCount(pwaModel) > 0
        ListView {
            id: pwaList
            model: pwaModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: pwaDelegate
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        QQC.ToolButton {
            icon.name: page.privateExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.privateExpanded = !page.privateExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.privateExpanded ? "Collapse private windows section" : "Expand private windows section"
        }
        Kirigami.Heading {
            level: 4
            Layout.fillWidth: true
            text: "Private windows (" + page.matchCount(privateModel) + ")"
        }
    }
    FormCard.FormCard {
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
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        QQC.ToolButton {
            icon.name: page.customsExpanded ? "arrow-down" : "arrow-right"
            flat: true
            onClicked: page.customsExpanded = !page.customsExpanded

            Accessible.role: Accessible.Button
            Accessible.name: page.customsExpanded ? "Collapse custom apps section" : "Expand custom apps section"
        }
        Kirigami.Heading {
            level: 4
            Layout.fillWidth: true
            text: "Custom apps (" + page.matchCount(customModel) + ")"
        }
    }
    FormCard.FormCard {
        visible: page.customsExpanded || page.searchText.length > 0
        ListView {
            id: customList
            model: customModel
            interactive: false
            spacing: 0
            Layout.fillWidth: true
            implicitHeight: contentHeight
            visible: count > 0
            moveDisplaced: Transition {
                YAnimator { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad }
            }
            property string dragId: ""
            delegate: customDelegate
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
            status: page.customCommandError.length > 0 ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
            statusMessage: page.customCommandError
            onTextEdited: page.customCommandError = ""
        }
        FormCard.FormButtonDelegate {
            text: "Add custom app"
            icon.name: "list-add"
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
    }
}
