import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Window
import QtTest
import app.lane.ui as LaneUi

// Protects the round-2 Settings work (issue #71):
//  - SidebarNav is a real focusable control: Tab reaches it, Up/Down
//    move the current item, Enter/Space and click activate, and each
//    entry reports Accessible role/name/selected. The old Settings.qml
//    delegates were Rectangle+MouseArea, so Tab skipped the nav and
//    keys did nothing; these tests fail on that structure.
//  - ModelSync keeps unchanged rows alive across resyncs (delegate
//    identity survives) instead of clear()+append() destroying them.
//  - RenameQueue applies every same-tick rename instead of keeping
//    only the last one like the old shared Timer.
TestCase {
    id: test
    name: "SidebarNav"
    width: 720
    height: 480
    visible: true
    when: windowShown

    // The nav sits after a plain field so the Tab test has a starting
    // point; only these two (plus the view's delegates) are tab stops.
    QQC.TextField {
        id: tabField
        width: 100
        height: 30
    }

    LaneUi.SidebarNav {
        id: nav
        y: 40
        width: 200
        height: 200
        model: ListModel {
            ListElement { label: "Overview"; iconName: "help-about" }
            ListElement { label: "Browsers & apps"; iconName: "internet-web-browser" }
            ListElement { label: "Rules"; iconName: "view-filter" }
            ListElement { label: "Preferences"; iconName: "settings-configure" }
        }
    }
    SignalSpy {
        id: activatedSpy
        target: nav
        signalName: "activated"
    }

    ListModel { id: rows }
    ListView {
        id: diffList
        y: 260
        width: 600
        height: 240
        interactive: false
        model: rows
        moveDisplaced: Transition { YAnimator { duration: 180 } }
        delegate: Item {
            // marker is declared so tests can stamp the delegate and
            // detect whether a resync kept or recreated it.
            property string marker
            objectName: "row-" + model.targetId
            width: diffList.width
            height: 48
            QQC.ItemDelegate {
                anchors.fill: parent
                contentItem: QQC.Label { text: model.name; anchors.centerIn: parent }
            }
        }
    }
    LaneUi.ModelSync { id: modelSync }

    LaneUi.RenameQueue {
        id: renames
        onRename: (targetId, newName) => applied.push({ id: targetId, name: newName })
    }
    property var applied: []

    // Reproduces the pre-fix shared Timer: one targetId/newName pair,
    // restarted on every edit, so a second same-tick edit overwrites
    // the first before it fires.
    Timer {
        id: legacyTimer
        property string targetId
        property string newName
        interval: 1
        onTriggered: legacyApplied.push({ id: targetId, name: newName })
    }
    property var legacyApplied: []
    function legacyEdit(id, name) {
        legacyTimer.targetId = id
        legacyTimer.newName = name
        legacyTimer.start()
    }

    function init() {
        nav.currentIndex = 0
        activatedSpy.clear()
        rows.clear()
        seedRows()
        applied = []
        legacyApplied = []
        wait(100)
    }

    function seedRows() {
        rows.clear()
        rows.append({ targetId: "a", name: "Alpha", kind: "browser", hidden: false })
        rows.append({ targetId: "b", name: "Beta", kind: "browser", hidden: false })
        rows.append({ targetId: "c", name: "Gamma", kind: "container", hidden: true })
    }

    function order() {
        const ids = []
        for (let i = 0; i < rows.count; ++i) ids.push(rows.get(i).targetId)
        return ids
    }

    function test_tabReachesNav() {
        tabField.forceActiveFocus()
        verify(tabField.activeFocus)
        keyClick(Qt.Key_Tab)
        verify(nav.activeFocus, "Tab from the preceding field must land on the nav")
    }

    function test_keyboardMovesAndActivates() {
        nav.forceActiveFocus()
        verify(nav.activeFocus)
        keyClick(Qt.Key_Down)
        compare(nav.currentIndex, 1)
        keyClick(Qt.Key_Down)
        compare(nav.currentIndex, 2)
        keyClick(Qt.Key_Up)
        compare(nav.currentIndex, 1)
        keyClick(Qt.Key_Return)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 1)
        keyClick(Qt.Key_Space)
        compare(activatedSpy.count, 2)
    }

    function test_accessibleNamesAndSelectedState() {
        compare(nav.count, 4)
        const labels = ["Overview", "Browsers & apps", "Rules", "Preferences"]
        nav.currentIndex = 1
        for (let i = 0; i < 4; ++i) {
            const item = nav.itemAtIndex(i)
            verify(item, "delegate " + i + " must exist")
            compare(item.Accessible.role, Accessible.ListItem)
            compare(item.Accessible.name, labels[i])
            compare(item.Accessible.selected, i === 1)
        }
        const current = nav.itemAtIndex(1)
        current.forceActiveFocus()
        verify(current.activeFocus, "current entry must be focusable")
        keyClick(Qt.Key_Return)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 1)
    }

    function test_clickActivates() {
        const third = nav.itemAtIndex(2)
        verify(third)
        mouseClick(third, third.width / 2, third.height / 2)
        compare(nav.currentIndex, 2)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 2)
    }


    function row(id) {
        return findChild(diffList, "row-" + id)
    }

    function waitForRow(id) {
        tryVerify(() => row(id) !== null, 3000)
        return row(id)
    }

    function test_syncKeepsUnchangedDelegate() {
        compare(order(), ["a", "b", "c"])
        const b = waitForRow("b")
        verify(b)
        b.marker = "still-me"
        modelSync.sync(rows, [
            { targetId: "a", name: "Alpha", kind: "browser", hidden: false },
            { targetId: "b", name: "Beta II", kind: "browser", hidden: false },
            { targetId: "c", name: "Gamma", kind: "container", hidden: true }
        ], "targetId")
        wait(50)
        compare(order(), ["a", "b", "c"])
        compare(rows.get(1).name, "Beta II", "changed field updates in place")
        const bAfter = row("b")
        verify(bAfter === b, "unchanged row must keep its delegate")
        compare(bAfter.marker, "still-me")
    }

    function test_clearAppendLosesDelegateIdentity() {
        // The old syncModels() pattern: prove the bug the new helper
        // fixes, so the identity test above is not a tautology.
        const b = waitForRow("b")
        verify(b)
        b.marker = "doomed"
        const snapshot = []
        for (let i = 0; i < rows.count; ++i) {
            const e = rows.get(i)
            snapshot.push({ targetId: e.targetId, name: e.name, kind: e.kind, hidden: e.hidden })
        }
        rows.clear()
        for (const row of snapshot) rows.append(row)
        wait(50)
        const bAfter = waitForRow("b")
        verify(bAfter !== b, "clear()+append() must destroy delegates")
        verify(bAfter.marker === "")
    }

    function test_syncInsertRemoveMove() {
        modelSync.sync(rows, [
            { targetId: "c", name: "Gamma", kind: "container", hidden: true },
            { targetId: "a", name: "Alpha", kind: "browser", hidden: false },
            { targetId: "z", name: "Zed", kind: "app", hidden: false }
        ], "targetId")
        compare(order(), ["c", "a", "z"], "move c up, drop b, insert z in place")
        compare(rows.get(0).name, "Gamma")
        compare(rows.get(2).kind, "app")
    }

    function test_syncOrderKeyedByIdNotIndex() {
        // Same ids, new order: the model must follow row ids, not
        // blindly rewrite fields at each index.
        modelSync.sync(rows, [
            { targetId: "b", name: "Beta", kind: "browser", hidden: false },
            { targetId: "c", name: "Gamma", kind: "container", hidden: true },
            { targetId: "a", name: "Alpha", kind: "browser", hidden: false }
        ], "targetId")
        compare(order(), ["b", "c", "a"])
        compare(rows.get(0).name, "Beta")
        compare(rows.get(2).name, "Alpha")
    }


    function test_renameQueueFlushesBothEdits() {
        renames.push("a", "A2")
        renames.push("c", "C2")
        tryVerify(() => applied.length === 2, 1000)
        compare(applied, [{ id: "a", name: "A2" }, { id: "c", name: "C2" }])
        // A second edit for the same row before the flush keeps the
        // latest name, still one entry for that row.
        renames.push("a", "A3")
        renames.push("a", "A4")
        tryVerify(() => applied.length === 3, 1000)
        compare(applied[2], { id: "a", name: "A4" })
    }

    function test_legacySharedTimerDropsEdits() {
        // Locks in the pre-fix failure mode the queue replaces: two
        // same-tick edits, only the last one survives.
        legacyEdit("a", "A2")
        legacyEdit("c", "C2")
        tryVerify(() => legacyApplied.length >= 1, 1000)
        compare(legacyApplied, [{ id: "c", name: "C2" }])
    }
}
