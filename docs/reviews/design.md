# Design review: Tern picker, hold, and settings

Reviewed from `src/qml/*.qml`, `src/app/Controller.cpp`, `src/core/*.cpp`, and `docs/screenshots/{picker,hold,settings}.png`. No live overlay injection.

---

## Scores

| Dimension | Score | What a 10 looks like |
|-----------|------:|----------------------|
| First 200ms | 6 | Resident daemon, rule matches open instantly with zero chrome; remembered and default opens are invisible unless you explicitly want a veto window |
| Picker density | 5 | Forty-plus targets stay scannable: grouped sections, filter finds any row in two keystrokes, scroll position is obvious |
| Keyboard model | 7 | Every binding is either muscle memory or labeled in the footer; path scope and Always are discoverable without reading source |
| The Always checkbox | 6 | Label states exact scope and target; regret is one undo from the picker or tray, not a settings dig |
| Settings IA | 4 | Targets are browsable at scale: search, collapse, or split by kind without a 2000px scroll |
| Empty and error states | 5 | Missing browser, failed launch, and blocked URL all tell you what happened and what to do next |
| Visual identity | 8 | Reads as native Plasma: accent, blur/tint, Kirigami card, icon; not a generic Qt dialog |
| Accessibility | 3 | Overlay is fully navigable by keyboard and AT: labeled rows, focus order, contrast-safe tint, non-color cues for containers |

---

## The one thing

**Default `holdAutoOpen` to off, or cut `holdMs` from 1600 to ~350.**

The hold HUD (`Hold.qml`) runs on every remembered, PWA, and default open when `holdAutoOpen` is true (the default in `types.h`). That is the opposite of DESIGN.md line 5: "either nothing visible (correct app already opened)". A person who has trained sixteen containers will hit the hold bar dozens of times a day. Enter/esc help power users but everyone else waits 1.6 seconds staring at `QQC.ProgressBar` in `Hold.qml` with no setting to shorten the duration (`holdMs` exists in config but has no UI in `PreferencesPage.qml`).

**File:** `src/core/types.h` — change `holdAutoOpen` default to `false`, or `holdMs` to `350`. Secondary: expose `holdMs` in `PreferencesPage.qml` if the hold stays on by default.

---

## Blocking

1. **Silent launch failure.** `launchTarget()` in `launcher.cpp` returns false when the executable is missing; `Controller::launch()` returns without notification. The link does nothing. A vanished browser or broken custom command looks like Tern is broken.

2. **Hold on the trained happy path.** With `holdAutoOpen` default true, the product's core promise (invisible routing after memory) is violated on every remembered open. Esc-to-picker is a recovery path, not a primary flow.

3. **No accessibility tree on overlays.** `Picker.qml` and `Hold.qml` have zero `Accessible` properties. Screen readers on Wayland get an unnamed fullscreen window with no row labels. This is blocking for anyone relying on AT.

---

## Should fix

1. **Picker survives 44 targets only via filter; numbers die at 9.** `PickerModel` assigns shortcuts 1-9 by row index (`ShortcutRole`). Rows 10-44 need scroll plus filter. With sixteen Zen containers sharing the same `iconName` and subtitle pattern (`Zen · Banking`), visual discrimination is name-only. Add section headers (Browsers / Containers / Apps) in `Picker.qml` and `rankForPicker()`, or pin recent targets above the fold.

2. **Comma and period path scope is unlearnable from the UI.** `Picker.qml` wires `,` and `.` shortcuts (lines 42-51) but the footer only shows `‹` / `›` `ToolButton`s and `currentDestinationKey`. No hint that comma widens and period narrows. Add footer labels next to the ladder control, or tooltips on the chevrons.

3. **Always checkbox scope is ambiguous.** `alwaysBox` text is `"Always for " + controller.currentDestinationKey`. When the ladder is at `github.com/bitskc`, a person may think they are committing the whole host, not a path-scoped key. The checkbox should read something like "Always open this path in [target]" and show the picked row's name, not just the key.

4. **Undo Always costs too many clicks.** Regret path: open Settings (`Settings.qml`), nav to Rules (`RulesPage.qml`), find host in "Remembered destinations", click delete (`forgetHost`). That is tray → settings → rules → scroll → delete. Offer "Undo" on the post-open toast (`Controller::toast`) or a one-click forget on the hold HUD.

5. **`TargetsPage.qml` does not scale.** Five `ListView`s (Browsers, Containers, Installed web apps, Private windows, Custom apps) each at `rowHeight: 48` with drag handle, rename field, hide switch, and sometimes Default. Sixteen containers alone are 768px before PWAs and browsers. **Proposed fix:** split "Browsers & apps" into a searchable `Kirigami.SearchField` at the top of `TargetsPage.qml`, collapse each section with `Kirigami.CollapsibleCard` (Containers expanded by default, Private windows collapsed), and move Containers to their own nav item if count exceeds ~8. Drop per-row Default buttons; one global default combo already exists at the top.

