# QML audit

Fixing commits: findings 2, 3, 4, and 7 are `49653e1` (focusable `SidebarNav`, 1px divider, `Layout.fillWidth` rule rows, `ModelSync`, `RenameQueue`). Findings 1, 5, and 8 are `58a7547` (one hint table, notice not cleared by hover, comma and period consumed at the ladder ends). Finding 6 stays: those keys live on the filter field on purpose, so a digit can still be typed. Findings 9 through 12 are outside this change.

Scope: every file under `src/qml/` (pages, windows, `src/qml/ui`). There is no `src/ui/` tree; the form controls live in `src/qml/ui`. Product code was not modified. Rows 9-19 of `docs/designs/gstack-full-analysis.md` are not repeated. The LaneIcon `sourceSize` loop fixed in `2734da6` is checked below and stays closed.

## Verdict

No P0. The mac hint properties that exist (`hintCopy`, `hintAltP`, `hintAltE`) match the real shortcuts: Qt maps `ControlModifier` to ⌘ and `Alt` to ⌥, and nothing in that table swaps the two. The gaps are strings that bypass the table, plus keyboard and layout bugs the picker/settings surfaces still have. Settings cannot change page from the keyboard, the sidebar divider is a layout-managed `Rectangle` with no preferred width, rule rows set `width` on a `ColumnLayout` child, and the private-mode notice is cleared by hover.

## Findings

### P2

1. **Zero-target hints bypass the mac table** (`src/qml/Picker.qml:19-27`, `src/qml/Picker.qml:66-75`, `src/qml/Picker.qml:286-294`). `Shortcut` sequences `Alt+S` and `Alt+R` are Option on macOS, same as `Alt+P` / `Alt+E`, which render as `⌥P` and `⌥E`. The empty state still hardcodes `alt+s settings · alt+r rescan` for every OS, and that `Label` has no separate `Accessible.name`, so the spoken string is the same literal. `Alt+A` (`Picker.qml:80`) toggles the checkbox and has no hint text at all. Hold's `enter` / `esc / space` labels (`src/qml/Hold.qml:117-134`) always use `font.family: "monospace"` and ignore `hintFont`. Previously noted in `docs/reviews/design.md`; still the code on `main`. Confirmed this pass: the three `hint*` properties are not swapped.

   | Action | Code | mac label that should show | Label that does show |
   | --- | --- | --- | --- |
   | Copy link | `Qt.ControlModifier` + `Key_C` (`Picker.qml:209`) | `⌘C` via `hintCopy` | `⌘C` |
   | Private | `Shortcut` `Alt+P` (`Picker.qml:81-85`) | `⌥P` via `hintAltP` | `⌥P private` |
   | Email | `Shortcut` `Alt+E` (`Picker.qml:86`) | `⌥E` via `hintAltE` | `⌥E` |
   | Always | `Shortcut` `Alt+A` (`Picker.qml:80`) | `⌥A` | none |
   | Settings / rescan | `Shortcut` `Alt+S`, `Alt+R` (`Picker.qml:66-75`) | `⌥S`, `⌥R` | `alt+s`, `alt+r` |

2. **Settings sections are mouse-only** (`src/qml/Settings.qml:56-104`). The nav `ListView` has no `focus`, no `keyNavigationEnabled`, and the delegate is a `MouseArea` with `Accessible.role: ListItem` on a rectangle that never takes focus. `selectPage()` (`Settings.qml:16-18`) has no caller in `src/` or `tests/`. Tab order starts in the current page (Overview). Keyboard and switch users cannot reach Browsers, Rules, or Preferences.

3. **Sidebar divider sets `width` inside a `RowLayout`** (`src/qml/Settings.qml:109-113`). The separator is a direct child of the window `RowLayout` with `width: 1` and `Layout.fillHeight`. Qt Quick Layouts size children from `implicitWidth` / `Layout.preferredWidth`, and a `Rectangle` implicit width is 0. `FormDelegateSeparator.qml:10-13` already does this the working way (`implicitHeight: 1`, `Layout.fillWidth`). [INFERENCE] the 1px line is laid out at 0 width and does not show. The conflict itself is in the file.

4. **Rule rows bind `width` to an unqualified `parent` while that parent is a `ColumnLayout`** (`src/qml/RulesPage.qml:42-55`). The `Repeater` delegate is inserted into `FormCard`'s column (`FormCard.qml:13`, `FormCard.qml:29-31`) and sets `width: parent ? parent.width : 100`. Layout children are supposed to use `Layout.fillWidth`; the layout also writes `width` every pass. The `parent ?` guard admits the delegate is briefly parentless. Inner rows then read that width. This is the only delegate-root `parent` lookup under `src/qml` (picker rows use `ListView.view.width`).

