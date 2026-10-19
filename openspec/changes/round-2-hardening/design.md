# Design: round-2-hardening

## Context
The review lenses agreed on one pattern: a value crossed a boundary (network to launcher, file to router, image thread to GUI thread, signal to nested loop) and was trusted on a string comparison or parsed with no budget. This design records the decision for each fix and, for the forward work, the alternatives that were rejected. Items already implemented are listed with their invariant so a reviewer can check the code against the claim.

## Decisions for implemented work (retroactive)

### D1. Budget the Info.plist parse instead of trusting the file
`readPlistValue` takes `depth` and a shared `nodes` counter. Past depth 32 or 20,000 nodes it raises a stream error and returns an empty map, and the caller drops the bundle. A converted binary plist is capped at 8 MiB.
- **Why not parse and truncate:** a partial dict could still contain a valid `CFBundleURLTypes` and would register a bundle the scanner could not fully vouch for. Dropping is the fail-closed choice.
- **Why not a different parser:** `QXmlStreamReader` is already the dependency, and recursion depth is the only unbounded resource. A budget is the smallest change that bounds it.
- **Invariant:** a bundle whose Info.plist exceeds either cap never becomes a target, and a sibling bundle in the same directory still does (test `oversizedPlistIsDropped`).

### D2. The first-run flag is consumed by the first thing that does its job
`m_firstRun` exists so Settings opens once for onboarding. Any explicit Settings open does that job, so `openSettings()` clears it. On macOS `--rediscover` and `--pick` are commands, not a bare launch, so they do not call `maybeFirstRunSettings`.
- **Invariant:** Settings opens at most once as a side effect of first run, and never on top of a command the user just issued.

### D3. Default-handler state is a pure function of two query results
`linuxDefaultHandlerState(httpOk, httpDesktop, httpsOk, httpsDesktop)` returns Registered only if both are Lane, Partial only if exactly one is Lane and the other answered, and Unknown otherwise (including any failed query). It lives in a header so it is testable without spawning `xdg-mime`.
- **Why not leave the loop and add a flag:** the original loop could not distinguish "Firefox owns both" from "one is Lane". The distinction is the whole feature, and only a function over both results can express it.
- **Invariant:** Partial implies exactly one of http/https resolves to `app.lane.Lane.desktop`.

### D4. Cross-thread state is published, not shared
`g_macBundlePaths` is read by `ThemeIconProvider::requestPixmap` on Qt's image-loader thread and written by `scanMacBundles` on the GUI thread. The scan builds a local map and swaps it in under a `QMutex`. `bundle:` icons set `asynchronous: false` because `QFileIconProvider` reaches AppKit on the macOS side, which must stay on the GUI thread.
- **Why not make the hash a `QReadWriteLock`-protected member:** one writer, publish-once-per-scan, no partial state. A swap is simpler and cannot expose a half-built map.
- **Cost:** `bundle:` icons now load synchronously on the GUI thread. There are tens of them and `QFileIconProvider` caches, so this is accepted.

### D5. One click is resolved at a time, and Quit abandons it
`unshortenSync` is the only place Lane runs a nested loop. It now excludes user input. `aboutToQuit` sets `m_quitRequested`; `openUrl` checks it after the pipeline returns and does not route, launch or drain. SIGTERM is turned into `quit()` through a self-pipe and `QSocketNotifier`, because a signal handler may not call Qt. `--rediscover` and `--settings` that arrive while `m_inOpenUrl` set a pending flag and run after the click. D-Bus `Open` of several URIs restores the activation token before each URI because `openUrl` consumes it.
- **Why not remove the nested loop:** `runPipeline` is synchronous by contract (`UnshortenFn` returns a string) and the router consumes its result immediately. Making it asynchronous changes the pipeline's type and every caller. The nested loop is bounded (4 hops, 1.8 s each) and is now safe. A full async pipeline is a larger change that this review did not justify.
- **Why not `QEventLoop::ExcludeSocketNotifiers`:** the HEAD reply arrives on a socket. Excluding user input only is the narrowest mask that still lets the request finish.
- **Residual, accepted:** an already-visible picker keeps the rows from the decision that opened it. A reload during that time does not rebuild them mid-choice.

### D6. Host classification works on a canonical host
`isPrivateOrLocalHost` strips every trailing dot and folds U+3002 and U+FF0E to `.` before any comparison. `localhost.`, `127.0.0.1.`, `2130706433.` and `metadata.google.internal.` are private; `1.1.1.1.` is not.
- **Why fold in the predicate and not at the call sites:** it has five call sites (`pipeline`, `unshorten`, `updatedecision`, `Controller` twice). One canonicalisation point cannot be forgotten at the sixth.

