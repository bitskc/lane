import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: page
    title: "Tern"

    Kirigami.Icon {
        source: "app.tern.Tern"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 72
        Layout.preferredHeight: 72
        Layout.topMargin: Kirigami.Units.largeSpacing
    }

    QQC.Label {
        text: "Open the right thing"
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
        opacity: 0.7
    }

    FormCard.FormHeader {
        title: "System"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: "Default browser"
            description: controller.isDefaultBrowser ? "Tern is handling http and https links" : "Tern is not the default browser"
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: controller.isDefaultBrowser ? "Default browser is set" : "Use Tern as default browser"
            icon.name: "checkmark"
            enabled: !controller.isDefaultBrowser
            onClicked: controller.makeDefaultBrowser()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: "Start Tern when I log in"
            description: "Keeps the picker instant on Wayland"
            checked: controller.autostart
            onToggled: controller.autostart = checked
        }
    }

    FormCard.FormHeader {
        title: "Today"
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: "Discovered targets"
            description: controller.targetCount + " browsers, profiles, and apps"
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Browsers & apps"
            icon.name: "internet-web-browser"
            onClicked: applicationWindow().goTo("TargetsPage")
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Rules"
            icon.name: "view-filter"
            onClicked: applicationWindow().goTo("RulesPage")
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Preferences"
            icon.name: "settings-configure"
            onClicked: applicationWindow().goTo("PreferencesPage")
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: "Rediscover"
            icon.name: "view-refresh"
            onClicked: controller.rediscover()
        }
    }

    FormCard.FormHeader {
        title: "Try it"
    }
    FormCard.FormCard {
        FormCard.FormButtonDelegate {
            text: "Open picker with example.com"
            icon.name: "window"
            onClicked: controller.openUrl("https://example.com", true)
        }
    }
}
