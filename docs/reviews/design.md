# Design review: Lane picker, hold, and settings

Reviewed from `src/qml/**/*.qml`, `src/app/Controller.{h,cpp}`, `src/app/{PickerModel,TargetModel,RuleModel}.*`, `src/core/types.h`, `DESIGN.md`, and stale `docs/screenshots/{picker,hold,settings}.png` (pre-rename "Tern", pre-section-headers picker, pre-search Targets page). No live overlay or running daemon.

---

## Scores

| Dimension | Score | What a 10 looks like |
|-----------|------:|----------------------|
| First 200ms | 5 | Rule matches and post-training remembered opens are invisible; hold veto exists only for people who opt in, with a sub-400ms default |
| Picker density | 7 | Forty-plus targets stay scannable without typing: grouped sections, sane viewport math, filter finds any row in two keystrokes |
| Keyboard model | 8 | Every binding works from the focused filter field and is labeled in the footer; path ladder keys are discoverable without reading source |
| The Always checkbox | 6 | Label states exact scope and chosen target; regret is one action from the picker, hold HUD, or toast |
| Settings IA | 7 | Targets are browsable at scale via search and collapsible sections; reorder cannot surprise you under a filter |
| Empty and error states | 8 | Missing browser, failed launch, blocked URL, and broken rules/remembered entries all say what happened and what to do |
| Visual identity | 8 | Reads as native Plasma: Kirigami card, accent selection, light tint; not a generic Qt dialog |
| Accessibility | 6 | Overlays and settings forms expose roles/names; sidebar nav and focus order are complete for keyboard and AT |

---

## The one thing

**Default `holdAutoOpen` to off, or cut default `holdMs` from 1600 to about 400.**

`holdAutoOpen` is still `true` by default (`src/core/types.h:150`). `shouldHold()` (`Controller.cpp:797-804`) runs the hold HUD on every remembered, PWA, and default open. With ~45 targets and trained memory, that is dozens of 1.6s interruptions per day. Preferences now exposes duration (`PreferencesPage.qml:54-65`), but most people never open Preferences for a daemon that "just works." DESIGN.md line 5 promises "either nothing visible"; the hold section documents the veto, but the default optimizes first-time safety over the trained path.

**Files:** `src/core/types.h:150` (`holdAutoOpen = false` or `holdMs = 400`).

---

## Blocking

Nothing new blocks shipping to the owner on Plasma 6.7.4. The hold default is a daily-friction product call, not a broken surface.

---

## Should fix

1. **Picker list height over-counts section headers.** `Picker.qml:186-187` adds `controller.pickerModel.sectionCount * sectionHeaderHeight` for every section in the filtered model (up to five), not for headers visible inside the eight-row viewport. With mixed sections this can leave dead space at the bottom of the clipped list or size the card taller than the rows actually shown. Count headers among the visible row budget, or cap header allowance to what fits in `maxRows`.

2. **Comma/period ladder still invisible.** `Picker.qml:164-174` handles `,` and `.` in the filter field, but the footer (`Picker.qml:324-348`) only shows `‹` / `›` chevrons and a 120px-elided key. Add `,` / `.` hints next to the ladder, or tooltips on the chevrons.

3. **Always checkbox omits the chosen target.** `Picker.qml:318-320` labels `"Always for " + controller.currentDestinationKey` but not the highlighted row's `name`. A path-scoped key like `github.com/bitskc/repo` does not say *where* links open. Append the current list selection: "Always open this path in Zen · Work."

4. **Regret path for Always is still a settings dig.** `pickId()` writes remembered (`Controller.cpp:381-384`); undo is tray → Settings → Rules → find host → delete (`RulesPage.qml:117-127`). Offer "Undo" on the post-open toast (`Controller.cpp:704-712`) or a forget action on the hold HUD.

5. **Settings sidebar has no accessibility names.** `Settings.qml:55-97` nav `ListView` delegates lack `Accessible.*`. Picker and hold are labeled; the settings shell is not. Add `Accessible.role` / `Accessible.name` on nav items and mark the active page.

6. **`DESIGN.md` drift.** Doc still says six visible rows and blur (`DESIGN.md:14-15`); code shows eight rows (`Picker.qml:22`) and a tint without blur (`CHANGELOG.md` "Overlay tints the desktop"). Align doc or accept intentional drift explicitly.

7. **Stale screenshots.** `docs/screenshots/*.png` still show "Tern", a six-row picker without section headers, and a flat Targets list without search/collapse. Refresh after the next visual pass so reviews do not argue against ghosts.

---

## Follow-ups

