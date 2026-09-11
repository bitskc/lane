import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Rules"

    readonly property var scopes: ["domain", "path", "any"]
    readonly property var scopeLabels: ["Domain", "Path", "Entire URL"]
    property var danglingHosts: []

    function refreshDangling() {
        danglingHosts = controller.danglingRememberedHosts()
    }

    Component.onCompleted: refreshDangling()
    Connections {
        target: controller
        function onSettingsChanged() { page.refreshDangling() }
    }

    FormCard.FormHeader {
        title: "Match first, then remembered destinations, then apps"
    }
    FormCard.FormCard {
        Repeater {
            model: controller.ruleModel
            delegate: ColumnLayout {
                required property int index
                required property string pattern
                required property string scope
                required property string targetId
                required property bool enabled
                required property bool isRegex
                readonly property int ruleIndex: index
                readonly property bool targetMissing: !controller.targetExists(targetId)
                spacing: 0
                width: parent ? parent.width : 100

                FormCard.FormTextFieldDelegate {
                    label: "When the link matches"
                    text: pattern
                    onEditingFinished: controller.ruleModel.setPattern(ruleIndex, text)
                }
                FormCard.FormComboBoxDelegate {
                    text: "Look at"
                    model: page.scopeLabels
                    currentIndex: Math.max(0, page.scopes.indexOf(scope))
                    onActivated: controller.ruleModel.setScope(ruleIndex, page.scopes[currentIndex])
                }
                FormCard.FormComboBoxDelegate {
                    text: "Open in"
                    model: controller.targetNames
                    currentIndex: Math.max(0, controller.targetIds.indexOf(targetId))
                    onActivated: controller.ruleModel.setTargetId(ruleIndex, controller.targetIds[currentIndex])
                }
                RowLayout {
                    visible: targetMissing
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        source: "data-warning"
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        color: Kirigami.Theme.neutralTextColor
                    }
                    QQC.Label {
                        Layout.fillWidth: true
                        text: "This rule's destination no longer exists (" + targetId + "). Pick a new one or remove the rule; it will never match until you do."
                        color: Kirigami.Theme.neutralTextColor
                        font: Kirigami.Theme.smallFont
                        wrapMode: Text.WordWrap
                    }
                }
                FormCard.FormSwitchDelegate {
                    text: "Regular expression"
                    description: "Must match the whole domain, path, or URL"
                    checked: isRegex
                    onToggled: controller.ruleModel.setRegex(ruleIndex, checked)
                }
                FormCard.FormSwitchDelegate {
                    text: "Enabled"
                    checked: enabled
                    onToggled: controller.ruleModel.setEnabledAt(ruleIndex, checked)
                }
                FormCard.FormButtonDelegate {
                    text: "Remove rule"
                    icon.name: "list-remove"
                    onClicked: controller.ruleModel.removeAt(ruleIndex)
                }
                FormCard.FormDelegateSeparator {}
            }
        }
        FormCard.FormButtonDelegate {
            text: "Add rule"
            icon.name: "list-add"
            onClicked: controller.ruleModel.addRule("example.com", controller.defaultTargetId)
        }
    }

    FormCard.FormHeader {
        title: "Remembered destinations"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            visible: controller.rememberedHosts.length === 0
            text: "None yet"
            description: "Use Always for this destination in the picker."
        }
        Repeater {
            model: controller.rememberedHosts
            delegate: FormCard.FormButtonDelegate {
                required property string modelData
                readonly property bool targetMissing: !controller.targetExists(controller.rememberedTarget(modelData))
                text: modelData
                description: targetMissing
                             ? "Destination no longer exists (" + controller.rememberedTarget(modelData) + ")"
                             : controller.displayNameFor(controller.rememberedTarget(modelData))
                icon.name: targetMissing ? "data-warning" : "edit-delete"
                onClicked: controller.forgetHost(modelData)
            }
        }
        FormCard.FormButtonDelegate {
            visible: page.danglingHosts.length > 0
            text: "Clear " + page.danglingHosts.length + " broken " + (page.danglingHosts.length === 1 ? "entry" : "entries")
            description: "Removes remembered destinations pointing at a target that is no longer installed."
            icon.name: "edit-clear-history"
            onClicked: {
                controller.clearDeadRemembered()
                page.refreshDangling()
            }
        }
    }
}
