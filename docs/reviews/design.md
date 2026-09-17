# Design review: Lane picker, hold, and settings (round 2)

Reviewed from `src/qml/**/*.qml`, `src/app/{Controller,PickerModel}.*`, `src/core/types.h`, `DESIGN.md`, `README.md`, `CHANGELOG.md`, and `docs/screenshots/{picker,hold,settings}.png` (dated 2026-09-10, still pre-rename). No live overlay or running daemon.

**Verdict: Ship for daily use on your own machine; hold default-off and Targets polish landed. Before pointing a stranger at the README, refresh screenshots and fix the picker filter empty state plus the hold-default doc drift.**

---

## Scorecard

| Dimension | R1 | R2 | What a 10 looks like |
|-----------|---:|---:|----------------------|
| First 200ms | 5 | **8** | Remembered opens are instant by default; hold is opt-in for people who want a veto window |
| Picker density | 7 | 7 | Forty-plus targets stay scannable without typing: grouped sections, sane viewport math, filter finds any row in two keystrokes |
| Keyboard model | 8 | **9** | Every binding works from the focused filter field and is labeled in the footer; path ladder keys are discoverable without reading source |
| The Always checkbox | 6 | **8** | Label states exact scope and chosen target; regret is one action from the picker, hold HUD, or toast |
| Settings IA | 7 | **8** | Targets are browsable at scale via search and collapsible sections; reorder cannot surprise you under a filter |
| Empty and error states | 8 | 7 | Missing browser, failed launch, blocked URL, and broken rules/remembered entries all say what happened and what to do |
| Visual identity | 8 | 7 | Reads as native Plasma in the app; marketing assets match what ships |
| Accessibility | 6 | **7** | Overlays and settings forms expose roles/names; sidebar nav and focus order are complete for keyboard and AT |

**Net: +5 points across eight dimensions.** Biggest gain is First 200ms (hold default flipped). Biggest loss is Visual identity / empty states (stale screenshots; picker filter miss).

---

## Round-1 fix verification

| # | Round-1 finding | Status | Evidence |
|---|-----------------|--------|----------|
| 1 | `holdAutoOpen` defaulted true at 1600ms | **Fixed** | `src/core/types.h:149` (`holdAutoOpen = false`). `PreferencesPage.qml:48-52` exposes opt-in switch. `CHANGELOG.md:81-83` documents the change. |
| 2 | Comma/period ladder keys invisible in footer | **Fixed** | `Picker.qml:412-426` footer shows `. widen` and `, narrow` with a comment tying them to `Keys.onPressed` at `164-174`. |
| 3 | Always checkbox omits chosen target name | **Fixed** | `Picker.qml:323-331` appends `in ` + `list.currentItem.name` when a row is highlighted. |
| 4 | Settings sidebar nav lacks Accessible names | **Fixed** | `Settings.qml:77-79` sets `Accessible.role`, `Accessible.name`, and `Accessible.selected` on each nav delegate. |
| 5 | `DESIGN.md` drift (6 rows + blur vs 8 rows + tint) | **Partially fixed** | Row count and tint match code: `DESIGN.md:14-15` (eight rows, dim tint). **Still wrong on hold default:** `DESIGN.md:44-46` says `holdAutoOpen` is on by default; code and CHANGELOG say off. |
| 6 | Stale screenshots show "Tern" branding | **Never fixed** | `docs/screenshots/*.png` last modified 2026-09-10. Picker image shows six rows, no section headers, "Tern" title. Settings image shows flat list, no search, "Tern" sidebar, six targets. README still embeds these at `README.md:14-21`. |

**Regressions: none** on the six round-1 items. Item 5 and 6 are doc/asset debt, not code regressions.

---

## Findings (by severity)

### Should fix

1. **Picker filter with zero matches shows a blank row, no explanation.** `Picker.qml:186-187` sets list height to `Math.max(1, count) * rowHeight` even when `count === 0`, so a typo leaves a dead 48px strip and Enter does nothing useful. Add a "No matching destinations" label when the filtered model is empty, and drop the fake row height.

2. **Section header height still over-counts.** `Picker.qml:186-187` adds `sectionCount * sectionHeaderHeight` for every section in the filtered model (up to five), not for headers visible inside the eight-row viewport. Mixed sections can leave dead space at the bottom of the clipped list or size the card taller than the rows shown. Same finding as round 1; unchanged.

3. **`DESIGN.md` and README still describe hold as default-on.** `DESIGN.md:44-46` documents `holdAutoOpen` on by default. `README.md:47-49` says remembered/PWA/default opens "shows a short hold bar" without noting opt-in first (the tutorial section at `101-102` mentions turning it off, but the product summary does not). New readers will expect a HUD they will not see until they enable Preferences.

4. **README screenshots are a lie relative to v0.2.0.** Files on disk (`docs/screenshots/picker.png`, `hold.png`, `settings.png`) predate Lane rename, eight-row picker, section headers, collapsible Targets, and the update checker block. First-time users judge the product from these images before install.

5. **Regret path for Always is still a settings dig.** `Controller::pickId()` writes remembered at `Controller.cpp:418-421`; undo is tray, Settings, Rules, find host, delete (`RulesPage.qml:117-127`). No undo on the post-open toast (`Controller.cpp:796-804`). Round-1 carry; still true.

