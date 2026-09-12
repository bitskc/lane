# Lane engineering review

Scope: `src/core`, `src/app`, `src/qml` (behavior paths), `tests`, `data`,
build/CI, and docs that make claims about behavior. Read at HEAD `fe77477`
(branch `lane-screenshots`, same commit as `origin/main`). `ctest --test-dir
build --output-on-failure`: 10/10 pass (1.37s). Read-only CLI run: `lane
--version` (0.1.0), `lane --config-path`, `lane --list` (44 targets), `lane
--explain https://github.com/bitskc/lane`. No windows, overlays, or daemon
restarts were triggered.

## Verdict

**SHIP WITH FIXES.** The routing core (`router.cpp`, `destination.cpp`,
`matcher.cpp`, `pipeline.cpp`) is small, well-factored, and backed by real
behavioral tests. All five round-1 fixes held. The interpreter blocklist,
QSaveFile write path, config migration, and picker sizing are all correct
at HEAD. What remains is quality debt, not breakage: two network code paths
(`UpdateChecker`, `unshorten`) have zero direct test coverage, every settings
mutation re-runs full browser discovery, and `openUrl()` has a re-entrancy
window through `unshortenSync`'s nested event loop that the prior review
already flagged and is still open. None of these block 0.1.0, but the
re-entrancy and the test gaps should close before 0.2.

## Blocking

None.

## Should fix

**1. [P2] (confidence: 9/10) `src/app/Controller.cpp:330-364`,
`src/core/unshorten.cpp:31-37` — `openUrl()` has no re-entrancy guard; a
second click during unshorten corrupts in-flight state.**

`openUrl()` calls `runPipeline(url, m_config, unshortenFn())` which calls
`unshortenSync(url, 1800)` which blocks on a `QEventLoop` for up to 1800ms
on any URL whose host is one of the 23 known shorteners. The nested loop
keeps Qt's event dispatcher running, so a second D-Bus `Open`/`Activate`
arriving mid-unshorten re-enters `openUrl` while `m_click`,
`m_holdAnimation`, and `m_pendingActivationToken` are mid-update from the
first call. The second call overwrites `m_click` (line 350); the first call
resumes with the wrong `m_click` and routes/launches against it. Two links
clicked in quick succession, one a `t.co`/`bit.ly` link, is a plausible
everyday trigger. Fix: add an in-flight guard (a bool or a deferred queue)
at the top of `openUrl`, or move unshorten off the synchronous-nested-loop
pattern (async `QNetworkAccessManager` with a callback, like
`UpdateChecker` already does).

**2. [P2] (confidence: 9/10) `src/app/Controller.cpp:448,516,532,552,561`
— every settings mutation re-runs full `discoverTargets()`.**

`hideTarget`, `addCustomTarget`, `removeCustomTarget`, `renameTarget`, and
`moveTarget` each call `discoverTargets(defaultDiscoveryPaths())`, which
re-scans all desktop files, Gecko profile directories, Chromium `Local
State` files, and PWA manifests. On this machine (44 targets), that is a
noticeable pause on every toggle, drag, rename, or add. Discovery is not
on the click path (confirmed: `openUrl()` never calls `discoverTargets()`),
but it makes the settings UI feel sluggish. `renameTarget` and `moveTarget`
do not change what is installed; they only reorder or relabel existing
targets. `hideTarget` only flips a config flag. None of these need a full
rescan. Fix: for mutations that do not change the installed set (rename,
reorder, hide), call `applyConfigToTargets(m_targets, m_config)` on the
existing target list instead of rediscovering. Reserve full
`discoverTargets()` for `addCustomTarget`, `removeCustomTarget`, and
`rediscover()`.

**3. [P2] (confidence: 9/10) `src/app/UpdateChecker.cpp:71-158` —
`handleReply()` has zero test coverage.**

`UpdateChecker` is 172 lines of real network logic: manual redirect vetting
(https-only, private-host-checked, 3-hop cap), HTTP status branching (403/429
rate-limit with `x-ratelimit-reset` parsing, 404, 200), JSON parsing,
`tag_name`/`html_url` extraction, release-URL safety re-check, and version
comparison via `compareVersions`. The `describe*` message functions are
tested in `test_version.cpp`, but the actual `handleReply()` control flow
is not. A stale-reply guard (line 74: `reply != m_reply`) is untested. The
redirect safety check (line 93-94: `location.scheme() == "https" &&
!isPrivateOrLocalHost(location.host())`) is untested. The class is testable
without a real network: `handleReply()` takes a `QNetworkReply *`, and Qt's
`QNetworkAccessManager` can be driven by a local `QHttpServer` or by
injecting a fake reply. Known issue #2.

