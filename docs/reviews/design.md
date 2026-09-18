# Design review: Lane picker, hold, and settings (round 3)

Reviewed from `src/qml/**/*.qml`, `src/app/{Controller,PickerModel}.*`, `src/core/router.cpp`, `DESIGN.md`, `README.md`, and `docs/screenshots/{picker,hold,settings}.png` (still dated 2026-09-10). No live overlay or running daemon.

**Verdict: Daily-driver ready for a power user who configured targets once. Round 2 polish mostly landed. Before sharing the repo, refresh screenshots. At 40+ targets, fixed section bucketing still buries the ranked suggestion below less relevant rows.**

---

## Scorecard

| Dimension | R2 | R3 | What a 10 looks like |
|-----------|---:|---:|----------------------|
| First 200ms | 8 | **8** | Silent opens stay instant; hold is opt-in and skippable |
| Picker density | 7 | **6** | Forty-plus targets scannable without typing; ranked suggestion visible in the first row |
| Keyboard model | 9 | **8** | Every binding labeled in the footer; no hidden shortcuts |
| The Always checkbox | 8 | **8** | Label states scope and target; regret is one action away |
| Settings IA | 8 | **7** | Picker section order matches how targets are configured |
| Empty and error states | 7 | **6** | Zero/hidden targets tell the user what to do next |
| Visual identity | 7 | **5** | Marketing assets match what ships |
| Accessibility | 7 | **8** | Overlays and settings expose roles/names; remaining gaps are minor |

**Net: -4 on picker density and marketing assets; +1 on accessibility from round 2 fixes.**

---

## Round-2 fix verification

| # | Round-2 finding | Status | Evidence |
|---|-----------------|--------|----------|
| 1 | Filter miss shows blank row | **Fixed** | `Picker.qml:197-212` uses `list.count === 0` height, shows "No matching destinations" with `Accessible.name` |
| 2 | Section header height over-counts | **Fixed** | `Picker.qml:20-36` `visibleSectionCount()` counts only headers inside the eight-row viewport |
| 3 | `DESIGN.md` hold default drift | **Fixed** | `DESIGN.md:45` says "off by default"; matches `Controller.cpp:917-924` |
| 4 | Stale README screenshots | **Never fixed** | `docs/screenshots/*.png` mtime 2026-09-10; README embeds at `README.md:14-21` |
| 5 | Regret path for Always is a settings dig | **Still true** | `Controller.cpp:433-436` writes remembered; undo only via `RulesPage.qml:117-127` or tray |
| 6 | Targets search silent on zero hits | **Fixed** | `TargetsPage.qml:503-514` page-level "No matches" label |
| 7 | Per-row Default buttons duplicate global fallback | **Still true** | `TargetsPage.qml:137-148`, `224-235` vs `516-526` |
| 8 | Section collapse toggles unlabeled | **Fixed** | `TargetsPage.qml:540-541` and siblings set `Accessible.name` with expand/collapse text |
| 9 | Ladder chevrons unlabeled | **Fixed** | `Picker.qml:383-384`, `401-402` `Accessible.name` on widen/narrow |
| 10 | Picker section headers visual only | **Fixed** | `Picker.qml:245-246` `Accessible.name` on section delegate |
| 11 | Default button a11y | **Fixed** | `TargetsPage.qml:141-145` `Accessible.name` + `ToolTip` |

**Regressions: none** on verified round-2 items.

---

## Findings (by user impact)

### Should fix

1. **README screenshots still misrepresent the product.** `docs/screenshots/picker.png` shows six rows, no section headers, and "Tern" branding. Current picker is eight rows, five fixed sections, Lane title (`Picker.qml:16-18`, `PickerModel.cpp:139-145`). Settings PNG shows a flat six-target list without search or collapsible sections (`TargetsPage.qml:486-767`). First-time readers judge from `README.md:14-21` before install.

2. **Ranked suggestion can sit below the fold at scale.** `rankForPicker()` in `router.cpp:85-118` pushes matching PWAs and remembered targets first, but `PickerModel::applyFilter()` regroups into fixed section order (`PickerModel.cpp:130-153`: Web apps, Containers, Browsers, Apps, Actions). A remembered browser profile lands in the Browsers bucket and can appear on row 15+ when PWAs and containers fill the viewport. Number keys 1-8 only reach the first eight *display* rows (`PickerModel.cpp:44-47`), not the ranked pick. At 40+ targets the user must filter or scroll to reach the obvious choice.