5. **Private-mode notice clears when the pointer changes row** (`src/qml/Picker.qml:309`, `src/qml/Picker.qml:338-341`). `onCurrentItemChanged` calls `controller.clearPickerNotice()`. Row `onEntered` assigns `list.currentIndex`. `pickPrivate` leaves the picker open and sets `pickerNotice` when a target has no private window (`Controller.cpp:682-688`). Moving across rows dismisses that error. Arrow keys do the same, which at least matches an explicit selection change; hover does not.

6. **Digit, comma, period, and copy shortcuts exist only on the filter field** (`src/qml/Picker.qml:183-236`). Window `Shortcut`s cover Return, arrows, and the Alt chords, so those survive a focus move. `1`-`8`, `,`, `.`, and Ctrl/Cmd+C are `Keys.onPressed` on `filterField`. `onVisibleChanged` calls `forceActiveFocus()` (`Picker.qml:38`), then Tab can move to the Always checkbox and the ladder buttons (`Picker.qml:453-502`), which are real focusable controls. After that, the number badges and the `,` / `.` hints still show, and they do nothing. Copy is the same: the footer advertises `⌘C` / `^C`, and the handler is gone until focus returns to the field.

7. **Settings destination lists are rebuilt from scratch on every `settingsChanged`** (`src/qml/pages/TargetsPage.qml:40-46`, `src/qml/pages/TargetsPage.qml:216-238`, `src/qml/pages/TargetsPage.qml:299-310`). `syncModels()` does `allModel.clear()` plus `append` (same for `privateModel`). `hideTarget`, `renameTarget`, and `moveTarget` all emit `settingsChanged` (`Controller.cpp:789-801`, `974-990`). `Qt.callLater` only leaves the signal stack so the drag delegate is not destroyed mid-callback. `callLater` still runs on the next event-loop turn, while `moveDisplaced` is `Theme.longDuration` (250ms, `Theme.qml:32`). The rebuild therefore still destroys the delegates, aborts the settle animation, and constructs new `LaneIcon`s (`asynchronous: true`). A rename `Timer` (`interval: 1`) shares one `targetId`/`newName` pair across every row, so two `editingFinished` signals in the same millisecond keep only the second rename.

### P3

8. **Ladder keys at the end of the range type into the filter** (`src/qml/Picker.qml:214-235`). Comma and period are accepted only when the index actually changes. At the first or last rung the event stays unaccepted and the character is inserted, which also turns on filtering and hides the number badges.

9. **A missing rule target displays as the first combo entry** (`src/qml/RulesPage.qml:76-80`). `currentIndex` uses `Math.max(0, targetIds.indexOf(targetId))`. A dangling id yields -1, the combo shows row 0, and the value is not written back until `onActivated`. The warning label (`RulesPage.qml:82-100`) tells the truth; the combo does not. The same clamp is on the fallback combo (`TargetsPage.qml:274-279`) and activity defaults (`RulesPage.qml:146-149`, missing id becomes "Use default").

10. **Targets rows are thinly named for assistive tech** (`src/qml/pages/TargetsPage.qml:87-153`, `src/qml/pages/TargetsPage.qml:176-211`). Rename fields bind `text: model.name` and set no `Accessible.name`. The show/hide `Switch` has no text. `Remove` is the same word on every custom row. The Default button does set `Accessible.name` (`TargetsPage.qml:139-141`). `FormTextFieldDelegate` does name its field (`FormTextFieldDelegate.qml:80`); `FormComboBoxDelegate` (`FormComboBoxDelegate.qml:66-70`) and `FormSpinBoxDelegate` (`FormSpinBoxDelegate.qml:57-61`) do not name the inner control, so the popup/spinbox is a second unlabeled widget beside the row label.

11. **Copy and email advertise `Accessible.Button` and are not in the tab order** (`src/qml/Picker.qml:509-544`, `src/qml/Picker.qml:545-580`). Activation is `TapHandler` plus the shortcut. Descriptions correctly say Command C and Option E on macOS. A keyboard user who has tabbed off the filter cannot focus them, and finding 6 is why the shortcut may be gone too.

12. **`FormRadioDelegate` replaces `contentItem` and is unused** (`src/qml/ui/FormRadioDelegate.qml:16-32`). Nothing in `src/qml` instantiates it. The radio indicator is the control's `indicator`, not `contentItem`, so the glyph can sit on top of the wrapped label if a caller starts using the type. Dead today.

## Considered and fine