**4. [P2] (confidence: 9/10) `src/core/unshorten.cpp:15-63` —
`unshortenSync()` has zero direct test coverage.**

63 lines of real HTTP/redirect logic: HEAD request with manual redirect
policy, `QEventLoop` with timeout, three-tier `Location` header extraction
(`RedirectionTargetAttribute`, `LocationHeader`, raw header with
null/length checks), relative-URL resolution, and redirect-target safety
(`isSafeOpenUrl` + `isPrivateOrLocalHost`). Only the pipeline's
injected-function seam is exercised (`test_pipeline.cpp::unshortenHook`),
never the real network code. The raw-header fallback (line 46-49:
`!raw.isEmpty() && !raw.contains('\0') && raw.size() < 4096`) is the kind
of defensive code that should have a test proving it works. Same testability
as UpdateChecker: local `QHttpServer` or mock `QNetworkReply`.

**5. [P3] (confidence: 7/10) `src/app/Controller.cpp:770-781` —
`LayerShellQt::Window::get()` dereferenced with no null check or platform
guard.**

`configureLayerShell()` calls `LayerShellQt::Window::get(window)` and
immediately calls `ls->setLayer(...)` on the result. `CMakeLists.txt` makes
`LayerShellQt` a hard `REQUIRED` build dependency, so the app is
Wayland-layer-shell-only by construction, but nothing at runtime checks
`QGuiApplication::platformName()` or branches on a compositor lacking
`wrl-layer-shell`. If the picker window ever fails to get placed as an
overlay (X11 session, XWayland fallback, a non-wlroots Wayland compositor),
this is a null-pointer crash for a default-browser handler: no picker, no
error, just a segfault. I run Wayland and could not exercise this path
without killing the live daemon. Fix: add an explicit
`platformName() != "wayland"` guard at startup that refuses to run with a
clear stderr message, or null-check `ls` and fall back to a normal window.

## Follow-ups

**[P3] `remembered` map has no garbage collection.** `config.cpp:252-255`
loads `remembered` verbatim; `saveConfig` writes it verbatim. Dead entries
(browser uninstalled, profile deleted) accumulate forever.
`Controller::clearDeadRemembered()` (line 488-499) exists but is manual-only
(settings page button). `danglingRememberedKeys()` is computed and exposed
but never called automatically. Consider pruning on `reload()` or on target
disappearance.

**[P4] `src/core/launcher.cpp:250-260` — `qputenv`/`qunsetenv` around
`startDetached` is process-wide mutation.** Still single-threaded on the
Qt GUI event loop. No background thread touches environment variables. Safe
today. The prior review's note stands: worth a comment if the app ever grows
a worker thread that calls `getenv`.

**[P4] `src/app/Controller.cpp:159-165` — `isDefaultBrowser()` spawns
`xdg-settings` on every property read.** `Q_PROPERTY(bool isDefaultBrowser
READ isDefaultBrowser NOTIFY defaultBrowserChanged)`. The signal fires from
`reload()` (line 579), which runs on startup and on `rediscover()`. Each
read starts a `QProcess` with a 1500ms timeout. Not a hot path, but caching
the result and only re-checking on `makeDefaultBrowser()` would be cheaper.

**[P3] `src/app/SourceInfo.cpp:6-11` — `activeSource()` is a stub.** Always
returns empty. `m_click.processName` and `m_click.windowTitle` are always
empty. Rules with `location: "title"` or `location: "process"` can never
match. `AGENTS.md:103-104` documents these as working features, and the
settings UI exposes them. Either implement them or remove them from the
UI and docs. The in-code comment is honest about the limitation; the docs
are not.

**[P4] Config schema not validated at runtime.** `docs/config.schema.json`
has `additionalProperties: false` and lists all 22 fields, matching
`config.cpp` exactly. But `loadConfig()` silently ignores unknown keys (Qt
JSON reader just does not read them). A hand-edited config with a typo
(`"pickerPolcy"`) is silently treated as default with no warning. A
`qWarning()` on unrecognized top-level keys would catch typos that
`AGENTS.md:19` explicitly invites.

**[P4] `src/core/urlutil.cpp:133` — `urlInScope` uses prefix match, not
path-segment match.** `up.startsWith(sp, Qt::CaseInsensitive)` matches
`/bitskc/lane` against scope `/bits`, which is probably not intended.
`destinationKeyMatches` (destination.cpp:159) does proper segment-aware
matching (`urlPath.startsWith(keyPath + '/')`). The inconsistency is
unlikely to bite in practice (PWA scopes are typically origin-wide or exact
paths), but it is a latent correctness gap if someone sets a mid-segment
PWA scope.