6. **Custom app add failures are invisible.** `addCustomTarget()` logs `qWarning` and returns. `TargetsPage.qml` `FormButtonDelegate` "Add custom app" gives no inline error on rejected commands.

7. **Blocked URL shows picker, not explanation.** `route()` sets `reason: "blocked"` and `applyDecision` shows the picker. The person sees a normal picker for a `javascript:` link with no banner. `Controller::launch()` does notify for unsafe opens on launch path, but blocked routing never reaches that.

8. **Expose `holdMs` in preferences** if hold stays default-on. `PreferencesPage.qml` has the toggle but not the duration slider.

9. **Container color is stored but never shown.** `Target.color` from `containerColor()` in `discovery.cpp` is not exposed through `PickerModel` or `TargetModel::targetsByKind`. Unmapped colors (e.g. cyan) return invalid `QColor` and matter only if you add a color dot to the picker row. Today they are invisible, so the cyan gap is latent, not user-visible.

10. **DESIGN.md drift.** Doc says six visible rows; `Picker.qml` `maxRows: 8`. Screenshot `picker.png` shows six. Align doc or screenshot after the density change settles.

---

## Right as-is

1. **Resident daemon + layer-shell overlay.** `Picker.qml` / `Hold.qml` use `LayerShell.Window` with exclusive keyboard. Cold-start avoided. Correct architecture for Wayland.

2. **12% tint instead of heavy dim.** `Picker.qml` `dim` rectangle at `Qt.rgba(0,0,0,0.12)` keeps context visible. Hold uses 8%. Matches "quiet overlay" intent better than the old heavy dim in the screenshot.

3. **Rules skip hold.** `shouldHold()` only fires for `remembered`, `pwa`, `default`. Rule matches launch immediately. Respects explicit user intent.

4. **Checkbox for Always, not a switch.** Matches DESIGN.md and reads as a commitment, not a mode toggle.

5. **Filter field auto-focused on show.** `onVisibleChanged` clears filter and `forceActiveFocus()` on `filterField`. Right for type-to-filter with sixteen new container names.

6. **Destination ladder with chevrons.** `currentDestinationKey` in the footer plus `‹`/`›` buttons gives path scope without opening settings. Power feature; just needs keyboard hints.

7. **Two-pane settings without hamburger.** `Settings.qml` fixed 220px nav, `StackLayout` for pages. Stable IA shell; the problem is page content length, not nav count. (Nav is four items today: Overview, Browsers & apps, Rules, Preferences. No About page in tree.)

8. **Container discovery gated on protocol handler.** `geckoContainers()` in `discovery.cpp` checks `extensions.json` before listing containers. Avoids offering sixteen dead targets that open blank tabs. Good product call even though it complicates the empty state.

9. **Kirigami card treatment.** `ShadowedRectangle`, `highlightColor` selection in list delegate, monospace shortcut badges. Reads Plasma-native in screenshots.

10. **`containerColor()` deliberate gaps.** Leaving unmapped Firefox color names as default `QColor` avoids wrong guesses. Fine until picker rows show color dots; then add cyan or fall back to a neutral ring plus container name.

---

## Notes by surface

### Picker (`Picker.qml`)

Card is 440px wide. List height is `min(8, count) * 48` = up to 384px of rows plus filter (36px), header, footer. Total card ~500px tall with eight rows. Acceptable on 1080p; tight on 720p laptops.

Footer row packs `alwaysBox`, ladder chevrons, and `esc` hint into one `RowLayout`. At narrow widths `currentDestinationKey` elides at 120px fixed width, which truncates long path keys.

Number shortcuts disabled when `filterField.text.length > 0`. Typing "per" to find Personal disables 1-9. Correct tradeoff; filter becomes the only path.

### Hold (`Hold.qml`)

360px card, `QQC.ProgressBar` bound to `controller.holdProgress`. Copy is clear ("Opening in …", enter now / esc pick instead). Problem is duration and default-on, not layout.

Clicking the dimmed background calls `cancelHold()` which opens the picker. Good escape hatch.

### Settings (`TargetsPage.qml`)

Inline rename via `QQC.TextField` with `onEditingFinished` and 1ms `Timer` debounce to `renameTarget`. Drag reorder per section via `Kirigami.ListItemDragHandle`. Works for six targets; at forty-four the page is a configuration spreadsheet.

Empty container message (line 429-438) only shows when `hasGeckoBrowsers()` and count is zero. Good. No equivalent for empty PWA list or missing default browser target.

### Remembered destinations (`RulesPage.qml`)

`forgetHost` is one click per entry with delete icon. Clear. Just buried in settings.
