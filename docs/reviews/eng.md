# Lane engineering review, round 3

Scope: `src/core`, `src/app`, `src/qml`, `tests`, `data`, `CMakeLists.txt`,
CI workflows, packaging, and docs that make claims about behavior. Read at
HEAD `1de0d59` (main, 53 commits past v0.2.0). Source read only; no daemon
interaction, no config writes. `cmake --build build` and
`QT_QPA_PLATFORM=offscreen ctest --test-dir build` run: all 12 tests pass.

## Verdict

**SHIP.** Every round-2 fix verified in source and held, and the wave-3
hardening is real: the flatpak argv gate scans position-independently and
requires `run` plus an app id, the click snapshot kills the stale-URL
launch class, and the config durability story (quarantine, atomic write,
failure notification) is complete and tested. What remains: one silent
discovery gap that has already bitten in production (flatpak targets
invisible when the daemon's environment lacks `XDG_DATA_DIRS`), an X11
picker whose `closeOnFocusLoss` name does not match what it does, a
fingerprint miss that degrades Flatpak Edge to a generic target, and a
handful of defense-in-depth and doc nits.

## Round-N fix verification

| # | Item | Status | Evidence |
|---|------|--------|----------|
| 1 | `flatpakRunArgsBlocked` scans dangerous opts position-independently | **Held** | launcher.cpp:107-123 scans every token for `--command`/`--env`/`--filesystem`/`--socket`/`--device` (both `=` and separate-token values) before the subcommand check at 125-138, so `flatpak --command=sh run app.id` is blocked. The first non-option token must be exactly `run` and a dotted app id must be present (136-147). Tested: test_launcher.cpp:347-352 (pre-subcommand `--command=sh`), 289-308 (non-run subcommands), 310-345 (global options before `run`). |
| 2 | Symlink-to-flatpak: canonical basename checked | **Held** | launcher.cpp:315-318 checks both `QFileInfo(exe).fileName()` and the canonical path's basename before applying the flatpak argv gate. Tested: test_launcher.cpp:354-378. |
| 3 | Click snapshot: launch paths carry Click by value | **Held** | `requestActivationAndLaunch`'s `finish` lambda captures `click` by value (Controller.cpp:803-810); `launch()` reads `click.openUrl`, never `m_click` (763-786). `startHold` snapshots `m_holdClick` (932) and both the animation-finished path (954) and `confirmHold` (971) pass it. Verified empirically on Qt6 that `QAbstractAnimation::stop()` does not emit `finished()`, so a new click during a hold cancels it cleanly instead of launching the held URL (Controller.cpp:383-386). |
| 4 | `execPrefix` env-unwrap: `-i`/`-u`/`-C`/`--` handled; residual gaps fail closed | **Held** | discovery.cpp:113-159 skips env option flags, value-taking options (`-u`, `-C`, `-S`, `--unset`, `--chdir`, `--split-string`), then assignments. `-S`/`--split-string` is now handled (it is in `envOptWithValue`, 126-130). The remaining gap is `-a`/`--argv0`: `env -a name firefox` unwraps to program=`name`, which produces a dead target (or the wrong binary if `name` resolves), never a shell. Fail-closed. Entries that unwrap to nothing are dropped at scanDesktopFiles:281. Tested: test_discovery.cpp:734-837. |
| 5 | `recentTargetIds`/`toastMs` fully gone | **Held** | No occurrence in `src/`, `tests/`, `docs/config.schema.json`, or `AGENTS.md`. Only the CHANGELOG removal note remains. |
| 6 | No `org.kde.layershell` QML import; CMake requires only the lib | **Held** | Zero matches in `src/qml/`. CMakeLists.txt:57 `find_package(LayerShellQt REQUIRED)`; src/CMakeLists.txt:75 links `LayerShellQt::Interface`. `configureLayerShell` null-checks and falls back to a normal window (Controller.cpp:886-890). |

Also re-verified from round 2: `m_pendingUrls` capped at 4, newest kept
(Controller.cpp:372-376); `applyDecision` hides the picker on any non-Pick
decision (730-733); `handleArgs` strips argv[0] unconditionally (319-321);
`dragId` cleared unconditionally in all four `onDropped` handlers
(TargetsPage.qml:93, 180, 267, and the custom-targets list); picker
section order is fixed via `kSectionOrder` (PickerModel.cpp:139-145);
`openSettings` rescans via `reload()` (Controller.cpp:473); `holdMs` is
400-5000 in schema, spinbox, and setter; chromium missing-profile-dir
check has a real body now (discovery.cpp:832-835, tested at
test_discovery.cpp:879-925); update redirects and release URLs are
host-pinned to github.com/api.github.com (updatedecision.cpp:13-23,
84-87); non-rate-limit 403s no longer report as rate limiting
(updatedecision.cpp:48-57).

## New findings

**1. [P2] discovery.cpp:989-997: a daemon started without `XDG_DATA_DIRS`
silently loses every flatpak target, and nothing warns.**

`defaultDiscoveryPaths` builds `applicationDirs` solely from
`QStandardPaths::standardLocations(ApplicationsLocation)`, which is
`XDG_DATA_DIRS` plus `XDG_DATA_HOME`. A daemon launched from a context
missing that variable (a bare systemd unit, a session that did not export
it) scans only the default `/usr/local/share:/usr/share` set: flatpak
exports under `/var/lib/flatpak/exports/share` and
`~/.local/share/flatpak/exports/share` are never seen, so every flatpak
browser vanishes from the picker with no log line and no UI signal. The
settings-open rescan (Controller.cpp:473) does not help; it re-reads the
same environment. Observed in production. Two cheap fixes, do both:
append the two well-known flatpak export dirs unconditionally (they are
stable paths, not env-derived), and `qWarning` when a `flatpak`
executable exists but no flatpak desktop entries were found.

**2. [P3] Picker.qml:10,63: on X11 `closeOnFocusLoss` does not observe
focus loss, and keyboard input is not guaranteed.**

The picker is a frameless, always-on-top, screen-sized `Window`. On
Wayland the layer-shell exclusive keyboard grab makes focus semantics
moot. On X11 (the `configureLayerShell` null path, Controller.cpp:887)
it is a normal window: `closeOnFocusLoss` is wired only to the backdrop
MouseArea (Picker.qml:63), and nothing connects `onActiveChanged` or
`onActiveFocusChanged` to `cancelPicker`. Alt-Tab away and the window
stays up covering the screen until the user clicks the dim region or
Escapes. Worse, `requestActivate()` on a frameless always-on-top window
is not guaranteed focus under every X11 WM; if focus is refused, the
Shortcuts and the filter field are dead and the only escape is the
backdrop click. Same shape in Hold.qml:10,18-21, though there the cost
is lower since the hold auto-launches anyway. Fix: on the non-layer-shell
path, bind `onActiveChanged: if (!active && controller.closeOnFocusLoss)
controller.cancelPicker()` (and equivalent for hold), or accept focus
loss as cancel unconditionally on X11.

**3. [P3] discovery.cpp:454-456: Flatpak Edge fingerprints as Generic, so
it gets no profile discovery and no flatpak profile path.**

The brand check needs `microsoft-edge` or `msedge` in the blob. The real
flatpak id is `com.microsoft.Edge` and the desktop Name is "Microsoft
Edge": lowercased that is `microsoft edge`, which matches neither token.
The entry falls to `genericBrowser` (890-906): one default target, no
profiles, no incognito row, and no `~/.var/app` data dir. Chrome survives
only because its Name happens to be "Google Chrome" (446). Any flatpak
browser whose Name lacks the hyphenated brand string hits the same wall.
Fix: add `microsoft edge` (and `edge` as a word, not substring, to avoid
matching "ledger" style names) to the blob check, or match the flatpak
app id directly when `isFlatpak` is true.

**4. [P4] launcher.cpp:100-123: the dangerous-option scan overclaims;
sandbox-widening options pass unchecked.**

The comment says these are options "whose value names what executes or
widens the sandbox", but the block only fires when the option's value is
a blocked-interpreter basename. `--filesystem=host`, `--socket=session-bus`,
`--device=all`, `--env=LD_PRELOAD=...` all pass, since `host`,
`session-bus`, `all`, and `LD_PRELOAD=...` are not interpreter names.
`--talk-name=org.freedesktop.Flatpak` (the classic sandbox-escape
primitive) is not in the set at all. Only `--command` is meaningfully
gated. Related: versioned interpreter names bypass both this check and
`isBlockedInterpreterChain` (`--command=python3.13`, or a custom target
whose exec is literally `python3.13`, a real file that is not a symlink
to `python3`). Defense-in-depth only: reaching this needs write access
to config.json, and a discovered desktop file can already name any
binary. Still, either narrow the comment to what the gate does or block
the widening options outright for `Kind::Custom` targets.

**5. [P4] Zombie remoting: no detection or mitigation for "already
running but not responding".**

`launchTarget` returns `startDetached`'s spawn result (launcher.cpp:343);
a crashed browser whose xdg-dbus-proxy still owns the remoting name
reports success and the user gets the browser's own not-responding
dialog on every launch. The in-sandbox `--profile` path fix
(discovery.cpp:682-688) removed the most common cause, but a genuinely
wedged instance still produces it. There is no cheap correct fix from
Lane's side (the dialog lives in the browser; detecting it means
watching for a window Lane did not spawn). Worth a known-limitations
note in AGENTS.md so the next reviewer does not re-investigate, and a
tray-menu "browser looks stuck" hint is about the ceiling of what is
reasonable.

