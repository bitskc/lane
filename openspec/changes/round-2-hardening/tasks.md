# Tasks: round-2-hardening

Branch `fix/round-2-hardening`. Issues: #81 (implemented set), #71, #72, #73, #78. Issues #74, #75, #76, #77, #79, #80 are out of scope (see proposal).

Process: this change is gated on gstack eng and arch sign-off before merge. Items marked **retro** were implemented before this spec existed and are reviewed against their invariant, not assumed correct.

## 1. Already implemented (retro, #81)
- [x] 1.1 Plist depth/node/size budget (D1). Test `oversizedPlistIsDropped`.
- [x] 1.2 First-run flag consumed by explicit Settings open; mac `--rediscover`/`--pick` excluded (D2).
- [x] 1.3 `linuxDefaultHandlerState` pure function (D3). Test `test_linux_handler`.
- [x] 1.4 Publish `g_macBundlePaths` under a mutex; `bundle:` icons synchronous (D4).
- [x] 1.5 Single in-flight click: `ExcludeUserInputEvents`, quit flag, SIGTERM self-pipe, deferred commands, `QPointer` callbacks, per-URI token, logged queue drop (D5).
- [x] 1.6 Canonical host in `isPrivateOrLocalHost` (D6). Tests in `test_url`.
- [x] 1.7 `normalizeInput` keeps the origin; strict `isSafeOpenUrl`; shared control predicate (D7). Tests in `test_url` run the real `normalizeInput` then `isSafeOpenUrl` chain. The first attempt tested `isSafeOpenUrl` alone and was wrong.
- [x] 1.8 Per-format link-file parse, `isFile()` before `open`, entity decode (D8). Tests in `test_urlevents`.
- [x] 1.9 `%u` token boundary (D9). Test `percentUDoesNotMatchLongerToken`.
- [x] 1.10 Windows interpreter basenames (D10). Test `windowsInterpreterBasenamesAreBlocked`.
- [x] 1.11 **Add the missing tests for 1.2, 1.4 and 1.5.** `test_firstrun` builds a real Controller: explicit Settings consumes the flag and the startup hook does not open a second window; an existing config is not a first run. Removing the clear in `openSettings()` fails it. `rescanDropsBundlePathsThatLeft` requires a second scan to drop Brave; merging into the old map fails it. Quit and queue stay in `test_clickgate`. SIGTERM stays the restart smoke: the handler only writes a byte. Dep: none.

## 2. Engine retry and dead code (#73)
- [x] 2.1 `ensureEngine` deletes the engine and nulls the pointer when no root object loads (D11, `25ac107`). No unit test of the retry: QML is compiled into resources, so a failed load cannot be produced at runtime, and a test of the delete would restate that line. Settings and a forced picker load on a private daemon.
- [x] 2.2 Remove `Controller::save()`, `pickerVisibleChanged`, and the unreachable LRU branch in `NotificationActionMap::insert` (`25ac107`). Callers checked first; no shims.

## 3. NAT64 and 6to4 (#78)
- [x] 3.1 Decode `64:ff9b::/96` and `2002::/16` before the range check (D12, `f688994`).
- [x] 3.2 Table test in `test_url`: each prefix with loopback, RFC1918, link-local and a public IPv4, plus the address forms QHostAddress accepts. The public case is not private. Dep: 3.1.

## 4. Settings keyboard and layout (#71)
- [x] 4.1 Add `LaneUi.SidebarNav` to `laneui` with focus, key navigation, accessible role and selected state (D13). Registered in the QML module file list.
- [x] 4.2 `Settings.qml` uses `SidebarNav`; the `MouseArea` delegates and the caller-less `selectPage` are gone. Dep: 4.1.
- [x] 4.3 The divider uses `implicitWidth: 1` plus `Layout.fillHeight`. `RulesPage` rows use `Layout.fillWidth`.
- [x] 4.4 `TargetsPage` diff-updates via `LaneUi.ModelSync` keyed by `targetId`. `LaneUi.RenameQueue` keeps every same-tick rename. `Qt.callLater` stays, because a drop still emits `settingsChanged` on the delegate's stack. Dep: none.
- [x] 4.5 QML tests in `tests/qml/tst_sidebarnav.qml`. Tab, Down/Enter, accessible name and selected state, click, delegate identity, insert/remove/move. They fail when sync is `clear()+append`, when the rename queue keeps one slot, when key navigation is off, and when rows cannot take focus. Dep: 4.1, 4.4.
- [x] 4.6 Offscreen grab on a private bus (`/tmp/lane-grab-nav.*/out/settings-nav.png`, not in the repo). Window 920x660. A 1px line at x=220 runs the full height (pixel 220,220,220 against a 244 sidebar). The second nav row has a blue focus edge. No QML errors. Dep: 4.2, 4.3.

