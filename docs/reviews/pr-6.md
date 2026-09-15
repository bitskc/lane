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
