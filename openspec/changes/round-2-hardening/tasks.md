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
- [ ] 1.11 **Add the missing tests for 1.2, 1.4 and 1.5.** The review found these fixed but untested: first-run once, bundle-path publish, quit/SIGTERM/queue behavior. Each test must fail on the pre-fix code (mutation check). Dep: none.

## 2. Engine retry and dead code (#73)
- [ ] 2.1 `ensureEngine` deletes the engine and nulls the pointer when no root object loads (D11). Test: first call fails, second succeeds after the module becomes loadable.
- [ ] 2.2 Remove `Controller::save()`, `pickerVisibleChanged`, and the unreachable LRU branch in `NotificationActionMap::insert`. `lsp references` first; no shims.

## 3. NAT64 and 6to4 (#78)
- [ ] 3.1 Decode `64:ff9b::/96` and `2002::/16` before the range check (D12).
- [ ] 3.2 Table test: each prefix with loopback, RFC1918, link-local and a public IPv4, plus the two address forms QHostAddress accepts. Assert the public case is not private. Dep: 3.1.

## 4. Settings keyboard and layout (#71)
- [ ] 4.1 Add `LaneUi.SidebarNav` to `laneui` with focus, key navigation, accessible role and selected state (D13). Register in the QML module file list.
- [ ] 4.2 `Settings.qml` uses `SidebarNav`; remove the `MouseArea` delegates and the caller-less `selectPage`. Dep: 4.1.
- [ ] 4.3 Replace the `width: 1` divider with an implicit-width item. Replace the `RulesPage` `parent.width` binding with `Layout.fillWidth`.
- [ ] 4.4 `TargetsPage` diff-updates its models instead of `clear()`+`append`; the rename `Timer` carries per-row state (D13).
- [ ] 4.5 QML tests in `tests/qml`: keyboard Tab/Down/Enter selects a section; accessible name and selected state; and a model diff-update that keeps an unaffected delegate's identity. Each must fail on the old structure. Dep: 4.1, 4.4.
- [ ] 4.6 Screenshot of the Settings window (offscreen, private bus, per `lane-screenshots-no-kwin`) showing the divider and the focused nav item. Evidence, not inference. Dep: 4.2, 4.3.

## 5. Picker hints (#72, narrowed)
- [ ] 5.1 Zero-target hint from `hintSettings`/`hintRescan` in the existing table; `Alt+A` hint; `Hold.qml` uses `hintFont` (D14).
- [ ] 5.2 Comma/period at the first or last rung are consumed, never inserted into the filter.
- [ ] 5.3 The private-mode notice clears on keyboard selection change, not hover.
- [ ] 5.4 Document in the picker that digits, comma, period and Ctrl/Cmd+C intentionally live on the filter field (already commented); no change to that split.
- [ ] 5.5 Picker QML test with a stub `controller` is out of scope (150+ properties). Verify 5.1 to 5.3 in the real app instead: launch `lane --pick` on a private bus with an empty config and observe the hint, then press `,` at the last rung with a filter focused. Dep: 5.1 to 5.3.

## 6. Verification
- [ ] 6.1 `cmake --build build && ctest` green on Linux (22 now, more after 1.11, 3.2, 4.5).
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
