# Lane: full gstack analysis, macOS port round

Synthesis of 6 review lenses run 2026-10-17 against the shipped macOS port
(44 commits since spec commit `8ba3be2` through `2734da6`).
Review inputs: `docs/reviews/{ceo,eng,design,devex-macos,qa,security}.md`.
Verification: every high-stakes claim re-checked against source before this
document was written.

## Verdict

**Ship-worthy port with one real ship-blocker found and fixed.** The port is
architecturally sound, the Linux regression surface is small and now closed,
and both test suites are green on both OSes (Linux 21/21, macOS 20/20).
The review round found and fixed one genuine user-visible defect class
(Lane claiming file-type ownership it cannot serve) plus a cluster of
platform-gating, UX-lying, and marshalling bugs.

## Round findings and what was done

| # | Sev | Finding | Status |
|---|-----|---------|--------|
| 1 | P0 | `makeDefaultHandler` claimed `public.html` + `public.url` via `toOpenContentType:`; `UrlEventFilter` dropped every `file://` event those claims delivered, so double-clicking .html/.webloc files silently did nothing once Lane was default. Confirmed live: `public.html` -> `app.lane.lane` in secure.plist. | **Fixed** (`2734da6`): dropped the `public.html` claim (url-only now re-points http/https), and `UrlEventFilter` unwraps `.webloc`/`.url` file opens to their embedded link instead of swallowing them. |
| 2 | P1 | "Quit Lane" tray action and `StandardKey.Quit` shortcuts were added unguarded — violating `openspec/specs/lane/spec.md:252` ("tray SHALL NOT offer Quit" on Linux) and adding an undocumented daemon-kill path. | **Fixed**: menu gated `#ifdef Q_OS_MACOS`; QML shortcuts bound to new `quitShortcutEnabled` (false on Linux). |
| 3 | P1 | `LaneNotificationDelegate::didReceiveNotificationResponse` ran `notificationActions().take()` (which invokes Controller code) on UserNotifications' arbitrary queue; every other completion in the same file marshals to main. | **Fixed**: `dispatch_async(dispatch_get_main_queue(), …)`. |
| 4 | P1 | `m_changeDeclined` was set on API-error paths too, so a system rejection (permErr) presented as "user declined" in the Overview banner. | **Fixed**: `finish(false)` no longer sets the flag; it is set only on the accepted-but-handler-is-not-us path. |
| 5 | P1 | `LaneIcon.qml` `sourceSize` bound to `width × DPR` was self-referential → RangeError seen live on the Mac. | **Fixed**: fixed `Qt.size(64,64)` source size. |
| 6 | P2 | `maybeFirstRunSettings()` called unconditionally from `launch()` — Linux first-run link launches would pop Settings (behavior change Linux never had). | **Fixed**: `#ifdef Q_OS_MACOS`. |
| 7 | P2 | The 103-byte `sun_path` fail-fast applied on Linux too (limit is Darwin's; Linux allows 108), tightening a Linux invariant for no reason. | **Fixed**: `#ifdef Q_OS_MACOS`. |
| 8 | P2 | UrlEventFilter header documented the opposite drop policy (oldest) of what the code implements (newcomer); the `qInfo` waiting-count was off-by-one. | **Fixed**: header rewritten, off-by-one removed. |
| 9 | P2 | No macOS CI job; all macOS-only tests run only on the MacBook. | **Open** — add a `macos-latest` job running `cmake + ctest` (no signing). |
| 10 | P2 | Single-instance socket has no peer verification beyond DAC — same-UID process can inject URL frames or pre-bind the path to snoop/drop clicks. | **Open** — accepted risk in spec, but the snoop/drop half is not documented; consider `LOCAL_PEERCRED` check. |
| 11 | P2 | `flatpakRunArgsBlocked` only blocks `--command` values naming interpreters; `--command=/tmp/evil` still substitutes the exec'd program. | **Open** — Linux-side hardening. |
| 12 | P2 | Design: `NSWindowCollectionBehaviorMoveToActiveSpace` alone replaces the specced `canJoinAllSpaces|fullScreenAuxiliary|ignoresCycle`; D5 full-screen-space fallback is not implemented. | **Open** — verify against real fullscreen Space in task 9.3 owner checks. |
| 13 | P2 | tasks.md marks `[x]` items whose own text requires owner-in-person verification (6.3 key hints, 9.2 fixture note that does not exist). | **Open** — correct the checkboxes or add the missing evidence. |
| 14 | P3 | `isPrivateOrLocalHost` misses IPv4-mapped IPv6, integer/hex IPv4, and reserved ranges. | **Open** — defense-in-depth gap. |
| 15 | P3 | `readPlistValue` recursion + unbounded plutil output — crafted Info.plist can crash the daemon during scan. | **Open** — depth/element budget needed. |
| 16 | P3 | Linux `action:email` passes raw URL as `xdg-email --body`, same injection class the macOS path fixed. | **Open**. |
| 17 | P3 | `QFileOpenEvent` for `.html` (not .webloc) still silently logs-and-drops — acceptable if Lane no longer claims `public.html` by default, but a stray "Open With Lane" on an .html file still does nothing. | **Residual, acceptable** — only reachable if the user explicitly picks Lane for .html; noted. |
| 18 | P3 | macOS notification toasts all share identifier `"lane"` — consecutive toasts replace rather than stack; per-request action-category labels can misrender on rapid posts. | **Open** — cosmetic. |
| 19 | P3 | ~1/3 of `QIcon::fromTheme` lookups return null on macOS (no icon theme); some picker/Settings rows get dead gutters. | **Open** — bundle fallback icons or accept the gutters. |
| 20 | P3 | `public.html`/`public.xhtml`/`com.apple.application-bundle` handler entries in secure.plist may be residue of earlier probes, not current calls — worth confirming clean state after the fix ships. | **Verify** in task 9.3. |

## Cross-review consensus

Four independent lenses (Eng, Security, Design, QA) all surfaced the same
root cause — Lane claiming file-type roles it cannot serve — without
coordination. That is the strongest signal in the set and it is now fixed.
Two lenses (QA, Eng) independently flagged the `MaybeFirstRun`/`quit` Linux
behavior changes; both are now gated.

## Deliberately not doing

- **Developer ID / notarization**: the $99/yr Apple Developer Program is a
  product decision, not an engineering fix. Lane works as Apple
  Development-signed for the owner's machine; distribution to strangers
  needs Developer ID and is an open product question, not a bug.
- **Removing `public.html` from `CFBundleDocumentTypes` in the plist**:
  it is needed for Settings-dropdown eligibility (every shipping router
  declares it); only the *handler claim* was the bug, and that is gone.
- **Rewriting `SingleInstance` auth**: DAC + same-UID is the documented
  trust model for both D-Bus (Linux) and the socket (macOS). Hardening is
  a P2 follow-up, not a blocker.
- **macOS CI**: flagged as P2; a GitHub `macos-latest` job is a
  straightforward add but was not in this round's scope.

## What the repo already decided

- D1 Your Mac first, D2 Quit in status menu, D3 `LSUIElement`, D4
  Option-key accelerators, D5 normal top-most window (not
  canJoinAllSpaces), D11 ad-hoc/Apple-Development signing — all stand.
- `toOpenContentType:` replacing `toOpenURLsWithScheme:` is a deliberate
  workaround for a macOS 26 system-level gate, not a regression.
- `LSHandlerRank` absent from `CFBundleURLTypes` is correct prior art
  (OpenIn/Finicky/Browserosaurus all leave schemes unranked).