## 5. Picker hints (#72, narrowed)
- [x] 5.1 Zero-target hint, `Alt+A`, and `Hold.qml` font come from the existing per-platform table (D14). Linux hint text was read on a private daemon. macOS Option glyphs are code-only on this host. The hint keyed off `targetCount`, which is never 0 because copy and email are always targets; it now keys off an empty picker list.
- [x] 5.2 Comma and period at the first or last rung are consumed while the filter is empty. Verified by code reading. Key events cannot be injected into the offscreen picker.
- [x] 5.3 The private-mode notice clears from the Up/Down handlers and from filter text, not from `onCurrentItemChanged`. A programmatic `currentIndex` change (what hover does) leaves the notice up; the old clear-on-change path dismissed it.
- [x] 5.4 The picker comment says digits, comma, period and Ctrl/Cmd+C stay on the filter field, and that Tab moving focus off the field is why they stop. That split is unchanged.
- [x] 5.5 No stub-controller QML test. Private-bus `--pick` twice on the merged tree loads with no QML errors. The comma-at-the-last-rung keypress is still a manual check. Dep: 5.1 to 5.3.

## 6. Verification
- [x] 6.1 `cmake --build build && ctest` green on Linux: 24 passed on the merged tree (was 23 before `test_firstrun`).
- [ ] 6.2 ASan+UBSan build (`build-asan`) full ctest, no reports.
- [ ] 6.3 Install and `systemctl --user restart lane.service`: active, D-Bus introspection shows `org.freedesktop.Application`, `lane --list` unchanged, `lane --explain` routes, offscreen Settings smoke on a private bus with no QML errors. Restart exercises the SIGTERM path.
- [ ] 6.4 macOS CI job green on the self-hosted runner for the pushed branch. Not claimed verified on the device (see #80).
- [ ] 6.5 `openspec validate round-2-hardening --strict` clean and a scratch archive dry run clean.

## 7. Close-out
- [ ] 7.1 CHANGELOG entry. Reviews in `docs/reviews/` cross-reference the fixing commit.
- [ ] 7.2 gstack PR review; fix findings; merge; close #71, #72, #73, #78, #81 with the commit.
- [ ] 7.3 Archive the change after merge. Dep: 7.2.

## Test plan
| Test | Protects | Fails when | Does not claim |
|---|---|---|---|
| `test_linux_handler` | Partial only if exactly one scheme is Lane | Early-break loop returns Partial for a miss | That `xdg-mime` output is real |
| `oversizedPlistIsDropped` | Deep and wide plists are dropped, shallow sibling kept | Cap removed or scanner ignores dir | `plutil` path on macOS |
| URL chain tests | `normalizeInput` then `isSafeOpenUrl`: attack strings blocked, spaces/`%20`/IDN/IPv6 still open, control chars refused, trailing-dot hosts private | `fromUserInput` restored in `normalizeInput`, or `%20` decoded again | Resolver behavior |
| `test_urlevents` link-file tests | Decoy line, fifo, `/dev/zero`, entities | Single regex or `open` before `isFile` | Binary Finder plists |
| NAT64/6to4 table (3.2) | Embedded private IPv4 classified private | Only `::ffff:` unwrapped | A real connect |
| Engine retry (2.1) | Failed load can retry | Pointer left set | A broken install end to end |
| `SidebarNav` QML tests (4.5) | Keyboard reaches every section | Mouse-only delegates | Screen-reader speech |
| Concurrency tests (1.11) | Quit and SIGTERM abandon the click | Quit flag removed | Real HEAD request |