**6. [P4] discovery.cpp:126-130: `env -a`/`--argv0` still unwraps wrong.**

`env -a myname firefox %u` skips `-a` as a flag, then takes `myname` as
the program: the discovered target's exec is `myname`, which either
fails to resolve (dead target that shows in settings) or launches the
wrong binary. Add `-a`/`--argv0` to `envOptWithValue`. Fail-closed in
practice, which is why it is P4.

**7. [P4] urlutil.cpp:156-192: `isPrivateOrLocalHost` misses integer and
hex IPv4 literals.**

`QHostAddress` does not parse `http://2130706433/` or `http://0x7f000001/`,
but browsers resolve both to 127.0.0.1. A shortener redirect to a
decimal-IP loopback URL passes the private-host check at
unshorten.cpp:56 and becomes the open URL. Impact is bounded (the URL
still opens in a real browser; Lane never fetches it beyond the HEAD it
already made), but the check's intent is defeated for these forms.
Parsing a bare-integer or `0x` host before the `QHostAddress` attempt
closes it.

**8. [P4] Controller.cpp:817: the activation-token fallback discards the
click's own token.**

When a Lane-owned window exists, `requestActivationAndLaunch` ignores
`m_pendingActivationToken` entirely: the 300ms fallback calls
`finish(QString())`. A click that arrived over D-Bus carrying a valid
token, then went through the picker, loses that token if the compositor
never answers the second request. Rare and low-cost (the launch still
happens, just unraised), but `finish(m_pendingActivationToken)` is the
strictly better fallback.

