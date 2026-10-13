import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Window
import app.lane.ui as LaneUi

LaneUi.ScrollableFormPage {
    id: page

    LaneUi.FormHeader {
        title: "System"
    }
    LaneUi.FormCard {
        LaneUi.FormTextDelegate {
            text: "Default browser"
            description: controller.isDefaultBrowser
                         ? "Lane is handling http and https links. mailto and PDF stay with their own apps."
                         : "Lane is not the default browser yet. Nothing is redirected until you opt in."
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: controller.isDefaultBrowser ? "Default browser is set" : "Use Lane as default browser"
            iconName: controller.isDefaultBrowser ? "security-high" : "checkmark"
            enabled: !controller.isDefaultBrowser
            onClicked: controller.makeDefaultBrowser()
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormSwitchDelegate {
            text: "Start Lane when I log in"
            description: "Keeps the picker instant on Wayland. Recommended."
            checked: controller.autostart
            onToggled: controller.autostart = checked
        }
    }

    LaneUi.FormHeader {
        title: "This machine"
    }
    LaneUi.FormCard {
        LaneUi.FormTextDelegate {
            text: "Discovered"
            description: controller.targetCount + " browsers, profiles, and apps"
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: "Rediscover"
            iconName: "view-refresh"
            onClicked: controller.rediscover()
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: "Try the picker"
            iconName: "window"
            onClicked: controller.openUrl("https://example.com", true)
        }
    }

    LaneUi.FormHeader {
        title: "Information"
    }
    LaneUi.FormCard {
        LaneUi.FormTextDelegate {
            text: "Version"
            description: "v" + controller.appVersion
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormTextDelegate {
            text: "License"
            description: "PolyForm Noncommercial 1.0.0. Free for personal and hobby use. Commercial use needs a separate license."
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: "Read the license"
            iconName: "text-html"
            onClicked: controller.openExternalUrl("https://polyformproject.org/licenses/noncommercial/1.0.0/")
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: "Project page on GitHub"
            description: controller.projectUrl
            iconName: "internet-services"
            onClicked: controller.openExternalUrl(controller.projectUrl)
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormTextDelegate {
            text: "Updates"
            description: {
                switch (controller.updateCheckState) {
                case "checking":
                    return "Checking for updates…"
                case "up-to-date":
                    return "Lane v" + controller.appVersion + " is up to date.\n" + controller.updateLastChecked
                case "update-available":
                    return "Lane v" + controller.updateLatestVersion + " is available. You have v" + controller.appVersion + ".\n" + controller.updateLastChecked
                case "failed":
                    return controller.updateErrorMessage + "\n" + controller.updateLastChecked
                default:
                    return "Not checked yet."
                }
            }
        }
        LaneUi.FormDelegateSeparator {}
        LaneUi.FormButtonDelegate {
            text: controller.updateCheckState === "checking" ? "Checking…" : "Check for updates"
            iconName: "view-refresh"
            enabled: controller.updateCheckState !== "checking"
            onClicked: controller.checkForUpdates()
        }
        LaneUi.FormDelegateSeparator { visible: controller.updateCheckState === "update-available" }
        LaneUi.FormButtonDelegate {
            visible: controller.updateCheckState === "update-available"
            text: "Open release page"
            iconName: "download"
            onClicked: controller.openExternalUrl(controller.updateReleaseUrl)
        }
    }
}