## Round-1 fix verification

| Finding | Status | Evidence |
|---------|--------|----------|
| Interpreter blocklist bypassed by absolute-path symlink | **Held** | `isBlockedInterpreterChain()` (launcher.cpp:112-142) walks `QFileInfo::symLinkTarget()` hop-by-hop, called in `launchTarget()` (line 235) and `targetFromJson()` (config.cpp:146). Tests: `launchTargetRejectsSymlinkToBlockedInterpreter` (test_launcher.cpp:144), `customTargetsDropSymlinkToBlockedInterpreter` (test_config.cpp:80). Both create a real symlink to `/bin/sh` and assert rejection. |
| Blocklist never inspected `target.args`; `/usr/bin/env bash -c` bypass | **Held** | `env` and 17 other re-exec wrappers added to `blockedInterpreters()` (launcher.cpp:57-75). Tests: `launchTargetRejectsEnvReExecWrapper` (test_launcher.cpp:162), `customTargetsDropEnvReExecWrapper` (test_config.cpp:119). Both set `exec="/usr/bin/env"`, `args=["bash","-c",...]` and assert rejection. |
| `Controller::persist()` discarded `saveConfig()`'s QSaveFile result | **Held** | `persist()` (Controller.cpp:582-600) checks `saveConfig()` return value; on failure fires `KNotification("save-failed")` with `setComponentName("app.lane.Lane")`. `data/app.lane.Lane.notifyrc` has matching `[Event/save-failed]` section. |
| No test for `migrateLegacyConfig()` | **Held** | Four tests in test_config.cpp: `migrateLegacyConfigFreshCopy` (line 193), `migrateLegacyConfigIdempotentOnSecondRun` (line 220), `migrateLegacyConfigDestinationExistsWins` (line 249), `migrateLegacyConfigFailureLeavesSourceIntact` (line 278). All call the two-arg overload directly. |
| Picker height hardcoded "2 section headers" | **Held** | `PickerModel::applyFilter()` (PickerModel.cpp:147) sets `m_sectionCount = sectionOrder.size()`. Exposed as `Q_PROPERTY(int sectionCount READ sectionCount NOTIFY countChanged)` (PickerModel.h:19). `Picker.qml:187` uses `controller.pickerModel.sectionCount` in the height formula. |

## Considered and fine

**Unshorten is not an SSRF proxy.** `urlutil.cpp`'s `kShorteners` is a
fixed 23-entry allowlist. `pipeline.cpp:25` only invokes the network call
when `isShortener(working)` is true. `unshorten.cpp:17` double-gates on
`isSafeOpenUrl`. The `QNetworkAccessManager` never connects to an
attacker-chosen host, only to one of the 23 literal domains. The
`Location`-header safety check is string-based, not resolved-IP-based, so a
compromised shortener could redirect to a hostname that resolves to a
private IP without the literal matching `isPrivateOrLocalHost`. But Lane
never connects to that redirect target; it only proposes it as the URL to
hand to the browser, exactly as if the user pasted the link. This matches
`DESIGN.md`'s stated scope.

**UpdateChecker redirect handling is correct.** `UpdateChecker.cpp:88-101`
handles redirects manually: each hop is checked for `https` scheme and
non-private host before being followed, capped at 3 hops, and fails closed
with `describeUnsafeRedirect` on an unsafe target or hop exhaustion. The
release URL itself is re-checked with `isSafeOpenUrl` and
`isPrivateOrLocalHost` before being accepted (line 142). No cookies
persisted (`setCookieJar(nullptr)`, line 37).

**Config schema and `config.cpp` are in sync.** `docs/config.schema.json`
lists 22 properties with `additionalProperties: false`. All 22 match fields
read/written by `loadConfig`/`saveConfig`. No drift.

**D-Bus surface is standard KDBusService.** No custom adaptor; just
`org.freedesktop.Application` (`Activate`, `Open`, `ActivateAction`) and
`org.kde.KDBusService.CommandLine`. `Open()` runs the same `isSafeOpenUrl`-
gated pipeline a real click would. Any local process calling it has no
more power than running `lane <url>` on the CLI.

**Custom handler argv, never a shell, holds up.** `launcher.cpp:253` calls
`QProcess::startDetached(exe, expandArgs(target, open))` with a real argv
array. `expandArgs` (line 82-105) substitutes `$url`/`$urlEncoded` into
individual argv tokens. `isSafeOpenUrl` gates every URL before it reaches
`expandArgs`, so a URL cannot begin with `-`. Verified by
`test_launcher.cpp`'s `urlEncodedPlaceholderNeverInjectsNewline` and
`refusesFileUrlLaunch`.

