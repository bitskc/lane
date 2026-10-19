import QtQuick

// Keyed diff-update of a ListModel against an array of row objects.
// The old TargetsPage sync did clear() + append() on every
// settingsChanged, which destroyed every delegate and aborted the
// 250 ms moveDisplaced drag settle. sync() instead matches rows by the
// `keyRole` property: rows absent from `rows` are removed, new rows are
// appended, rows that moved are move()d, and rows still in place get
// only their changed fields set() so untouched rows keep their
// delegate (and its identity) alive. Order follows `rows`, keyed by
// `keyRole`, never by row index.
QtObject {
    id: root

    function sync(model, rows, keyRole) {
        var target = []
        for (var t = 0; t < rows.length; ++t) {
            target.push(rows[t][keyRole])
        }
        for (var i = model.count - 1; i >= 0; --i) {
            if (target.indexOf(model.get(i)[keyRole]) === -1) {
                model.remove(i)
            }
        }
        for (var j = 0; j < rows.length; ++j) {
            var row = rows[j]
            var id = row[keyRole]
            var at = -1
            for (var k = 0; k < model.count; ++k) {
                if (model.get(k)[keyRole] === id) {
                    at = k
                    break
                }
            }
            if (at === -1) {
                model.insert(j, row)
                at = j
            } else if (at !== j) {
                model.move(at, j, 1)
            }
            var cur = model.get(j)
            var changed = {}
            var dirty = false
            for (var key in row) {
                if (cur[key] !== row[key]) {
                    changed[key] = row[key]
                    dirty = true
                }
            }
            if (dirty) {
                model.set(j, changed)
            }
        }
    }
}