1. **Per-row Default buttons on Targets.** `TargetsPage.qml:117-122` (and container rows) duplicate the global "Fallback target" combo (`TargetsPage.qml:415-420`). Fine at six targets; noisy at forty-five.

2. **Container rows without mapped color.** Neutral dot fallback is correct (`Picker.qml:254-268`); subtitle and `Accessible.description` carry kind. Consider a "Container" text badge when `colorName` is empty so shape-blind users do not rely on dot alone.

3. **Blocked URL is notification-only.** `applyDecision()` calls `notifyBlocked()` (`Controller.cpp:605-610`) with no in-overlay explanation. Acceptable for v0.1; a one-line banner in a future picker-empty state would help when notification permissions are off.

4. **Settings minimum size at scale.** `Settings.qml:9-10` (`880×560`) is tight when Containers and PWAs are both expanded with search cleared. Not broken with collapse defaults (`TargetsPage.qml:14-18`); watch on 720p laptops.

5. **Picker screenshot density at 45 targets.** Eight rows plus up to five section headers in height math means filter is the real navigation path. "Recent targets" pin above sections would reduce typing for power users.

---

## What landed since last review

- Picker section headers and stable kind grouping (`PickerModel.cpp:97-147`, `Picker.qml:198-214`).
- Dynamic `sectionCount` replaces hardcoded header guess (`PickerModel.h:14-19`, `Picker.qml:187`).
- Container color dots with neutral fallback for unmapped colors (`Picker.qml:254-268`, `PickerModel.cpp:42-43`).
- `holdMs` slider 0.4-5s in Preferences (`PreferencesPage.qml:54-65`).
- Picker/hold `Accessible` roles and row labels (`Picker.qml:72-76`, `233-235`; `Hold.qml:58-60`).
- Targets search field and collapsible sections with counts (`TargetsPage.qml:394-409`, `424-548`).
- Drag reorder disabled while search filter is active (`TargetsPage.qml:65-73`).
- Rules warn on missing rule targets and offer bulk clear for dangling remembered (`RulesPage.qml:39-79`, `130-137`).
- Launch failure and blocked-link notifications (`Controller.cpp:656-662`, `715-722`).
- Custom app add shows inline command errors (`TargetsPage.qml:654-665`).
- Number keys 1-8, comma, period, and Ctrl+C work while filter is focused (`Picker.qml:147-175`).
- Copy link moved to footer (`Picker.qml:350-385`); no longer steals a row or shortcut.

---

## Considered and fine

1. **Resident daemon + layer-shell overlays.** `Picker.qml` / `Hold.qml` use `LayerShell.Window` with exclusive keyboard. Correct for Wayland.
2. **Light tint, not heavy dim.** Picker `dim` at 12% (`Picker.qml:47`); hold at 8% (`Hold.qml:32`). Matches "quiet overlay."
3. **Rules skip hold.** `shouldHold()` excludes rule matches (`Controller.cpp:802-804`). Explicit rules launch immediately.
4. **Checkbox for Always.** `Picker.qml:314-321` uses `QQC.CheckBox`, not a switch. Matches DESIGN.md.
5. **Filter auto-focused on show.** `Picker.qml:26-33` clears filter and focuses the field. Right default for large target lists.
6. **Destination ladder in footer.** `currentDestinationKey` plus chevrons gives path scope without opening settings; only needs keyboard labels.
7. **Two-pane settings, no hamburger.** `Settings.qml` fixed 220px nav and four pages. Shell is stable; page content was the problem and is much improved.
8. **Container discovery gated on protocol handler.** CHANGELOG and `TargetsPage.qml:489-498` explain the extension requirement. Avoids dead container rows.
9. **Kirigami card treatment.** `ShadowedRectangle`, highlight selection, monospace shortcut badges. Plasma-native in screenshots and source.
10. **Global fallback target combo.** Single default picker at `TargetsPage.qml:415-420` is clearer than per-row Default at scale (per-row buttons are the leftover noise).

---

## Method

1. Ran `gstack-skill-start --skill plan-design-review` (SESSION_ID `3239336-1789189764-a24f48b0`, TEL_START `1789189764`).
2. Read `plan-design-review/SKILL.md`; adapted plan-review dimensions to shipped QML at HEAD.
3. Read all QML under `src/qml/`, Controller surface, models, and `DESIGN.md`.
4. Could not see live UI. Judged layout from source plus stale `docs/screenshots/*.png` (Tern branding, old picker row count, no section headers, flat Targets page). Stated limitations here.
5. Re-scored eight dimensions from the prior review table; verified each CHANGELOG claim in source.
