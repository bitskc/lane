# Proposal: round-2-hardening

## Why
A second deep review of Lane (2026-10-18/19) ran six lenses against `main` plus the unmerged macOS work: portability, concurrency, code quality, QML, fuzzing under ASan/UBSan, and an independent sweep. The write-ups are `docs/reviews/{portability,concurrency,quality,qml,fuzz}.md`. They found no P0 and one class of real defects: **input that crosses a trust or thread boundary was accepted on a string comparison or parsed with no budget.**

Most of those fixes were implemented before this change existed. This proposal is therefore **retroactive for the implemented items** and **forward for the rest**. The proposal says which is which so the record is honest. GitHub issue #81 tracks the implemented set; #71, #72, #73 and #78 track the forward set.

## What changes

### Implemented before this spec (retroactive, tracked in #81)
Each row is a root cause, not a symptom.

| # | Root cause | Fix |
|---|---|---|
| 1 | `Info.plist` parsing had no depth, node or size budget | Depth 32, 20,000 nodes, 8 MiB converted-plist cap; over-limit bundles are dropped, not partially parsed |
| 2 | The first-run flag outlived an explicit Settings open | `openSettings()` clears it; macOS `--rediscover` and `--pick` no longer trigger onboarding |
| 3 | Linux default-handler state came from an early-`break` loop that labelled any miss "partial" | Pure function `linuxDefaultHandlerState`; Partial only when exactly one scheme is Lane |
| 4 | `g_macBundlePaths` was shared unlocked with Qt's image-loader thread | Built locally, published under a mutex; `bundle:` icons load on the GUI thread |
| 5 | `unshortenSync` ran a full nested event loop, so user input and Quit ran mid-click | `ExcludeUserInputEvents`; `aboutToQuit` flag stops routing and launching; SIGTERM routed through a self-pipe to `quit()`; `--rediscover`/`--settings` deferred while a click is in flight; callbacks hold `QPointer`; D-Bus `Open` restores the token per URI |
| 6 | `isPrivateOrLocalHost` compared the raw host string | Trailing and ideographic/fullwidth dots folded before every check |
| 7 | `normalizeInput` used `QUrl::fromUserInput`, which invents an origin from an authority it cannot parse and decodes `%20` | `normalizeInput` keeps the origin when a scheme is present and emits fully encoded; `isSafeOpenUrl` parses strictly; both share one forbidden-control predicate (U+2028 and U+2029 included) |
| 8 | One regex parsed both `.webloc` and `.url`, and the size cap trusted `size()` | Per-format parse, `isFile()` before `open`, XML entity decode |
| 9 | `%u` expanded as a substring | Token-boundary match |
| 10 | Windows interpreter basenames were listed without `.exe` | Added |

### Forward work (this change specifies it, then implements it)
- **#73** `ensureEngine` keeps the engine pointer after a failed QML load, so it can never retry. Delete the engine on failure. Remove dead members left by the platform seam (`Controller::save()`, the `pickerVisibleChanged` signal, the unreachable LRU branch in `NotificationActionMap`).
- **#78** `isPrivateOrLocalHost` unwraps only `::ffff:` mapped addresses. Decode NAT64 (`64:ff9b::/96`) and 6to4 (`2002::/16`) embedded IPv4 and run it through the existing private-range check.
- **#71** Settings sections are mouse-only, the sidebar divider and rule rows set `width` on layout children, and the target lists rebuild on every change.
- **#72** (narrowed, see design) The zero-target hint and Hold bypass the per-platform hint table.

## Explicitly not in this change
- **#74 Windows.** Needs a go/no-go on supporting Windows at all. No code, no spec.
- **#75 socket peer credentials.** Needs a decision: document the accepted risk or add `SO_PEERCRED`/`getpeereid`.
- **#76 Flatpak `--command` allowlist.** Needs a decision: a strict allowlist can break hand-written custom targets.
- **#77, #80 macOS owner-attended gate and overlay.** Need a person at the Mac.
- **#79 macOS notification identifier and theme icons.** Cosmetic, deferred.
- Filter-field key handling in the Picker (part of #72). See design: it is a Qt constraint, not a bug, and moving those keys to window shortcuts would stop digits from being typed.

## Capabilities
- `lane`: MODIFIED `Security` (URL parsing, private-host classification, link-file handling, argument expansion). MODIFIED `Settings window` (keyboard reachability and stable lists). ADDED `Single in-flight click`, `Plist scan budget`, and `Clean shutdown`.
- `cross-platform`: no change.

No requirement in `native-macos-app` is modified. The two changes touch disjoint requirements and can archive in either order.

## Non-goals
- No new features, no new settings, no behavior change for any input that was already handled correctly.
- No shims or compatibility paths. Where a fix replaces a mechanism (for example `fromUserInput` to `StrictMode`) the old mechanism is removed, not kept behind a flag.