### D7. The normalizer keeps the origin; the safety check stays strict
Two functions own this, and the first attempt fixed only the second.

`normalizeInput` is the first place a raw click string is parsed (`pipeline.cpp`), so it is where an origin could be invented. It used `QUrl::fromUserInput(...).toString()`. Given an authority it cannot parse (`https://example.com..`, `http://example.com:99999/`), `fromUserInput` guesses a scheme and moves the original one into the host, which produced `http://https//example.com..`, a well-formed URL whose host is `https`. That string then passed any strict check downstream. `toString()` also pretty-decodes, turning `%20` into a space.

Fix, in `normalizeInput`:
- A string that already contains `://` is parsed in `TolerantMode` and emitted with `QUrl::FullyEncoded`. The scheme and authority are never reinterpreted. `TolerantMode` still re-encodes a stray `%` (`100%off` becomes `100%25off`) and a literal space, as the old code did.
- A string with no `://` (a bare host or `host/path`) still goes through `fromUserInput`, which is what it is for.
- A string holding a forbidden control character (CR, LF, NUL, U+2028, U+2029) is returned untouched, so it stays visibly bad and `isSafeOpenUrl` refuses it. Encoding it to `%0A` here would launder it into a string that looks safe.

`isSafeOpenUrl` and `sanitizedOpenUrl` parse in `StrictMode`. They share `hasForbiddenControl` with `normalizeInput`, so the two cannot disagree on what a forbidden character is.

- **Why the first attempt was wrong:** it changed only `isSafeOpenUrl`, and the unit test called `isSafeOpenUrl` directly. The real path is `normalizeInput` then `isSafeOpenUrl`, so the attack strings were repaired before the strict check saw them, and a correctly encoded `https://example.com/a%20b` was newly refused because `normalizeInput` had decoded it to a space. Found by the outside-voice review and reproduced against the real library.
- **Test the real chain:** the regression test calls `normalizeInput` and then `isSafeOpenUrl` on a table of inputs. A test of `isSafeOpenUrl` alone cannot catch this class.
- **Residual, deliberate:** `parseUrl`, `hostOf` and `displayUrl` still use `fromUserInput`. They serve matching and display, none gates a launch, and a bare host in a rule must still match.

### D8. Link files are parsed per format and stat'd before opened
`.webloc` reads only the plist `URL` key and decodes XML entities. `.url` reads only a `URL=` line. A path that is not a regular file is ignored before `open`, which stops a fifo (blocks in `open`) and a symlink to `/dev/zero` (reports size 0, so a size cap never fires). Symlinks to real files still work.

### D9. `%u` is a field code, not a substring
`expandArgs` replaces `%u` only when the next character is not a letter, digit, underscore or `%`. `--url=%u` and a bare `%u` expand; `%user` does not.

### D10. The exec blocklist names Windows interpreters with their extension
`isBlockedInterpreter` compares `fileName().toLower()`, which keeps `.exe`. `powershell.exe`, `pwsh.exe`, `python.exe`, `wscript.exe`, `cscript.exe` and `mshta.exe` are added.

## Decisions for forward work

### D11. A failed engine load is deleted so the next call retries (#73)
`ensureEngine` returns early when the pointer is set, including after `loadFromModule` produced no root object. On failure it now `deleteLater`s the engine and nulls the pointer.
- **Alternative rejected: retry with backoff.** There is no transient failure to wait out. A missing QML module or syntax error fails identically every time. Retrying on the next user action is enough and matches how a broken install is fixed (reinstall, then click again).
- **Dead code removed with no shim:** `Controller::save()` (zero callers; `persist()` is the real path), `pickerVisibleChanged` (zero listeners, no `Q_PROPERTY`), and the move-to-front branch in `NotificationActionMap::insert` (keys are a monotonic counter, so the branch cannot run).

### D12. Decode NAT64 and 6to4 before the range check (#78)
For an IPv6 host, if the address is in `64:ff9b::/96` take the low 32 bits; if in `2002::/16` take bits 16 to 47. Run the resulting IPv4 through the existing private-range ladder. Both are decoded in the same step that already unwraps `::ffff:`.
- **Why not block all IPv6 transition prefixes:** `64:ff9b::/96` is how an IPv6-only network reaches public IPv4 sites, so a blanket block would break real shorteners on such networks. Decoding classifies by the address the connection actually reaches.
- **Out of scope:** `64:ff9b:1::/48` (local-use NAT64) carries a different embedding. It is named in the issue as a follow-up and is not claimed fixed.

