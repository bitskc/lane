import QtQuick

// Batches rename edits that land in the same event-loop tick. The old
// TargetsPage rename Timer kept a single targetId/newName pair, so two
// onEditingFinished in one tick overwrote each other and only the last
// rename reached the controller. Here each row pushes its own
// {targetId, newName}; a later edit for the same row replaces the
// earlier pending value, and one timer flush applies every pending
// rename. Rows are identified by targetId, never by index.
QtObject {
    id: root

    // Emitted once per pending row when the batch flushes. Connect
    // onRename(id, name) to the persistence call.
    signal rename(string targetId, string newName)

    property var _ids: []
    property var _names: []

    function push(targetId, newName) {
        // Copy, then write the arrays back. A var property can hand
        // back a detached copy, and pushing into that copy would drop
        // every queued rename except whatever the timer still closed over.
        var ids = root._ids.slice()
        var names = root._names.slice()
        var at = ids.indexOf(targetId)
        if (at === -1) {
            ids.push(targetId)
            names.push(newName)
        } else {
            names[at] = newName
        }
        root._ids = ids
        root._names = names
        flushTimer.restart()
    }

    property QtObject _flushTimer: Timer {
        id: flushTimer
        interval: 1
        onTriggered: {
            var ids = root._ids
            var names = root._names
            root._ids = []
            root._names = []
            for (var i = 0; i < ids.length; ++i) {
                root.rename(ids[i], names[i])
            }
        }
    }
}