### Follow-ups

6. **Targets search with no hits is silent.** Section headers show `(0)` counts (`TargetsPage.qml:463`, `506`, etc.) but there is no page-level "Nothing matched" when every `matchCount` is zero. Not broken, just confusing during a failed search.

7. **Per-row Default buttons on browser and container rows.** `TargetsPage.qml:122-127` and `193-198` duplicate the global "Fallback target" combo at `440-446`. Fine at six targets; noisy at forty-five. Round-1 carry.

8. **Section collapse toggles on Targets lack accessibility metadata.** Chevron `ToolButton`s at `TargetsPage.qml:455-458` (and siblings for Containers, PWAs, Private, Custom) have no `Accessible.name` or expanded state. Keyboard users can activate them, but screen readers get unnamed buttons.

9. **Inline rename fields are invisible until you click.** Hint text at `TargetsPage.qml:468` ("Click a name to rename") helps, but bare `TextField`s with `background: Item {}` look like static labels. Consider a subtle underline on hover or an edit affordance icon.

10. **Ladder chevron buttons still have no tooltips or Accessible names.** `Picker.qml:340-362` exposes mouse affordance only; footer hints cover comma/period keys but not the chevrons.

11. **Picker section headers are visual only.** `Picker.qml:200-214` section delegates have no `Accessible` role; AT users hear a flat destination list without group context.

12. **Blocked URL is notification-only.** `Controller.cpp:697-703` calls `notifyBlocked()` with no in-overlay copy. Acceptable when notifications work; invisible when the user has disabled Lane toasts.

### New since round 1 (looked good, minor notes)

- **Drag-reorder + inline rename on Targets** (`TargetsPage.qml:54-131`, Loader-to-Item wrapper): drag disabled while search is active (`78`, `149`), which avoids the filtered-index trap. Solid.
- **Collapsible sections with counts**: Private starts collapsed (`TargetsPage.qml:17`), which keeps first open manageable.
- **Update checker UI** (`OverviewPage.qml:87-117`): clear states (checking, up-to-date, update-available, failed), manual-only, no background nag. Matches CHANGELOG promise.
- **Flatpak discovery**: backend-only in Unreleased CHANGELOG; no new settings surface needed for v0.2.0 UX.
- **Hold HUD default-off**: `shouldHold()` at `Controller.cpp:893-900` gates on `holdAutoOpen`; rules still skip hold. Behavior matches the new default.

---

## Considered and fine

1. **Resident daemon + layer-shell overlays.** `Picker.qml` / `Hold.qml` use `LayerShell.Window` with exclusive keyboard. Correct for Wayland.
2. **Light tint, not heavy dim.** Picker `dim` at 12% (`Picker.qml:47`); hold at 8% (`Hold.qml:32`). Matches DESIGN.md "quiet overlay."
3. **Rules skip hold.** `shouldHold()` excludes rule matches (`Controller.cpp:898-900`). Explicit rules launch immediately.
4. **Checkbox for Always.** `Picker.qml:319-337` uses `QQC.CheckBox`, not a switch. Matches DESIGN.md.
5. **Filter auto-focused on show.** `Picker.qml:26-33` clears filter and focuses the field. Right default for large target lists.
6. **Destination ladder in footer.** `currentDestinationKey` plus chevrons and comma/period hints give path scope without opening settings.
7. **Two-pane settings, no hamburger.** `Settings.qml` fixed 220px nav and four pages. Shell is stable; Targets content scaled well.
8. **Container discovery gated on protocol handler.** `TargetsPage.qml:509-518` explains the extension requirement. Avoids dead container rows.
9. **Kirigami card treatment.** `ShadowedRectangle`, highlight selection, monospace shortcut badges. Plasma-native in source (not in stale PNGs).
10. **Global fallback target combo.** Single default at `TargetsPage.qml:440-446` is clearer than per-row Default at scale (per-row buttons are the leftover noise).
11. **Custom app errors inline.** `TargetsPage.qml:654-665` surfaces command rejection in the add form.
12. **Dangling rule/remembered warnings.** `RulesPage.qml:60-79`, `130-137` with bulk clear. Nothing auto-deleted.
13. **Copy link in footer.** `Picker.qml:369-404` with Ctrl+C from filter field (`147-151`). No longer steals a row or number key.
14. **Hold overlay accessibility.** `Hold.qml:58-60` names target and documents Enter/Esc/Space. Progress bar is visual-only, which is fine for a 0.4-5s veto.

---

## Method

1. Ran `gstack-skill-start --skill plan-design-review` (SESSION_ID `777869-1789660303-fac0d072`, TEL_START `1789660303`).
2. Read round-1 `docs/reviews/design.md` and `docs/designs/gstack-full-analysis.md` for prior scores and claimed fixes.
3. Read all QML under `src/qml/`, Controller hold/picker paths, PickerModel section logic, `DESIGN.md`, `README.md`, `CHANGELOG.md`.
4. Opened `docs/screenshots/*.png` to verify branding, row count, and section headers against current source. Stated limitations: no live UI, no daemon.
5. Re-scored eight dimensions; verified each round-1 claim against file:line evidence rather than CHANGELOG alone.