- **LaneIcon binding loop.** `sourceSize` is the constant `Qt.size(64, 64)` (`src/qml/ui/LaneIcon.qml:14-18`). No other `sourceSize` binding under `src/qml`. Call sites set `Layout.preferredWidth/Height` to literals. The self-binding from `2734da6` is gone.
- **Card `height: content.implicitHeight + 32`** (`Picker.qml:102-103`, `Hold.qml:38-39`). The column's height comes from children with their own heights (fixed row height, one-line labels, or the notice's wrap at a fixed width of `parent.width - 32`). That does not read the card height. Same for Hold's progress row.
- **`Layout.preferredWidth: emptyState.width`** (`Picker.qml:269`) and **`suggestedLabel.implicitWidth + 12`** (`Picker.qml:395`). The empty-state item is `anchors.fill` of a width set by the column, not by the label. The badge label is `anchors.centerIn` and does not read the badge width.
- **Settings `ListView` `implicitHeight: contentHeight`** (`TargetsPage.qml:303-304`, `341-342`). Delegate height is `rowHeight` or 0 (`TargetsPage.qml:56`, `163`) and does not read the view height, so this is not the LaneIcon cycle. Both lists set `interactive: false`; the page `Flickable` scrolls. [INFERENCE] a runtime "binding loop" warning is still possible inside `ListView`'s own `contentHeight` writer; the QML dependency direction does not close a loop. Not probed in a window this pass.
- **`onCompleted` vs models.** `Controller::reload()` runs in the constructor (`Controller.cpp:57`) before `loadFromModule` (`Controller.cpp:1200`). `TargetsPage` `syncModels()` and `RulesPage` `refreshDangling()` (`RulesPage.qml:29`) therefore see a filled model. Later edits arrive through `Connections` on `controller.settingsChanged`. `controller` owns the engines (`new QQmlApplicationEngine(this)`), so those `Connections` targets outlive the windows.
- **`ScrollableFormPage` focus connection** (`ScrollableFormPage.qml:52-57`). `target: root.Window.window` rebinds when the attached window appears. Pages are children of the `ApplicationWindow` and are destroyed before it. `tst_controls.qml` covers scroll-into-view for a nested field.
- **Up/Down are not double-stepped.** Window `Shortcut`s (`Picker.qml:78-79`) and `Keys.onDownPressed` / `onUpPressed` (`Picker.qml:200-201`) both increment. A `Shortcut` that matches consumes the key before the focused item's `Keys` handler. The `Keys` lines are backup, not a second step. [INFERENCE] from Qt `QShortcut` delivery; not re-probed on device here. Design review already saw Alt chords deliver while the field is focused.
- **`Qt.platform.os === "osx"`** (`Picker.qml:22`) is still the Qt 6 name for macOS. Windows and Linux correctly skip the mac strings. On those OSes `Alt+*` is the Alt key, and the copy path's `ControlModifier` is Ctrl. The `^C` / `alt+p` Linux labels match that. No Windows `src/ui` fork exists to drift.
- **Timers.** `DragHandle`'s 16ms timer is a child and `running` is bound to `dragActive` (`DragHandle.qml:96-99`). It is not created per move. The rename timer is one object on the page, not a per-edit allocation (the shared-id race is finding 7, not a leak).
- **ListModel.move while the page scrolls.** Drag math uses scene coordinates and `model.move` (`DragHandle.qml:22-31`, `96-109`). Persisted order goes through `targetId` (`TargetsPage.qml:78-79`, `Controller.cpp:982`). `tst_controls.qml` covers reorder and cancel. Drag is disabled while the search filter is non-empty (`TargetsPage.qml:74`), so 0-height filtered rows are not index targets. The outer flick autoscroll path is the untested part; the index formula uses row height, not animated `y`.
- **Picker delegate `parent.width`** on labels is the inner `Column`, and the delegate root uses `ListView.view.width` (`Picker.qml:327`). Hold/Picker header labels do the same inside a `Column` that is not a `Layout` child.
- **Anchors inside layout cells** (nav row, picker row, filter field) are anchors on items whose parent is a `Rectangle` or `Item`, with `Layout.*` only on the layout's own children. That split is consistent except findings 3 and 4.
- **Hold shortcuts** match the visible words: Return/Enter confirm, Escape/Space cancel (`Hold.qml:18-21`). `requestActivate()` is called when the hold window is shown (`Controller.cpp:1260-1262`).
- **Checkbox / policy bindings.** `setChecked` in Qt 6.11 emits `checkedChanged` and does not emit `toggled` (verified in `qquickabstractbutton.cpp` `setChecked` vs `QQuickAbstractButtonPrivate::toggle`). `FormSwitchDelegate.qml:47-50` calls `root.toggle()` only when the inner switch and the row disagree, then emits `toggled` once. `tests/qml/tst_controls.qml` locks the row and the glyph to a single toggle and checks that `checked: preference.value` still tracks later writes.

## Open questions

1. Does the settings sidebar divider actually paint on Linux and macOS? Finding 3 follows the layout rules; a screenshot would confirm the 0-width result.
2. Does `implicitHeight: contentHeight` on the two destination `ListView`s print a binding-loop warning when Settings opens with a full target list? The delegate heights do not close a QML cycle; `ListView` has historically warned anyway.
3. On a real Mac keyboard, does Option+S in the zero-target picker show the wrong hint and still fire `openSettings`, including when Option+S is a dead key? Synthetic delivery was already treated as fine in `docs/reviews/design.md`; the label drift does not depend on that.