**9. [P4] discovery.cpp:918,955: stale firefoxpwa config produces dead
PWA rows.**

`discoverPwas` reads `firefoxpwa/config.json` and emits a target per
site even when `findExecutable("firefoxpwa")` comes back empty; exec
falls back to the literal string and every launch fails with the
generic launch-failed notification. Skip the sites (or the whole
discovery) when the binary is absent.

**10. [P4] Doc and comment drift.**

- launcher.cpp:100-101: comment claims the scan blocks options that
  "widen the sandbox"; it does not (finding 4).
- config.cpp:225-227: still says unrecognized keys are "silently
  ignored"; they have warned since round 1.
- unshorten.cpp:27: User-Agent is hardcoded `Lane/0.1`; the project is
  past 0.2.0. Use `LANE_VERSION_STRING`.
- docs/config.schema.json:121-127 still lists `title` and `process` as
  rule locations; both can never match (SourceInfo.cpp:6-11 stub).
  AGENTS.md is honest about it; the schema is not. Carried from round 2.
- CHANGELOG 0.1.0 still advertises matching "by URL, window title, or
  source process". Carried from round 2.

## Considered and fine

**Re-entrancy around `unshortenSync`'s nested loop is fully guarded.**
`m_inOpenUrl` queues re-entrant `openUrl` calls with their own captured
tokens (Controller.cpp:367-378), the queue is bounded at 4, and the
drain restores each queued call's token into the environment before
recursing (409-415). `pickId` and `confirmHold` invoked during the
nested loop act on `m_click`/`m_holdClick`, which still belong to the
click whose UI is actually up. A `reload()` re-entered through the same
loop assigns `m_config`/`m_targets` in place, so the `const Config &`
held by `runPipeline` stays valid and the decision simply uses the
newer config. Consistent.

