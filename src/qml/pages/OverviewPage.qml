import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Overview"

    FormCard.FormHeader {
        title: "System"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: "Default browser"
            description: controller.isDefaultBrowser
                         ? "Lane is handling http and https links. mailto and PDF stay with their own apps."
                         : "Lane is not the default browser yet. Nothing is redirected until you opt in."
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: controller.isDefaultBrowser ? "Default browser is set" : "Use Lane as default browser"
            icon.name: controller.isDefaultBrowser ? "security-high" : "checkmark"
            enabled: !controller.isDefaultBrowser
            onClicked: controller.makeDefaultBrowser()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Start Lane when I log in"
            description: "Keeps the picker instant on Wayland. Recommended."
            checked: controller.autostart
            onToggled: controller.autostart = checked
        }
    }

    FormCard.FormHeader {
        title: "This machine"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: "Discovered"
            description: controller.targetCount + " browsers, profiles, and apps"
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Rediscover"
            icon.name: "view-refresh"
            onClicked: controller.rediscover()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Try the picker"
            icon.name: "window"
            onClicked: controller.openUrl("https://example.com", true)
        }
    }

    FormCard.FormHeader {
        title: "Information"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: "Version"
            description: "v" + controller.appVersion
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextDelegate {
            text: "License"
            description: "PolyForm Noncommercial 1.0.0. Free for personal and hobby use. Commercial use needs a separate license."
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Read the license"
            icon.name: "text-html"
            onClicked: controller.openExternalUrl("https://polyformproject.org/licenses/noncommercial/1.0.0/")
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Project page on GitHub"
            description: controller.projectUrl
            icon.name: "internet-services"
            onClicked: controller.openExternalUrl(controller.projectUrl)
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextDelegate {
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
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: controller.updateCheckState === "checking" ? "Checking…" : "Check for updates"
            icon.name: "view-refresh"
            enabled: controller.updateCheckState !== "checking"
            onClicked: controller.checkForUpdates()
        }
        FormCard.FormDelegateSeparator { visible: controller.updateCheckState === "update-available" }
        FormCard.FormButtonDelegate {
            visible: controller.updateCheckState === "update-available"
            text: "Open release page"
            icon.name: "download"
            onClicked: controller.openExternalUrl(controller.updateReleaseUrl)
        }
    }
}
