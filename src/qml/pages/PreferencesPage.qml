import QtQuick
import app.lane.ui as LaneUi

LaneUi.ScrollableFormPage {
    title: "Preferences"

    readonly property var policyIds: ["no-rule", "always", "conflict", "never"]
    readonly property var policyLabels: [
        "Ask when Lane does not already know",
        "Always ask",
        "Ask only when rules disagree",
        "Never ask"
    ]

    LaneUi.FormHeader {
        title: "Picker"
    }
    LaneUi.FormCard {
        LaneUi.FormComboBoxDelegate {
            text: "When to ask"
            description: "The Velja-style default asks only when Lane does not already know."
            model: policyLabels
            currentIndex: Math.max(0, policyIds.indexOf(controller.pickerPolicy))
            onActivated: controller.pickerPolicy = policyIds[currentIndex]
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Prefer installed web apps"
            description: "Open GitHub, Claude, Outlook and other PWAs automatically when the link belongs to them"
            checked: controller.preferPwa
            onToggled: controller.preferPwa = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Show the full URL in the picker"
            checked: controller.showUrl
            onToggled: controller.showUrl = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Close picker when clicking the dimmed background"
            checked: controller.closeOnFocusLoss
            onToggled: controller.closeOnFocusLoss = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Pause before opening"
            description: "Hold remembered, app, and default opens for a moment so you can undo."
            checked: controller.holdAutoOpen
            onToggled: controller.holdAutoOpen = checked
        }
        LaneUi.FormDelegateSeparator { visible: controller.holdAutoOpen }
        LaneUi.FormSpinBoxDelegate {
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

    LaneUi.FormHeader {
        title: "Pipeline"
    }
    LaneUi.FormCard {
        LaneUi.FormSwitchDelegate {
            text: "Unwrap Outlook / Teams safe links"
            description: "Rules see the real destination. The original wrapper is still opened."
            checked: controller.unwrapO365
            onToggled: controller.unwrapO365 = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Expand short URLs"
            description: "HEAD-request known shorteners only. Private and local addresses are never followed."
            checked: controller.unshorten
            onToggled: controller.unshorten = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Clean links before routing"
            description: "Strip tracking parameters (utm_*, fbclid, gclid) before rules match and before the browser opens the link."
            checked: controller.stripTrackingParams
            onToggled: controller.stripTrackingParams = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Show a notification after opening"
            checked: controller.toastEnabled
            onToggled: controller.toastEnabled = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Enable per-Activity routing"
            description: "Off by default. When on, rules can be limited to a Plasma Activity and activityDefaults can name a per-Activity fallback destination. Ignored on desktops without Plasma Activities."
            checked: controller.activityRoutingEnabled
            onToggled: controller.activityRoutingEnabled = checked
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Watch for default-browser takeover"
            description: "Alert when another app or a browser update replaces Lane as the default handler, with one click to restore it."
            checked: controller.watchdogEnabled
            onToggled: controller.watchdogEnabled = checked
        }
    }
}