**`stop()` on the hold animation does not emit `finished()`.** Verified
empirically on Qt6 (QVariantAnimation, state Running -> stop()): no
`finished` signal. A new click during a hold cancels it rather than
launching the held URL, which is the right semantic.

**`flatpakRunArgsBlocked` subcommand logic matches flatpak's own parse.**
First non-dash token must be `run`; `--` before it is consumed as the
options terminator; a dotted app id must appear after. `flatpak
--installation x run` is correctly blocked because flatpak itself would
take `x` as the subcommand. `flatpak run --command=sh` with no app id is
blocked twice over (dangerous opt, then missing app id).

**`isBlockedInterpreterChain` is fail-closed in both directions.** Empty
exec, missing file, self-referential link, and >40 hops all return true;
the walk checks every hop's basename so `python3 -> python3.14` is
caught at the first hop. `resolveExecutable` deliberately does not
canonicalize first (launcher.cpp:225-232), which is what makes the walk
meaningful.

**`expandArgs` cannot re-substitute.** `$urlEncoded` uses
`QUrl::toPercentEncoding`, so a URL containing a literal `%u` or `$url`
arrives percent-encoded and the `$url` pass cannot produce a second
substitution. Newline injection via `$urlEncoded` is tested
(test_launcher.cpp:61-71).

**Config durability is complete.** Corrupt JSON is quarantined to
`config.json.corrupt-<timestamp>` with a warning either way
(config.cpp:213-221, tested at test_config.cpp:174-197); writes go
through `QSaveFile` (376-384); a failed `persist()` raises a
`save-failed` KNotification (Controller.cpp:697-711) and the event id
exists in data/app.lane.Lane.notifyrc. `migrateLegacyConfig` copies
never moves and never overwrites an existing new-path file
(config.cpp:170-196).

**UpdateChecker shell is correct.** Manual redirect policy, per-hop
`isSafeUpdateRedirect` (now host-pinned), 3-hop cap, `deleteLater`
before the stale-reply check, `check()` refuses re-entry while
Checking, relative `Location` resolution against the reply URL.
`CookieSaveControlAttribute` is Manual. The decode half is unit tested
including the host pin (test_updatechecker.cpp:124-140).

**`isSafeOpenUrl` placement is right.** Private-host blocking applies to
unshorten redirects, update redirects, release URLs, `openExternalUrl`,
and remembered writes, but not direct launches, so LAN links still
open. Userinfo is rejected outright (urlutil.cpp:212-214) and stripped
again in `sanitizedOpenUrl`.

**`launch()`'s empty-target fallback is unreachable but harmless.** Every
`Launch` decision carries a target; the branch re-ranks the picker for
the current click and shows it, which is the correct shape if a future
caller ever hands it an empty target.

