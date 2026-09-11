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

    FormCard.FormHeader {
        title: "Match first, then remembered hosts, then apps"
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
        title: "Remembered sites"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            visible: controller.rememberedHosts.length === 0
            text: "None yet"
            description: "Use Always for this site in the picker."
        }
        Repeater {
            model: controller.rememberedHosts
            delegate: FormCard.FormButtonDelegate {
                required property string modelData
                text: modelData
                description: controller.displayNameFor(controller.rememberedTarget(modelData))
                icon.name: "edit-delete"
                onClicked: controller.forgetHost(modelData)
            }
        }
    }
}
