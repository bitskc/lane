# Pre-Landing Review: PR #6 — fix/blank-target-names

**Date:** 2026-09-14
**Commit:** `5eac43a` (single commit, `main..HEAD`)
**Scope:** `src/qml/pages/TargetsPage.qml` (5 `ListView` delegate assignments + 5 delegate root `height`/`visible` bindings), `CHANGELOG.md` (+12 lines).
**Method:** `/review` skill checklist. Full diff read, plus `TargetsPage.qml` read in full (all 653 lines: five `Component`/`ListView` pairs, `syncModels()`, drag/search/filter logic). Cross-checked against the vendored Kirigami source for `Kirigami.ListItemDragHandle` (`/usr/lib/qt6/qml/org/kde/kirigami/controls/ListItemDragHandle.qml`) since that's the one piece of runtime semantics a diff-only read can't settle. Ran `qmllint` on both the pre-PR and post-PR file and diffed the warning sets (line-shift only, no new categories). No build/run of the app itself (out of scope per assignment — this is a static-analysis + source-authority review).

**Verdict: Request changes.** The root-cause diagnosis and the blank-name fix itself are correct and well-verified. But the fix's *mechanism* — making the `ItemDelegate` the direct `ListView.delegate` — directly contradicts Kirigami's own documented contract for `ListItemDragHandle`, on the four sections that have one (browser, container, pwa, custom). This is a plausible, non-hypothetical drag-reorder regression that the PR's test evidence (12/12 `ctest`, C++-only) cannot have caught, and the PR body doesn't mention checking it.

---

## What the diff does

Confirmed against `git show origin/main:src/qml/pages/TargetsPage.qml` and the PR diff: five `ListView`s (`browserList`, `containerList`, `pwaList`, `customList`, `privateList`) each used to declare `delegate: Loader { width: ...; height: page.matchesSearch(...) ? page.rowHeight : 0; visible: height > 0; sourceComponent: <name>Delegate }`, where `<name>Delegate` is a `Component` whose root is a `QQC.ItemDelegate` reading `model.name`/`model.iconName`/`model.discoveredName`/`model.hidden`/`model.targetId`.

