import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "Tern"
    minimumWidth: 760
    minimumHeight: 560
    width: 860
    height: 680
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.Auto
    pageStack.initialPage: OverviewPage {}

    function goTo(pageName) {
        const component = Qt.createComponent("app.tern", pageName)
        if (component.status === Component.Error) {
            console.warn("Tern: failed to load", pageName, component.errorString())
            return
        }
        pageStack.replace(component)
    }

    globalDrawer: Kirigami.GlobalDrawer {
        title: "Tern"
        titleIcon: "app.tern.Tern"
        isMenu: true
        actions: [
            Kirigami.Action {
                text: "Overview"
                icon.name: "help-about"
                onTriggered: root.goTo("OverviewPage")
            },
            Kirigami.Action {
                text: "Browsers & apps"
                icon.name: "internet-web-browser"
                onTriggered: root.goTo("TargetsPage")
            },
            Kirigami.Action {
                text: "Rules"
                icon.name: "view-filter"
                onTriggered: root.goTo("RulesPage")
            },
            Kirigami.Action {
                text: "Preferences"
                icon.name: "settings-configure"
                onTriggered: root.goTo("PreferencesPage")
            }
        ]
    }
}
