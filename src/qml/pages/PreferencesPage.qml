import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    title: "Preferences"

    readonly property var policyIds: ["no-rule", "always", "conflict", "never"]
    readonly property var policyLabels: [
        "Ask when Tern does not already know",
        "Always ask",
        "Ask only when rules disagree",
        "Never ask"
    ]

    FormCard.FormHeader {
        title: "Picker"
    }
    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            text: "When to ask"
            description: "The Velja-style default asks only when Tern does not already know."
            model: policyLabels
            currentIndex: Math.max(0, policyIds.indexOf(controller.pickerPolicy))
            onActivated: controller.pickerPolicy = policyIds[currentIndex]
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Prefer installed web apps"
            description: "Open GitHub, Claude, Outlook and other PWAs automatically when the link belongs to them"
            checked: controller.preferPwa
            onToggled: controller.preferPwa = checked
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Show the full URL in the picker"
            checked: controller.showUrl
            onToggled: controller.showUrl = checked
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Close picker when clicking the dimmed background"
            checked: controller.closeOnFocusLoss
            onToggled: controller.closeOnFocusLoss = checked
        }
    }

    FormCard.FormHeader {
        title: "Pipeline"
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: "Unwrap Outlook / Teams safe links"
            description: "Rules see the real destination. The original wrapper is still opened."
            checked: controller.unwrapO365
            onToggled: controller.unwrapO365 = checked
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Expand short URLs"
            description: "Follows bit.ly and friends so rules can match the destination"
            checked: controller.unshorten
            onToggled: controller.unshorten = checked
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Show a notification after opening"
            checked: controller.toastEnabled
            onToggled: controller.toastEnabled = checked
        }
    }
}