The PR (`TargetsPage.qml:82-88`, `:96-102`, `:110-116`, `:124-130`, `:138-144` in the diff's line numbers) drops the `Loader` and assigns `delegate: <name>Delegate` directly, moving the `height`/`visible` search-filter expression onto each `Component`'s root `ItemDelegate` (`:58-60`, `:132-134`, `:198-200`, `:258-260`, `:318-320` in the post-PR file).

**Root cause is correctly diagnosed.** `Loader.sourceComponent`-instantiated objects do not inherit the `ListView` row's `model`/`index` context — only the `Loader` itself (the actual per-row delegate) has that context. This is real, standard QML behavior, not a misdiagnosis. Assigning the `Component` directly as `delegate` is the standard fix for restoring `model.*` bindings, and it does restore them: every `model.*` reference in all five delegates now resolves inside an object the `ListView` itself instantiates per row.

## Pass 1 — CRITICAL categories

Not applicable in the checklist's usual sense (no SQL, concurrency, LLM boundary, shell injection, or new enum value). The one item worth CRITICAL-level scrutiny — because it's a documented API contract violation, not a style nit — is the drag-reorder wiring below.

## Drag-reorder — the requested scrutiny point, and the actual finding

**(confidence: 8/10) `TargetsPage.qml:56-126` (browser), `:130-192` (container), `:196-252` (pwa), `:256-313` (custom) — making the `ItemDelegate` the direct `ListView.delegate` violates Kirigami's own documented contract for `ListItemDragHandle`, and is very likely to break drag-to-reorder on exactly the four sections that have it.**

The vendored Kirigami source itself says, in `ListItemDragHandle.qml:22-23`:

> "In order for ListItemDragHandle to work correctly, **the listItem that is being dragged should not directly be the delegate of the ListView, but a child of it.**"

And its own doc-comment usage example (`ListItemDragHandle.qml:31-75`) is, verbatim, the exact pattern this PR removes:

```qml
Component {
    id: delegateComponent
    QQC2.ItemDelegate {
        id: listItem
        ...
    }
}
ListView {
    id: mainList
    ...
    delegate: Loader {
        width: mainList.width
        sourceComponent: delegateComponent
    }
}
```

That's the `Loader`-wrapping pattern the PR calls the bug and deletes. It isn't accidental legacy cruft — it's the shape Kirigami's own reference example uses, specifically so that `listItem` (the thing being dragged) is a **child of** the real per-row delegate object, not the delegate object itself.

Why it matters mechanically: `ListItemDragHandle`'s `onPressed` (`ListItemDragHandle.qml:191-202`) does:

```qml
onPressed: mouse => {
    internal.originalParent = root.listItem.parent;
    root.listItem.parent = root.listView;
    root.listItem.y = internal.originalParent.mapToItem(root.listItem.parent, root.listItem.x, root.listItem.y).y;
    ...
}
```

It reparents `listItem` out of its current parent and onto `root.listView` directly for the duration of the drag, then reparents it back on `dropped()` (`:223-230`). In the pre-PR (documented-correct) shape, `listItem`'s parent is the `Loader` — which *is* the `ListView`'s real per-index delegate, stays behind, keeps its row's height/position reserved, and gets its child handed back on drop. In the post-PR shape, `listItem` **is** the `ListView`'s real per-index delegate object — the thing the `ListView`'s positioner is actively tracking and sizing for that model index. Reparenting it away from the `ListView`'s content item mid-drag removes the actual view item the positioner manages, not a placeholder holding its place; the positioner has no stand-in left in the layout for that index while the item is detached, and re-parenting a live `ListView`-owned delegate object out from under the positioner is exactly what the doc comment is warning against. At best this produces the layout glitch the doc is written to prevent (row snapping, gaps, other rows shifting during the drag); at worst it's undefined behavior per Kirigami's own words.

**Scope: applies to `browserList`, `containerList`, `pwaList`, `customList`.** `privateList`/`privateDelegate` (`:315-357`) has no `Kirigami.ListItemDragHandle` at all — private windows aren't reorderable — so that section is unaffected by this concern.

**Why this wasn't caught:** the PR's own test evidence is `ctest` (12/12, C++ `lane-core` unit tests) — this is a pure-QML delegate defect, and the repo (per the PR body itself) has no QML rendering/interaction test infrastructure. Static tools don't catch it either: `qmllint` on both the pre- and post-PR file produces the same warning categories (`Unqualified access`, one `Quick.layout-positioning` note), just at shifted line numbers — nothing about `ListItemDragHandle`'s reparenting contract is checkable statically.

**This is not a "cannot verify, so ignore" gap.** It's the specific thing the assignment asked to check, it's backed by the exact upstream doc comment and example that this diff contradicts (not speculation about an unfamiliar API), and drag-reorder is a real, exercised feature in this file (`onMoveRequested`/`onDropped` call `controller.moveTarget()`, i.e. persists to config) — a regression here would be a second user-visible bug shipped in the same fix that closes the first one. Recommend: manually drag-reorder rows on all four affected sections against a build of this branch before merge, or move the drag handle's child back under a thin non-`Loader` wrapper Item that stays as the real delegate (keeping `model.*` access via `required property` forwarding, since a plain `Item` doesn't get row context either — the fix needs *some* thin real per-row Item other than the dragged `ItemDelegate` itself, not necessarily the removed `Loader`).

## Pass 2 — remaining scrutiny points

