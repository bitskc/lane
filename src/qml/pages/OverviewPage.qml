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
                         ? "Tern is handling http and https links. mailto and PDF stay with their own apps."
                         : "Tern is not the default browser yet. Nothing is redirected until you opt in."
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: controller.isDefaultBrowser ? "Default browser is set" : "Use Tern as default browser"
            icon.name: controller.isDefaultBrowser ? "security-high" : "checkmark"
            enabled: !controller.isDefaultBrowser
            onClicked: controller.makeDefaultBrowser()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Start Tern when I log in"
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
}
