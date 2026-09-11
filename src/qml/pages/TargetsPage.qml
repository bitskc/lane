import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Browsers & apps"

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
    FormCard.FormCard {
        Repeater {
            model: controller.targetModel
            delegate: FormCard.FormSwitchDelegate {
                required property string name
                required property string targetId
                required property string iconName
                required property string kind
                required property bool hidden
                required property bool incognito

                visible: kind === "browser" && !incognito
                height: visible ? implicitHeight : 0
                text: name
                description: hidden ? "Hidden from picker" : "Shown in picker"
                icon.name: iconName
                checked: !hidden
                onToggled: controller.hideTarget(targetId, !checked)

                trailing: QQC.Button {
                    text: "Default"
                    flat: true
                    onClicked: controller.defaultTargetId = targetId
                }
            }
        }
    }

    FormCard.FormHeader {
        title: "Installed web apps"
    }
    FormCard.FormCard {
        Repeater {
            model: controller.targetModel
            delegate: FormCard.FormSwitchDelegate {
                required property string name
                required property string targetId
                required property string iconName
                required property string kind
                required property bool hidden
                required property bool incognito

                visible: kind === "pwa"
                height: visible ? implicitHeight : 0
                text: name
                description: "Firefox PWA"
                icon.name: iconName
                checked: !hidden
                onToggled: controller.hideTarget(targetId, !checked)
            }
        }
    }

    FormCard.FormHeader {
        title: "Private windows"
    }
    FormCard.FormCard {
        Repeater {
            model: controller.targetModel
            delegate: FormCard.FormSwitchDelegate {
                required property string name
                required property string targetId
                required property string iconName
                required property string kind
                required property bool hidden
                required property bool incognito

                visible: incognito
                height: visible ? implicitHeight : 0
                text: name
                description: "Available to rules, hidden from the picker"
                icon.name: iconName
                checked: !hidden
                onToggled: controller.hideTarget(targetId, !checked)
            }
        }
    }

    FormCard.FormHeader {
        title: "Custom apps"
    }
    FormCard.FormCard {
        Repeater {
            model: controller.targetModel
            delegate: FormCard.FormTextDelegate {
                required property string name
                required property string targetId
                required property string kind
                required property bool hidden

                visible: kind === "app"
                height: visible ? implicitHeight : 0
                text: name
                description: targetId

                trailing: QQC.Button {
                    text: "Remove"
                    flat: true
                    onClicked: controller.removeCustomTarget(targetId)
                }
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