**Search-filter height/visible move (`:59-60` etc.) — correct, not a regression.** Pre-PR, the collapse (`height: matchesSearch(...) ? rowHeight : 0` + `visible: height > 0`) lived on the `Loader` (the real delegate, so `model.*` resolved fine there already — this is why the bug report was specifically "no name/icon", not "no rows at all": the `Loader`'s own filtering logic worked throughout). The inner `ItemDelegate` had a hardcoded `height: page.rowHeight` pre-PR, which no longer matters once the `ItemDelegate` fully replaces the `Loader` as delegate and inherits its filtering responsibility. Moving the identical expression onto the object that's now the actual delegate preserves search-collapse behavior exactly; verified there's no double-collapse or dropped case across all five diff hunks — each section gets exactly one `height`/`visible` pair, on the new delegate root.

**Drag-reorder-while-filtered — unchanged, not touched by this diff.** The `enabled: page.searchText.trim().length === 0` guard on each `ListItemDragHandle` (e.g. `:74`), and its explanatory comment about filtered rows keeping their model index while only their visual height collapses, are untouched context lines in the diff (not part of any hunk). This pre-existing mitigation for the order-persistence quirk noted in prior review rounds stands as-is; this PR neither fixes nor breaks it.

**No remaining `model.` references resolving differently.** Grepped the full `src/qml` tree for `Loader`/`sourceComponent`: zero hits anywhere in the repo, not just `TargetsPage.qml`. There is no other nested `Loader`/`Component` in this file that legitimately still needs — or was left holding — a broken `model` context.

**`required property` declarations — not applicable to this diff.** `TargetsPage.qml`'s delegates use the dynamic `model.<role>` accessor form throughout (unchanged by this PR), not `required property` declarations (unlike `RulesPage.qml`, `Picker.qml`, `Settings.qml`, which do use `required property` for their own unrelated delegates). Nothing in this diff introduces or touches a `required property`, so there's no `required`-vs-`model`-context mismatch to check here.

**CHANGELOG entry (`CHANGELOG.md:195-206`) — accurate.** Matches the diff precisely: names all five affected sections, correctly attributes the cause to `Loader`/`sourceComponent` context loss (not e.g. a backend data bug), and correctly describes the fix as moving the filter check onto "the delegate's own root item, which does have model access." Lands under `## [Unreleased] / ### Fixed`, the correct section (verified against `CHANGELOG.md:8`, `:253` — `[Unreleased]` header and the last released `[0.1.0]` boundary).

## Suppressions applied

- Did not flag the missing QML test coverage as a new gap distinct from the drag-reorder finding — the repo has no `qmltestrunner` infra at all (stated in the PR body, consistent with `tests/CMakeLists.txt` wiring only `QTest` C++ unit tests), so this PR isn't carving out coverage that existed before.
- Did not flag the `qmllint` `Unqualified access` / `Quick.layout-positioning` warnings — both pre-exist on `main` unchanged (same categories, only line numbers shift), not introduced by this diff.

## Summary

| Area | Verdict |
|---|---|
| Root cause diagnosis (`Loader.sourceComponent` context loss) | Correct, well-verified (standalone harness evidence in PR body) |
| Blank name/icon fix itself | Correct — `model.*` bindings now resolve on all five sections |
| Drag-reorder (`browser`/`container`/`pwa`/`custom`) | **At risk** — contradicts Kirigami's documented `ListItemDragHandle` requirement that the dragged item be a child of the real delegate, not the delegate itself |
| Search-filter collapse logic | Correctly relocated, no regression |
| Drag-while-filtered mitigation | Untouched, unaffected |
| Remaining `Loader`/`model` context issues elsewhere in file/repo | None found |
| `required property` mismatches | Not applicable — file doesn't use the pattern |
| CHANGELOG accuracy | Accurate, correct section |

**Not safe to merge as-is.** Before landing: manually verify drag-to-reorder still works on the browser, container, pwa, and custom sections (the one thing this diff couldn't be checked for statically and that Kirigami's own docs say this exact pattern breaks). If it's broken, the fix needs a thin per-row wrapper Item (not the removed `Loader`, since that reintroduces the context-loss bug) between the `ListView` and the dragged `ItemDelegate`, with `model.*` forwarded down via `required property` so both the model-context bug and the drag-handle contract are satisfied simultaneously.

---

# Round 2 — commit `f012972`, "Rework drag-reorder contract: wrapper Item + child ItemDelegate"

**Date:** 2026-09-14
**Commit:** `f012972` on top of `5eac43a` (`gh pr diff 6` now shows both commits against `main`).
**Scope:** `src/qml/pages/TargetsPage.qml` (four `Component`s — `browserDelegate`, `containerDelegate`, `pwaDelegate`, `customDelegate` — restructured; `privateDelegate` untouched by this commit), `CHANGELOG.md` (entry rewritten to describe the new shape), `docs/reviews/pr-6.md` added by the author (superseded by this file, the one actually gating merge).
**Method:** Re-read the full round-2 diff (`gh pr diff 6`, 710 lines) plus the resulting `TargetsPage.qml` in full for the four reworked sections. Re-verified against the same authoritative source used in round 1, `/usr/lib/qt6/qml/org/kde/kirigami/controls/ListItemDragHandle.qml`. Ran `qmllint` on the round-2 file and diffed its warning set against round 1's (same 144 warnings, same two categories — `Unqualified access` and one `Quick.layout-positioning` note — only line/column-shifted from the added nesting level; no new category, no new count). No build/run of the app (same constraint as round 1).

**Verdict: Approve — round-1 finding resolved. Safe to merge.**

## What changed

For the four draggable sections, each `Component`'s root is now a plain `Item` (`id: wrapper`) assigned directly as `ListView.delegate` — e.g. `browserDelegate` (`TargetsPage.qml:54-131`):

```qml
Component {
    id: browserDelegate
    Item {
        id: wrapper
        width: browserList.width
        height: page.matchesSearch(model.name, model.discoveredName) ? page.rowHeight : 0
        visible: height > 0
        QQC.ItemDelegate {
            id: listItem
            width: wrapper.width
            height: wrapper.height
            contentItem: RowLayout {
                ...
                Kirigami.ListItemDragHandle {
                    listItem: listItem
                    listView: browserList
                    ...
                }
                ...
            }
        }
    }
}
```

`ListView.delegate: browserDelegate` (`:485`, unchanged assignment — only the `Component`'s internal shape moved) is unchanged from round 1: the `Loader` stays gone, so the round-1 fix (model context resolves because a plain-assigned `Component` is the real per-row delegate) is preserved. `containerDelegate`, `pwaDelegate`, `customDelegate` follow the identical pattern (verified by reading all four in full). `privateDelegate` (`:337-360`) is untouched by this commit — still a direct `QQC.ItemDelegate` delegate, correctly so, since it has no `ListItemDragHandle` to satisfy a contract for.

## Does this satisfy the documented `ListItemDragHandle` contract? Yes.

Re-checked against `ListItemDragHandle.qml:22-23`'s requirement — "the listItem that is being dragged should not directly be the delegate of the ListView, but a child of it" — and the reparenting mechanics in `onPressed`/`dropped()` (`:191-202`, `:223-230`) that round 1 walked through:

- **`listItem` is now a child of `wrapper`, not the delegate itself.** `Kirigami.ListItemDragHandle { listItem: listItem; listView: browserList }` (`:67-68`) still points at the id `listItem` — but that id now names the inner `QQC.ItemDelegate`, a normal child of `wrapper`. `wrapper` is the object the `ListView` actually assigned as delegate (`delegate: browserDelegate` resolves to `Item wrapper` as root).
- **The real per-index object stays behind during a drag.** On `onPressed`, `root.listItem.parent = root.listView` moves the inner `ItemDelegate` out to the `ListView` for the drag visual; `wrapper` — the object the `ListView`'s positioner actually tracks and sizes for that model index — stays put, still reporting its `height` (`page.rowHeight` while not filtered), still reserving the row's layout slot. This is functionally identical to the old `Loader`-as-delegate arrangement round 1 cited from Kirigami's own example (`Loader` stayed behind; its `sourceComponent`-instantiated child was what got reparented) — same structural shape, `Item` in place of `Loader`, same reason for existing.
- **On drop, `internal.originalParent` is `wrapper`,** so `root.listItem.parent = internal.originalParent` (`dropped()`) reparents the `ItemDelegate` straight back into the same `wrapper` that never left the `ListView`'s content item. No gap, no orphaned placeholder, no double-tracked object.
- **Model context is preserved independently of the reparenting.** `wrapper` is a plain `Item`, not a `Loader`, so its children (the inner `ItemDelegate`, and everything inside it — `Kirigami.Icon { source: model.iconName }`, `QQC.TextField { text: model.name }`, etc.) inherit the enclosing `QQmlContext`'s `model`/`index` properties through ordinary QML object-tree scoping, the same way any nested `Item`/`RowLayout`/`Rectangle` always has — this is not the `Loader.sourceComponent` boundary round 1 flagged, which is specific to `Loader` re-instantiating a `Component` in an isolated context. Verified end-to-end: every `model.*` reference in all four reworked delegates (icon, text field, switch, buttons, `onEditingFinished`, `Keys.onEscapePressed`) is textually unchanged from round 1's already-correct (for the model-context bug) version — only its enclosing wrapper changed, not any binding expression.

This is the exact resolution round 1 recommended: *"a thin per-row wrapper Item (not the removed Loader...) with model.* forwarded down... so both the model-context bug and the drag-handle contract are satisfied simultaneously."* The only difference from that recommendation's literal wording is that no `required property` forwarding was needed — `model.*` propagates to plain-nested children for free without it, since `required property` is only needed when a *sibling* `Component`/`Loader` boundary would otherwise block it, which a plain child `Item` never does.

## Requested check: `implicitHeight`/`implicitWidth` on the wrapper — sizes correctly, no gap found

`wrapper` does **not** use `implicitHeight`/`implicitWidth` at all — it sets explicit `width: browserList.width` and `height: page.matchesSearch(...) ? page.rowHeight : 0` directly (`:58-59`, and the equivalent lines in the other three sections). This is correct and sufficient: `ListView` positions delegates using each delegate's actual `height` property (the value read at layout time), not `implicitHeight` — `implicitHeight` only matters as a *fallback* for items that don't set `height` explicitly, which isn't the case here. Since `wrapper.height` is always bound to a concrete expression (`0` or `page.rowHeight`), the `ListView`'s row positions and `contentHeight` (used by `implicitHeight: contentHeight` on each `ListView`, e.g. `:487`) compute correctly regardless of `implicitHeight` being unset on `wrapper`.

The inner `ItemDelegate` (`listItem`) forwards size explicitly too — `width: wrapper.width; height: wrapper.height` (`:63-64`) — rather than relying on its own `Control`-derived implicit sizing (padding + content-driven `implicitWidth`/`implicitHeight`, which `QQC.ItemDelegate` does compute internally but which is irrelevant here since explicit `width`/`height` bindings always take priority over implicit ones in QtQuick). Both bindings are live property bindings (not one-time), so `listItem` continues to track `wrapper`'s size correctly even while reparented mid-drag — reparenting an `Item` doesn't invalidate its property bindings, which are structural, not parent-relative. Verified no size mismatch case: `wrapper`'s width/height and `listItem`'s forwarded width/height are always identical by construction (same expression, one hop removed), so the drag visual, the collapsed (filtered-out) height-0 state, and the normal 48px row all size identically to round 1's directly-set values — this restructuring is a pure indirection, not a new sizing rule.

**No implicit-sizing gap found.** The one pre-existing `qmllint` note about layout-managed `width` (`Quick.layout-positioning`, on the *private* section's unrelated `Item { width: Kirigami.Units.iconSizes.smallMedium }` spacer, `:343` post-round-2) is untouched by this commit and unrelated to the wrapper/child sizing checked here.

## Anything else regressed?

- **`qmllint` diff (round 1 → round 2): zero new warnings, zero new categories.** Same 144 total warnings both before and after this commit; every diff line is a pure line/column shift from the added nesting level (`Item wrapper { ... ItemDelegate listItem { ... } }` instead of `ItemDelegate listItem { ... }` directly). No new "unqualified access" appears for `wrapper.width`/`wrapper.height` (both resolve as ordinary sibling-scope id lookups, no `pragma ComponentBehavior: Bound` needed any more than round 1 needed it).
- **Drag-reorder-while-filtered mitigation untouched.** `enabled: page.searchText.trim().length === 0` and its explanatory comment on each `ListItemDragHandle` (`:78-89` etc.) are carried over verbatim from round 1 into the new nesting — not touched by this commit, still correctly gates the handle off during an active search.
- **`syncModels()`, `matchCount()`, `matchesSearch()`, and the five `ListView` declarations themselves are untouched** by this commit — confirmed via the diff, which only rewrites the four `Component` bodies and the corresponding `CHANGELOG.md` bullet.
- **CHANGELOG entry updated and accurate.** The rewritten bullet (`CHANGELOG.md:195-213` post-round-2) correctly describes the new wrapper-Item shape, explicitly names that `ListItemDragHandle.listItem` still points at the inner `ItemDelegate` (not the wrapper), and calls out that this matches "`ListItemDragHandle`'s documented contract" — an accurate, specific claim, not hand-waving.

## Summary (round 2)

| Area | Verdict |
|---|---|
| Round-1 finding (drag-handle contract violation) | **Resolved** — wrapper `Item` is the real delegate, `ItemDelegate` is its child, matches Kirigami's documented shape |
| Model-context propagation to the wrapper's child | Correct — plain child scoping, no `Loader` boundary re-introduced |
| `implicitHeight`/`implicitWidth` forwarding (requested check) | N/A by design — explicit `width`/`height` bindings used throughout, which is what `ListView` actually reads; no sizing gap |
| `qmllint` regression check | Clean — identical warning set to round 1, line-shifted only |
| Drag-while-filtered mitigation | Untouched, intact |
| CHANGELOG accuracy | Accurate, correctly describes the new shape and cites the satisfied contract |

**Safe to merge.** The round-1 blocking concern is resolved by construction, not by assertion: the new shape is structurally the same pattern Kirigami's own `ListItemDragHandle` documentation prescribes, just with a plain `Item` in place of the `Loader` that caused the original bug — which is exactly what avoids reintroducing the model-context defect while satisfying the drag-handle contract. Recommend the same light-touch follow-up as round 1 would have: an actual manual drag-and-drop pass on the four sections before/shortly after merge as a sanity check, since the repo still has no automated QML interaction test to encode this contract going forward — but this is a suggestion for future hardening, not a merge blocker.
