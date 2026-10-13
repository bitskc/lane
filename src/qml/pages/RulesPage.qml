import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import app.lane.ui as LaneUi

LaneUi.ScrollableFormPage {
    id: page
    title: "Rules"

    readonly property var scopes: ["domain", "path", "any"]
    readonly property var scopeLabels: ["Domain", "Path", "Entire URL"]
    // First entry clears a rule's Activity scope; the rest come from the
    // running activities service (empty on non-Plasma desktops).
    readonly property var activityIds: [""].concat(controller.availableActivities.map(function(a) { return a.id }))
    readonly property var activityLabels: ["Any activity"].concat(controller.availableActivities.map(function(a) { return a.name }))
    property var danglingHosts: []
    // Bumped on every settingsChanged so targetMissing (below) — which
    // calls controller.targetExists(), a method, not a NOTIFYable
    // property — re-evaluates. A QML binding only tracks property reads;
    // without this it would freeze at whatever targetExists() returned
    // when the delegate was created, going stale the moment a custom
    // target is removed without a full RulesPage reload.
    property int settingsRevision: 0

    function refreshDangling() {
        danglingHosts = controller.danglingRememberedHosts()
    }

    Component.onCompleted: refreshDangling()
    Connections {
        target: controller
        function onSettingsChanged() {
            page.refreshDangling()
            page.settingsRevision++
        }
    }

    LaneUi.FormHeader {
        title: "Match first, then remembered destinations, then apps"
    }
    LaneUi.FormCard {
        Repeater {
            model: controller.ruleModel
            delegate: ColumnLayout {
                required property int index
                required property string pattern
                required property string scope
                required property string targetId
                required property bool ruleEnabled
                required property string activity
                required property bool isRegex
                readonly property int ruleIndex: index
                readonly property bool targetMissing: page.settingsRevision >= 0 && !controller.targetExists(targetId)
                spacing: 0
                width: parent ? parent.width : 100

                LaneUi.FormTextFieldDelegate {
                    label: "When the link matches"
                    text: pattern
                    onEditingFinished: controller.ruleModel.setPattern(ruleIndex, text)
                }
                LaneUi.FormComboBoxDelegate {
                    text: "Look at"
                    model: page.scopeLabels
                    currentIndex: Math.max(0, page.scopes.indexOf(scope))
                    onActivated: controller.ruleModel.setScope(ruleIndex, page.scopes[currentIndex])
                }
                LaneUi.FormComboBoxDelegate {
                    visible: controller.activityRoutingEnabled
                    text: "Only in Activity"
                    description: "Restricts this rule to one Plasma Activity. With no Activity selected it applies everywhere."
                    model: page.activityLabels
                    currentIndex: Math.max(0, page.activityIds.indexOf(activity))
                    onActivated: controller.ruleModel.setActivity(ruleIndex, page.activityIds[currentIndex])
                }
                LaneUi.FormComboBoxDelegate {
                    text: "Open in"
                    model: controller.targetNames
                    currentIndex: Math.max(0, controller.targetIds.indexOf(targetId))
                    onActivated: controller.ruleModel.setTargetId(ruleIndex, controller.targetIds[currentIndex])
                }
                RowLayout {
                    visible: targetMissing
                    Layout.fillWidth: true
                    Layout.leftMargin: LaneUi.Theme.largeSpacing
                    Layout.rightMargin: LaneUi.Theme.largeSpacing
                    Layout.bottomMargin: LaneUi.Theme.smallSpacing
                    spacing: LaneUi.Theme.smallSpacing
                    LaneUi.LaneIcon {
                        iconName: "data-warning"
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }
                    QQC.Label {
                        Layout.fillWidth: true
                        text: "This rule's destination no longer exists (" + targetId + "). Pick a new one or remove the rule; it will never match until you do."
                        color: LaneUi.Theme.neutralTextColor
                        font.pointSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
                LaneUi.FormSwitchDelegate {
                    text: "Regular expression"
                    description: "Must match the whole domain, path, or URL"
                    checked: isRegex
                    onToggled: controller.ruleModel.setRegex(ruleIndex, checked)
                }
                LaneUi.FormSwitchDelegate {
                    text: "Enabled"
                    checked: ruleEnabled
                    onToggled: controller.ruleModel.setEnabledAt(ruleIndex, checked)
                }
                LaneUi.FormButtonDelegate {
                    text: "Remove rule"
                    iconName: "list-remove"
                    onClicked: controller.ruleModel.removeAt(ruleIndex)
                }
                LaneUi.FormDelegateSeparator {}
            }
        }
        LaneUi.FormButtonDelegate {
            text: "Add rule"
            iconName: "list-add"
            onClicked: controller.ruleModel.addRule("example.com", controller.defaultTargetId)
        }
    }

    LaneUi.FormHeader {
        title: "Per-Activity fallbacks"
        visible: controller.activityRoutingEnabled && controller.availableActivities.length > 0
    }
    LaneUi.FormCard {
        visible: controller.activityRoutingEnabled && controller.availableActivities.length > 0
        LaneUi.FormTextDelegate {
            text: "Used instead of the overall fallback target while one of these Activities is active and no rule or remembered destination matches."
            description: " "
        }
        Repeater {
            model: controller.availableActivities
            delegate: LaneUi.FormComboBoxDelegate {
                required property var modelData
                text: modelData.name
                // Index 0 is "Use default" (empty = fall through to
                // defaultTargetId, router.cpp's own fallback order).
                model: ["Use default"].concat(controller.targetNames)
                currentIndex: {
                    const _r = page.settingsRevision // force re-eval on settingsChanged
                    const id = controller.activityDefaultTarget(modelData.id)
                    return id.length === 0 ? 0 : Math.max(0, controller.targetIds.indexOf(id) + 1)
                }
                onActivated: controller.setActivityDefaultTarget(modelData.id,
                    currentIndex === 0 ? "" : controller.targetIds[currentIndex - 1])
            }
        }
    }

    LaneUi.FormHeader {
        title: "Remembered destinations"
    }
    LaneUi.FormCard {
        LaneUi.FormTextDelegate {
            visible: controller.rememberedHosts.length === 0
            text: "None yet"
            description: "Use Always for this destination in the picker."
        }
        Repeater {
            model: controller.rememberedHosts
            delegate: LaneUi.FormButtonDelegate {
                required property string modelData
                readonly property bool targetMissing: !controller.targetExists(controller.rememberedTarget(modelData))
                text: modelData
                description: targetMissing
                             ? "Destination no longer exists (" + controller.rememberedTarget(modelData) + ")"
                             : controller.displayNameFor(controller.rememberedTarget(modelData))
                iconName: targetMissing ? "data-warning" : "edit-delete"
                onClicked: controller.forgetHost(modelData)
            }
        }
        LaneUi.FormButtonDelegate {
            visible: page.danglingHosts.length > 0
            text: "Clear " + page.danglingHosts.length + " broken " + (page.danglingHosts.length === 1 ? "entry" : "entries")
            description: "Removes remembered destinations pointing at a target that is no longer installed."
            iconName: "edit-clear-history"
            onClicked: {
                controller.clearDeadRemembered()
                page.refreshDangling()
            }
        }
    }
}
