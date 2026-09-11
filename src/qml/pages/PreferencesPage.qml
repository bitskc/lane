import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    title: "Preferences"

    readonly property var policyIds: ["no-rule", "always", "conflict", "never"]
    readonly property var policyLabels: [
        "Ask when Lane does not already know",
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
            description: "The Velja-style default asks only when Lane does not already know."
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
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Pause before opening"
            description: "Hold remembered, app, and default opens for a moment so you can undo."
            checked: controller.holdAutoOpen
            onToggled: controller.holdAutoOpen = checked
        }
        FormCard.FormDelegateSeparator { visible: controller.holdAutoOpen }
        FormCard.FormSpinBoxDelegate {
            id: holdMsDelegate
            visible: controller.holdAutoOpen
            label: "Pause duration"
            value: controller.holdMs
            from: 400
            to: 5000
            stepSize: 100
            textFromValue: (value) => (value / 1000).toFixed(1) + "s"
            valueFromText: (text) => Math.round(parseFloat(text) * 1000)
            onValueModified: controller.holdMs = value
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
            description: "HEAD-request known shorteners only. Private and local addresses are never followed."
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