3. **`suggested` model role is wired but invisible.** `PickerModel.cpp:48-49` sets `SuggestedRole` when `index.row() == 0` (first row after regrouping, not the router's suggestion). `Picker.qml:256` binds `suggested` but never uses it for weight, badge, or color. Users get no visual cue for the smart default beyond `currentIndex: 0` highlighting whatever ended up first after section bucketing.

4. **Empty picker conflates "no matches" with "nothing configured."** `Picker.qml:204` always says "No matching destinations." When every target is hidden or discovery found zero browsers (`router.cpp:220-222`, reason `empty`), the message does not point to Settings or Rediscover. Enter and digit keys are guarded (`Picker.qml:49-50`, `169-174`) but offer no next step. Harmless dead end, poor first-run experience.

5. **Alt+A is the only keyboard path to Always, and it is not labeled.** `Picker.qml:53` binds Alt+A; footer shows ladder keys and Esc (`Picker.qml:446-467`) but never Alt+A. Mouse users see the checkbox (`Picker.qml:354-372`); keyboard-only users must read README tutorial (`README.md:83-85`) or source.

### Follow-ups

6. **Settings section order disagrees with picker section order.** Targets page lists Browsers first (`TargetsPage.qml:529-573`), then Containers, PWAs, Private, Custom. Picker shows Web apps, Containers, Browsers, Apps, Actions (`PickerModel.cpp:139-145`). Reorder in Settings does not match scan order in the picker. Mental-model friction when tuning a large list.

7. **Per-row Default buttons still duplicate the global fallback combo.** `TargetsPage.qml:516-526` "Fallback target" is the right single control. Default buttons on every browser and container row (`137-148`, `224-235`) add noise at 40+ rows. PWAs and custom apps correctly omit them.

8. **Regret path for Always remains a settings dig.** No undo on the post-open toast (`Controller.cpp:820-828`). Tray to Settings to Rules to find host to delete (`RulesPage.qml:117-127`). Acceptable for v1, painful when the checkbox label was misread.

9. **Hold progress bar is visual-only for assistive tech.** `Hold.qml:93-99` has no `Accessible.value` or live-region text for countdown. Name and description at `Hold.qml:52-54` cover Enter/Esc/Space well; the bar itself is decorative. Fine for a 1.6s veto, minor gap.

10. **X11 fallback loses exclusive keyboard grab.** `Controller.cpp:887-889` logs layer-shell unavailable and returns; picker relies on `Qt.WindowStaysOnTopHint` only (`Picker.qml:10`). Focus can leak to windows behind the card. Known platform constraint; worth a one-line note in Overview for X11 users.

11. **Digit shortcuts disabled while filtering.** `Picker.qml:165-167` returns early when `filterField.text.length > 0`. Intentional (digits become filter input), but power users cannot type two letters then press 3 to pick the third filtered row. Filter-then-Enter only.

12. **Blocked URL is notification-only.** `Controller.cpp:717-724` skips picker rows that would all fail; `notifyBlocked()` at `831-838` is the only feedback. Invisible when notifications are off.

---

## Considered and fine

1. **Resident daemon + layer-shell overlays.** `Controller.cpp:884-907` exclusive keyboard on Wayland; correct for Plasma.
2. **Light tint, not heavy dim.** Picker 12% (`Picker.qml:59`); hold 8% (`Hold.qml:27`). Matches DESIGN.md.
3. **Rules skip hold.** `Controller.cpp:917-924` excludes `rule` reason.
4. **Hold default off, 1600ms configurable.** `PreferencesPage.qml:48-65` exposes 400-5000ms range. Sane veto window.
5. **Hold HUD copy and keys.** `Hold.qml:77-78` reason text; Enter now, Esc/Space pick instead (`Hold.qml:18-21`, `107-133`). Clear.
6. **Checkbox for Always, not a switch.** `Picker.qml:354-372`. Matches DESIGN.md.
7. **Filter auto-focused on show.** `Picker.qml:38-45`. Right default for large lists.
8. **Destination ladder in footer.** Chevrons, comma/period hints (`Picker.qml:373-467`), `Accessible.name` on buttons.
9. **Two-pane settings, no hamburger.** `Settings.qml:20-122` four stable pages.
10. **Collapsible Targets sections + search.** Private collapsed by default (`TargetsPage.qml:17`). Drag disabled under filter (`83`, `170`). Solid.
11. **Global fallback target combo.** `TargetsPage.qml:516-526` clearer than per-row Default at scale.
12. **Container empty hint.** `TargetsPage.qml:622-631` explains `containers.json` and extension requirement.
13. **Dangling rule/remembered warnings.** `RulesPage.qml:60-79`, `130-137`.
14. **Copy link in footer.** `Picker.qml:410-444` plus Ctrl+C from filter (`160-163`).
15. **First-run Overview flow.** Autostart, default browser, Try picker, Rediscover (`OverviewPage.qml:16-57`, `README.md:59-73`).
16. **Not-default-browser state is honest.** Overview explains nothing redirects until opt-in (`OverviewPage.qml:18-20`).
17. **Digit keys on empty list are harmless.** `Controller::pick()` no-ops on invalid row (`Controller.cpp:418-424`).

---

## Question answers

### 1. Keyboard model

**Verdict: coherent with two discoverability gaps.**

Flow works: filter focused on open, type to narrow, Up/Down or hover to move highlight, Enter to confirm, Esc to cancel, comma/period (or chevrons) for destination ladder, Alt+A for Always. TextField correctly owns digits and comma/period when focused (`Picker.qml:153-187`); Return/Escape/Alt+A stay as `Shortcut{}`.

Dead ends: empty list plus Enter is a no-op (guarded). Digit keys 1-8 on empty list call `pick(row)` which returns early. Harmless. Filtering disables digit shortcuts by design.

Gaps: Alt+A not in footer. No Tab affordance to the Always checkbox from the filter field (checkbox is below the fold on short screens).

### 2. Section order and scannability at 40+

Fixed picker order Web apps, Containers, Browsers, Apps, Actions (`PickerModel.cpp:139-145`) matches DESIGN.md intent (PWAs above browser windows). Good for frequency when PWAs are the primary picks.

At 40+ targets, eight visible rows plus section headers (`Picker.qml:16-18`, `197-199`) mean scrolling or filtering is mandatory. Number keys only cover the first eight *display* rows, which may not include the ranked suggestion after regrouping. Filter saves the day but breaks the "200ms, no thinking" bar for remembered-browser users with many PWAs installed.

### 3. Empty and edge states

| State | UI | Gap |
|-------|-----|-----|
| Filter miss | "No matching destinations" (`Picker.qml:204`) | Good |
| Zero/hidden targets | Same message | No link to Settings / Rediscover |
| No default browser | Overview explains + button (`OverviewPage.qml:18-28`) | Good |
| First run | Overview tutorial path | Good |
| Blocked URL | Notification only (`Controller.cpp:717-724`) | No in-overlay copy |
| Broken rules/remembered | Warning rows (`RulesPage.qml:60-79`) | Good |

### 4. Accessibility

Picker dialog name and filter field are labeled (`Picker.qml:84-91`, `189-191`). List items include shortcut in name when unfiltered (`268`). Section headers named (`245-246`). Hold overlay named with key hints (`Hold.qml:52-54`). Settings nav items named (`Settings.qml:77-79`). Collapse toggles named on Targets.

Remaining: Hold progress not exposed to AT. Container color dot is supplementary to text (`Picker.qml:296-302`), not color-only. `suggested` not surfaced. Secure host lock icon (`Picker.qml:101-104`) has no text equivalent beyond host label.

### 5. Hold HUD

Reason text plus progress plus key hints are clear (`Hold.qml:77-133`). 1600ms default (`PreferencesPage.qml:58-60`, `DESIGN.md:46`) is a reasonable veto window: long enough to react, short enough not to feel like a second picker. Space as cancel is documented in hold footer but not in README product summary (tutorial covers it at `README.md:101`).

### 6. Settings IA

Four pages map well: Overview (system), Browsers and apps (inventory), Rules (automation), Preferences (behavior). Nothing critical is buried.

Duplication: per-row Default vs global Fallback target combo. Default browser (system handler) vs default target (Lane fallback) are different concepts; Overview covers the former, Targets the latter. Could use a cross-link but not blocking.

Picker vs Settings section order mismatch (finding 6) is the main IA friction.

---

## Method

1. Ran `gstack-skill-start --skill plan-design-review` (SESSION_ID `777869-1789769917-db092e71`, TEL_START `1789769917`).
2. Read round-2 `docs/reviews/design.md` and verified each claimed fix against current source.
3. Read all QML under `src/qml/`, `Controller.cpp` hold/picker paths, `PickerModel.cpp`, `router.cpp`, `DESIGN.md`, `README.md`.
4. Checked screenshot mtimes on disk. No live UI.