### D13. Settings navigation becomes a real focusable control (#71)
The sidebar is `MouseArea` delegates on a `ListView` that never takes focus. Extract it as `LaneUi.SidebarNav` in the shared `laneui` module: a `ListView` with `activeFocusOnTab`, `keyNavigationEnabled`, `highlightFollowsCurrentItem`, and delegates that are `QQC.ItemDelegate` (focusable, activate on click and Enter). `Settings.qml` instantiates it and binds `currentIndex` to the `StackLayout`.
- **Why extract rather than patch in place:** `Settings.qml` is in the `lane` module and needs a stub of 150 `controller` properties to load under the test harness. `test_qml` links only `laneuiplugin`. A control in `laneui` has no `controller` dependency, is testable in the existing harness, and is the control the page was missing. The in-place patch would be correct but untestable, which is how `selectPage()` ended up with no caller and no one noticing.
- **Alternative rejected: add `Keys` handlers to the existing delegates.** That keeps a non-focusable `Rectangle` and re-implements what `ItemDelegate` provides, including the accessible role and focus ring.
- **Divider:** replace the `width: 1` rectangle with `implicitWidth: 1` and `Layout.fillHeight`, matching `FormDelegateSeparator`. Confirmed by screenshot, not by reasoning, because the layout claim is inference.
- **Rule rows:** replace `width: parent ? parent.width : 100` with `Layout.fillWidth: true`.
- **Target lists:** diff-update the `ListModel` (insert, remove, set, move by id) instead of `clear()` plus `append`. This keeps delegates alive across `settingsChanged`, so the 250 ms drag settle completes and icons are not rebuilt. The rename `Timer` carries its own `targetId` and `newName` per row so two edits in one millisecond both persist.

### D14. Picker key handling stays split, and that is a Qt constraint (#72, narrowed)
`Picker.qml` handles digits, comma, period and Ctrl/Cmd+C in `filterField.Keys.onPressed`. The comment at `Picker.qml:202-207` states why: a focused `TextField` claims plain character keys at the shortcut-override stage, so a window-level `Shortcut` for `1` never fires while the field has focus, and conversely, a window-level shortcut for `1` that did fire would stop the user typing `1` into the filter.
- **Decision: do not move those keys.** The review's complaint (the keys stop working after Tab moves focus off the field) is real, but the fix is to **keep focus on the filter field**, not to move the handlers. Tab from the field goes to the Always checkbox and ladder buttons; those are reachable by mouse and by Tab, and the number badges only advertise while the field is focused and empty.
- **What is fixed:** the zero-target hint becomes `hintSettings`/`hintRescan` properties from the existing table (macOS shows `⌥S`/`⌥R`), `Hold.qml` uses `hintFont`, and `Alt+A` gets a hint. Comma and period at the first or last rung are accepted (consumed, no index change) so they never insert into the filter. The private-mode notice is cleared by selection change from the keyboard, not by hover.
- **Alternative rejected: global key capture with an `Item.focus` trick.** It couples the picker to focus order and would break typing digits.

## Risks
| Risk | Mitigation |
|---|---|
| `bundle:` icons now load on the GUI thread and could stall the picker on a cold cache | Icons are small and cached by `QFileIconProvider`; measure picker show latency on the Mac in the owner gate, and revert to async with a GUI-thread pre-warm if it regresses |
| SIGTERM handler changes shutdown ordering | The handler only writes one byte; all Qt work happens on the GUI thread. Verified by `systemctl --user restart lane.service` and a clean `~Controller` |
| The normalizer or the strict check refuses a URL the old code opened | A table test runs the real `normalizeInput` then `isSafeOpenUrl` chain over spaces, `%20`, bare `%`, IDN, unicode paths, IPv6 literals, ports, query and fragment, and asserts each still opens |
| Diff-updating the target list could desync the model from `Controller` | Order is persisted by `targetId`, never by row index; a test drives hide, rename and move and compares model order to `controller.targetOrder` |
| Retroactive spec could rubber-stamp already-written code | Every decision lists its invariant and a test name; the gstack eng review checks each invariant against the code, not the prose |

## Open questions
- NAT64 local-use prefix `64:ff9b:1::/48` and other embeddings are not decoded. Filed as a follow-up in #78.
- Whether `bundle:` icon latency is acceptable is only measurable on the Mac.