**Router first-match and conflict-under-Never fix is correct and tested.**
`router.cpp:146-174`: multiple matching rules with the same target launch
without a picker; genuine conflicts under `Never` policy honor the first
rule (line 170-174) instead of falling through. Test:
`conflictHonorsFirstRuleWhenPickerNever` (test_router.cpp:154).

**QSaveFile write path is correct.** `config.cpp:355-363`: writes to a
temp file, checks byte count, calls `cancelWriting()` on short write, only
`commit()`s on full success. `corruptConfigMovedAsideNotDiscarded`
(test_config.cpp:151) proves the quarantine path preserves exact bytes.

**`version` field with no migration code is fine pre-1.0.** `config.cpp`
round-trips `version` but nothing branches on it. `CHANGELOG.md` states
"while the major version is 0, minor releases may still contain breaking
changes." Every field load falls back to a sensible default.

**Discovery cost is not on the click path.** `openUrl()` (Controller.cpp:330)
never calls `discoverTargets()`. Discovery runs from the constructor, from
`rediscover()`, and from settings mutations (see Should-fix #2). The seeded
suspicion that "rediscovery runs on every settings open" is not confirmed:
`openSettings()` (line 415) calls `ensureSettingsEngine()` and shows the
window; it does not call `reload()` or `discoverTargets()`. The QML pages
do not call `rediscover()` on load (verified by grep). The OverviewPage has
a manual "Rediscover browsers" button.

**`destinationKeyMatches` is segment-aware.** `destination.cpp:159`:
`urlPath == keyPath || urlPath.startsWith(keyPath + '/')`. A key
`github.com/bitskc` does not match `https://github.com/other`. Test:
`lookupRememberedDoesNotStealSibling` (test_destination.cpp:61).
`suggestedLadderIndex` cannot write a broader key than the user saw: the
key written is `currentDestinationKey()` which is
`m_destinationLadder.value(m_destinationIndex)`, and the user controls
`m_destinationIndex` via comma/period keys and the ‹ › buttons.

## Method

Read in full: `src/core/launcher.cpp` (262 lines), `launcher.h`, `config.cpp`
(365 lines), `config.h`, `unshorten.cpp` (63 lines), `pipeline.cpp` (66
lines), `urlutil.cpp` (235 lines), `urlutil.h`, `router.cpp` (223 lines),
`router.h`, `destination.cpp` (230 lines), `destination.h`, `discovery.cpp`
(925 lines, structural read), `discovery.h`, `matcher.cpp` (52 lines),
`matcher.h`, `version.cpp` (168 lines), `version.h`, `updatemessages.cpp`
(71 lines), `updatemessages.h`, `repoinfo.h`, `types.h` (195 lines),
`src/app/Controller.cpp` (895 lines), `Controller.h` (211 lines),
`UpdateChecker.cpp` (172 lines), `UpdateChecker.h`, `main.cpp` (154 lines),
`PickerModel.cpp` (152 lines), `PickerModel.h`, `SourceInfo.cpp`,
`SourceInfo.h`.

QML read for behavior: `Picker.qml` (395 lines), `Hold.qml` (142 lines).
Settings pages checked for discovery triggers via grep (none found on
load).

Tests read in full: all 10 ctest suites (`test_url.cpp`, `test_matcher.cpp`,
`test_pipeline.cpp`, `test_discovery.cpp`, `test_router.cpp`,
`test_config.cpp`, `test_launcher.cpp`, `test_destination.cpp`,
`test_version.cpp`).

Docs read: `docs/config.schema.json`, `AGENTS.md`, `CLAUDE.md`, prior
reviews (`pr-1.md`, `pr-1-round2.md`, `eng.md` being replaced).

Run: `ctest --test-dir build --output-on-failure` (10/10 pass, 1.37s).
`lane --version` (0.1.0), `lane --config-path`
(`/home/andy/.config/lane/config.json`), `lane --list` (44 targets),
`lane --explain https://github.com/bitskc/lane` (pick, reason: picker).

Not run (per constraints): `lane <url>`, `lane --pick`, `lane --settings`,
`lane --daemon`, `xdg-settings`, `xdg-mime`, no daemon kill/restart, no
screenshots. LayerShellQt null-deref (Should-fix #5) is a source read, not
a reproduction; I run Wayland and could not exercise the failure path
without killing the live daemon.

Inferred vs verified: all file:line references are verified by reading the
source. The re-entrancy finding (Should-fix #1) is inferred from the code
structure (nested `QEventLoop` + no guard) and the prior review's analysis,
not reproduced at runtime. The LayerShellQt null-deref (Should-fix #5) is
inferred from the absence of a null check, not reproduced.