**`handleArgs` is correct post-fix.** argv[0] stripped unconditionally;
`--settings`/`--rediscover` fire their slots inline and still fall
through to URL handling; a bare `lane` with no args opens settings only
when not `--daemon`.

**`moveIdAmongSiblings` matches the QML contract** (kind + incognito
sibling set, hidden rows included on both sides). Tested.

**`applyConfigToTargets` is idempotent** under reapplication onto the
live list: hidden is assigned not OR-ed, custom targets append only when
missing by id, aliases are re-read, and the order sort is stable.

**Gecko container gating is sound.** `containerHandlerStatus` refuses
containers when extensions.json is readable and names no handler, falls
back to the custom-name heuristic only when the file is unreadable, and
`ext+container` args keep the flatpak prefix in front
(discovery.cpp:651-656).

**`launchProfileDir` translation is correct.** Only host paths under
`~/.var/app/<id>/` are rewritten to the in-sandbox spelling; native
installs and non-matching paths pass through (discovery.cpp:682-688).

**`fingerprint` grouping dedupes correctly.** The signature is
exec+brand+dataDir, so a native and a flatpak install of the same brand
stay separate groups while two desktop files for one install collapse
(discovery.cpp:1017-1025).

**PickerModel role 262 in Picker.qml:29 is `SectionRole`** (UserRole+6).
Magic number, but QML cannot name the C++ enum; standard practice.

**`rankForPicker` computes on every click** including direct launches
(router.cpp:124). Wasted work, a few dozen appends; not worth
restructuring.

**Autostart Exec line is unquoted** (Autostart.cpp:41-48). Breaks only
with spaces in `applicationFilePath()`; no normal install produces
that. Cosmetic spec violation, carried from round 2.

**`version.cpp` prerelease compare** can overflow `toLongLong` on a
>19-digit numeric identifier. Semver-conformant input cannot trigger
it. Carried, not worth fixing.

**`isO365Wrapper` apex miss is fine**; Microsoft only issues regional
subdomains.

**Incognito exclusion from the picker is deliberate** (router.cpp:95);
private windows remain reachable via rules and remembered destinations.

**`openRequested` multi-URL loop is sequential** (main.cpp:148-152):
each URL gets its own pipeline and decision; a multi-URL D-Bus
activation opens each in turn. Intended.

## Method

Read in full: `Controller.cpp` (1016), `Controller.h`, `main.cpp`,
`UpdateChecker.cpp`, `PickerModel`, `RuleModel`, `SourceInfo`,
`Autostart`, `TargetModel`; `config.cpp`, `discovery.cpp` (1146),
`launcher.cpp`, `router.cpp`, `pipeline.cpp`, `matcher.cpp`,
`destination.cpp`, `urlutil.cpp`, `unshorten.cpp`, `updatedecision.cpp`,
`updatemessages.cpp`, `version.cpp`, `types.h`, `repoinfo.h`;
`Picker.qml`, `Hold.qml`, `Settings.qml`, `TargetsPage.qml`,
`RulesPage.qml`, `OverviewPage.qml`, `PreferencesPage.qml`; all 11 test
files; `data/` desktop/service/notifyrc/metainfo/systemd;
`.github/workflows/ci.yml` and `release.yml`; `packaging/PKGBUILD`;
`CHANGELOG.md`, `AGENTS.md`, `docs/config.schema.json`, prior
`docs/reviews/eng.md`.

Verified empirically: `QAbstractAnimation::stop()` does not emit
`finished()` on Qt6 (compiled and ran a minimal QVariantAnimation
probe). Ran `cmake --build build` and `ctest --test-dir build`: 12/12
pass. Not run per constraints: the daemon, `lane <url>`, settings
window, any window on the live session.

Inferred vs verified: all file:line references verified by reading
source. Finding 2 (X11 focus behavior) is a code-structure inference
against documented X11/WM behavior, not reproduced on a live X11
session. Finding 5 (zombie remoting) is confirmed absent as a
mitigation; the user-facing symptom is reported from production.
