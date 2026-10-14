import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Window
import QtTest
import app.lane.ui as LaneUi

TestCase {
    id: test
    name: "LaneControls"
    width: 720
    height: 480
    visible: true
    when: windowShown

    property var drops: []
    property int switchToggles: 0
    QtObject { id: preference; property bool value: false }

    ListModel { id: rows }
    ListView {
        id: list
        width: 600
        height: 240
        interactive: false
        model: rows
        property string dragId: ""
        moveDisplaced: Transition { YAnimator { duration: 180 } }
        delegate: Item {
            id: wrapper
            width: list.width
            height: 48
            QQC.ItemDelegate {
                id: row
                width: wrapper.width
                height: wrapper.height
                contentItem: RowLayout {
                    LaneUi.DragHandle {
                        objectName: "handle-" + model.targetId
                        listItem: row
                        ownerList: list
                        flickable: list
                        onDragStarted: list.dragId = model.targetId
                        onDragFinished: (id, destination) => {
                            test.drops.push({ id: id, destination: destination })
                            list.dragId = ""
                        }
                    }
                    QQC.Label { text: model.targetId; Layout.fillWidth: true }
                }
            }
        }
    }

    LaneUi.FormSwitchDelegate {
        id: toggle
        y: 280
        width: 500
        text: "Preference"
        description: "Click either the row or its switch"
        checked: preference.value
        onToggled: {
            preference.value = checked
            test.switchToggles++
        }
    }

    LaneUi.ScrollableFormPage {
        id: form
        y: 370
        width: 500
        height: 100
        LaneUi.FormCard {
            Repeater {
                model: 6
                LaneUi.FormTextFieldDelegate { label: "Nested field" }
            }
            LaneUi.FormTextFieldDelegate { id: lastField; label: "Last field" }
        }
    }

    function init() {
        list.forceActiveFocus()
        rows.clear()
        for (const id of ["a", "b", "c", "d"]) rows.append({ targetId: id })
        list.dragId = ""
        drops = []
        switchToggles = 0
        preference.value = false
        form.flickable.contentY = 0
        lastField.text = ""
        wait(200)
    }

    function order() {
        const ids = []
        for (let i = 0; i < rows.count; ++i) ids.push(rows.get(i).targetId)
        return ids
    }

    function handle(id) {
        return findChild(list.contentItem, "handle-" + id)
    }

    function test_drag_data() {
        return [
            { tag: "fast-up", id: "d", delta: -144, expected: ["d", "a", "b", "c"], destination: 0, steps: 1 },
            { tag: "fast-down", id: "a", delta: 144, expected: ["b", "c", "d", "a"], destination: 3, steps: 1 },
            { tag: "slow-up", id: "d", delta: -144, expected: ["d", "a", "b", "c"], destination: 0, steps: 12 },
            { tag: "slow-down", id: "a", delta: 144, expected: ["b", "c", "d", "a"], destination: 3, steps: 12 }
        ]
    }

    function test_drag(data) {
        const grip = handle(data.id)
        verify(grip.width >= 20 && grip.height >= 24, "Production-layout handle needs a usable hit area")
        const point = grip.mapToItem(test, grip.width / 2, grip.height / 2)
        mousePress(test, point.x, point.y)
        for (let step = 1; step <= data.steps; ++step)
            mouseMove(test, point.x, point.y + data.delta * step / data.steps, 20)
        mouseRelease(test, point.x, point.y + data.delta)
        wait(250)
        compare(order(), data.expected)
        compare(drops, [{ id: data.id, destination: data.destination }])
    }

    function test_cancelRestoresOrderAndNextClickDoesNotMove() {
        const grip = handle("a")
        const point = grip.mapToItem(test, grip.width / 2, grip.height / 2)
        mousePress(test, point.x, point.y)
        mouseMove(test, point.x, point.y + 96, 20)
        grip.enabled = false
        mouseRelease(test, point.x, point.y + 96)
        grip.enabled = true
        wait(250)
        compare(order(), ["a", "b", "c", "d"])
        compare(drops, [])
        compare(list.dragId, "")
        mouseClick(handle("c"))
        compare(order(), ["a", "b", "c", "d"])
        compare(drops, [])
    }

    function test_editorCaptionSurvivesEditingAndModelReset() {
        lastField.text = "seed"
        lastField.textField.forceActiveFocus()
        keyClick(Qt.Key_A, Qt.ControlModifier)
        keyClick(Qt.Key_R)
        compare(lastField.text, "r")
        compare(lastField.contentItem.children[0].children[0].text, "Last field")
        lastField.text = "reset"
        compare(lastField.contentItem.children[0].children[0].text, "Last field")
    }

    function test_nestedFocusedFieldScrollsIntoView() {
        lastField.textField.forceActiveFocus()
        tryVerify(() => form.flickable.contentY > 0)
        const focused = lastField.Window.window.activeFocusItem
        const point = focused.mapToItem(form.flickable, 0, 0)
        verify(point.y >= 0 && point.y + focused.height <= form.flickable.height)
    }

    function test_switchGlyphAndRowToggleOnce() {
        mouseClick(toggle, 20, toggle.height / 2)
        compare(toggle.checked, true)
        compare(switchToggles, 1)
        mouseClick(toggle, toggle.width - 25, toggle.height / 2)
        compare(toggle.checked, false)
        compare(switchToggles, 2)
        preference.value = true
        compare(toggle.checked, true)
        preference.value = false
        compare(toggle.checked, false)
        compare(switchToggles, 2)
    }
}
