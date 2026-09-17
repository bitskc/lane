# Lane engineering review, round 2

Scope: `src/core`, `src/app`, `src/qml`, `tests`, `data`, `CMakeLists.txt`,
CI workflows, and docs that make claims about behavior. Read at HEAD
`94602cf` (main, 16 commits past v0.2.0). Source read only; no builds, no
daemon interaction, no config writes. `git log v0.1.0..HEAD` reviewed for
the flatpak discovery, drag-reorder, rename, and blank-name changes.

## Verdict

**SHIP.** Every round-1 fix verified in source and held. The flatpak
discovery work is careful: `execPrefix` keeps `flatpak run ... <app-id>`
in front of Lane's own flags, `@@u` forwarding markers are stripped, and
the profile-store lookup prefers `~/.var/app/<app-id>` only when the Exec
line actually invokes flatpak. The drag/rename rework reuses `m_targets`
correctly and `applyConfigToTargets` is idempotent under reapplication.
What remains is a short list of real but narrow bugs: an async launch
that can pair a stale target with a newer click's URL, a picker that
stays up showing the previous click's rows after a queued click already
launched, and a `flatpak run --command=` hole in the interpreter
blocklist. None of these block a release.

## Round-1 fix verification

| # | Round-1 finding | Status | Evidence |
|---|---|---|---|
| 1 | `openUrl()` re-entrancy via `unshortenSync` nested QEventLoop | **Held** | `m_inOpenUrl` guard at Controller.cpp:360-364 queues the second call into `m_pendingUrls` with its own captured activation token; the queue drains after `applyDecision` at 389-400. `m_click` can no longer be overwritten mid-pipeline. |
| 2 | `configureLayerShell()` null deref on non-wlroots | **Held** | Null check at Controller.cpp:862-866: `if (!ls) { qWarning(...); return; }` with a comment noting X11/non-wlroots falls back to a normal window. |
| 3 | Rediscovery on every mutation | **Held** | All five mutators (`hideTarget` 484, `addCustomTarget` 547, `removeCustomTarget` 574, `renameTarget` 603, `moveTarget` 636) call `applyConfigToTargets(m_targets, m_config)` on the live list. `discoverTargets` only runs in `reload()` (651). `removeCustomTarget` correctly erases the id from `m_targets` first since reapply only appends (590-598). |
| 4 | UpdateChecker network logic untested | **Held, partially** | `tests/test_updatechecker.cpp` (121 lines, 10 cases) covers `decodeUpdateReply`: 200 new/same/older, 404, pure network error, 403/429 rate-limit with and without `x-ratelimit-reset`, malformed JSON, missing fields, unsafe release URL, unparsable tag, and `isSafeUpdateRedirect` scheme/host checks. Still untested: the `handleReply` shell itself (redirect-following loop, stale-reply guard at UpdateChecker.cpp:71, hop cap). The decode half, which is where the bugs would live, is covered. |
| 5 | unshorten untested | **Held, partially** | `tests/test_unshorten.cpp` covers the two pre-network gates (non-shortener passthrough, unsafe-URL passthrough, empty URL) with a comment honestly stating the live HEAD-request path is unreachable without a mock endpoint. The redirect-resolution body (unshorten.cpp:39-62) remains untested. |
| 6 | `remembered` map no GC | **Fixed** | `reload()` calls `clearDeadRemembered()` at Controller.cpp:668, after `m_ruleModel->setRules()` so `persist()` can't clobber the just-loaded rules (comment at 659-667 documents the ordering constraint). Dead-target entries are pruned on every reload; entries whose target still exists but whose host is gone stay, which is correct. |
| 7 | `isDefaultBrowser()` spawns xdg-settings per read | **Held** | `m_isDefaultBrowser` cache at Controller.cpp:159-175; `refreshDefaultBrowserState()` runs only from `reload()`, `makeDefaultBrowser()`, and `openSettings()` (455), so an external change is picked up when Settings opens. |
| 8 | Config silently ignores unknown keys | **Fixed** | `knownKeys` set at config.cpp:231-248 logs `qWarning` naming each unrecognized top-level key. The comment at config.cpp:225-227 still says unrecognized keys are "silently ignored"; stale, harmless. |
| 9 | `urlInScope` prefix match not segment-aware | **Fixed** | urlutil.cpp:113-144 now normalizes trailing slashes and requires exact match or `scope + '/'` boundary; comment cites `destinationKeyMatches` doing the same. Tested at test_url.cpp:38-45. |
| 10 | qputenv/qunsetenv around startDetached | **Addressed** | launcher.cpp:238-253 documents why the env mutation is safe (single-threaded event loop, no re-entrant `launchTarget`) and why `setProcessEnvironment` can't be used with the detached overload. Correct as written. |
| 11 | SourceInfo stub; title/process rules can never fire | **Partially fixed** | `activeSource()` still returns `{}` (SourceInfo.cpp:6-11). Mitigations landed: `ruleMatches` warns once per process that title/process rules cannot match (matcher.cpp:37-45), the Rules page no longer offers those locations (RulesPage.qml:11-12 scopes are domain/path/any only), and AGENTS.md:103-104 documents them as parsed-but-dead. Still true that a hand-edited config with `location: "title"` silently never matches; the schema (config.schema.json:132-137) still lists both values. |

## New findings

**1. [P3] Controller.cpp:768-794 — an in-flight `requestActivationAndLaunch` can launch the wrong URL into the picked target.**

`finish` captures `target` by value but `launch()` reads `m_click.openUrl`
at fire time (748). Sequence: click A shows the picker, user picks a row,
`xdgActivationToken` is in flight; click B arrives, runs its pipeline, and
updates `m_click`. When A's token resolves (or the 300ms fallback fires),
`launch(A's target, ...)` opens **B's URL** in A's target. If B's own
decision also launched, the user gets B's URL twice in two different
browsers, and A's URL silently never opens. Same shape via `confirmHold`
(939-948): hold finishes, token request pending, new click lands, stale
`finish` pairs old target with new URL. Narrow window (sub-second, needs a
second click inside it) but the failure is a wrong-destination open, which
is the one thing a link router must not do. Fix: capture the click's URL
(or a generation counter) in `finish` and bail if `m_click` moved on.

**2. [P3] Controller.cpp:694-717 — a queued click that decides Launch leaves the picker up showing the previous click's rows.**

`applyDecision` only touches the picker on `Pick` (705-706). If the picker
is visible for click A and queued click B decides `Launch` or `Hold`, B's
URL opens (or the hold bar appears) while the picker stays up with A's
rows. A subsequent `pick()` then launches B's URL into whatever row the
user selects, since `pickId` reads `m_click` which is now B's. The model
still shows A's targets, so the picker has silently retargeted. Fix:
`hidePicker()` in `openUrl` before `applyDecision`, or in `launch()`/
`startHold()`.

**3. [P3] launcher.cpp + discovery — `flatpak run --command=` bypasses the interpreter blocklist; `Exec=env ...` desktop files produce dead targets.**

Two sides of the same gap. The blocklist (launcher.cpp:57-75) added `env`,
`xargs`, `sudo`, `flatpak-spawn`, etc. because a wrapper can re-exec a
blocked interpreter through its args. `flatpak` itself is not blocked (it
must not be; discovered flatpak browsers need it), and nothing inspects
args, so a hand-edited `customTargets` entry `flatpak run --command=sh
some.app -c '...'` sails through `parseCustomCommand` and `launchTarget`.
That is exactly the `env bash -c` shape round 1 closed, one level down in
argv. Sandboxed to the app's permissions, and it needs config write access,
so defense-in-depth only, but the asymmetry with `flatpak-spawn` being
blocked is worth closing (scan args for `--command`/`--talk-name` style
escapes, or reject `flatpak run` custom targets without a dotted app id).

The flip side: a real desktop file with `Exec=env FOO=1 firefox %u` is
discovered with `exec` = `env` (firstToken, discovery.cpp:36-45), which
`isBlockedInterpreterChain` then rejects at launch. The target shows in
settings and the picker and silently fails every time. Discovery should
either unwrap leading `env VAR=...` assignments or drop the entry.

**4. [P3] TargetsPage.qml:79-89 (and the three sibling lists at 150, 221, 286) — `dragId` is not cleared on a cancelled drop.**

`onDropped` only resets `dragId` inside `if (newIndex >= 0 && dragId !==
"")`. If Kirigami emits `dropped` with `newIndex == -1` for a drop outside
the list (its documented cancel signal), `dragId` stays set. The next
drag's `onMoveRequested` skips capture because `dragId` is non-empty, so
`browserModel.move` moves the row the user grabbed while
`controller.moveTarget` persists a move for the *previous* row's id. The
visual list and `targetOrder` diverge until the next sync. One-line fix:
clear `dragId` unconditionally in `onDropped`.

**5. [P4] Controller.cpp:309-338 — `handleArgs` strips argv[0] by `contains("lane")`.**

For a D-Bus `Activate` the first arg is the caller's argv[0]. If the binary
is invoked through a differently-named symlink or renamed copy that does
not contain "lane", argv[0] falls through to the `!a.startsWith('-')`
branch and is treated as a URL: `openUrl("browser")` becomes
`http://browser` and opens or shows a picker for a nonsense host. Also
line 335: `rest.contains("--settings") == false && rest.isEmpty()` is dead
code, since `rest.isEmpty()` already implies the contains check. Minor.

**6. [P4] updatedecision.cpp:13-16, 60-69 — update release URL and redirects are not host-pinned to GitHub.**

`isSafeUpdateRedirect` accepts any https non-private host, and
`decodeUpdateReply` accepts any `html_url` passing the same check. A forged
API response (compromised CA, enterprise TLS proxy) could point "a newer
version is out" at an arbitrary phishing page. Low likelihood, and the
worst case is opening a web page, but pinning to `github.com` costs one
line. Related nit: any 403 is reported as rate limiting (34-39), including
403s that are not.

**7. [P4] discovery.cpp:724-727 — dead check in `chromiumProfiles::addProfile`.**

`if (!QDir(...).exists() && key != "Default") { /* comment */ }` has an
empty body; the `continue` that presumably belonged there is missing.
Stale `info_cache` entries for deleted profile dirs become targets that
fail on launch. Gecko profiles do check existence (651).

**8. [P4] Dead config fields: `recentTargetIds` and `toastMs`.**

`launch()` prepends to `m_config.recentTargetIds` and persists on every
successful open (Controller.cpp:757-762); nothing ever reads the list.
`toastMs` is loaded (config.cpp:256) and saved (328) but never consumed.
Both are in the published schema. Either wire them up or drop them;
`recentTargetIds` also means every link open writes the config file.

**9. [P4] discovery.cpp:74-102 — `execPrefix` splits args on spaces without honoring quotes.**

A leading quoted program is handled (78-84), but the rest of the Exec line
is split on `' '` with no quote or escape processing. `Exec=flatpak run
--command="zen browser" app.id` yields `--command="zen` and `browser"` as
separate args, corrupting the argv Lane rebuilds. Rare in real flatpak
exports (they use `--command=name` without spaces), but the parser claims
to match `firstToken`'s quoting rules and does not.

**10. [P4] Controller.cpp:360-363 — `m_pendingUrls` is unbounded.**

Every D-Bus `openRequested` during a nested unshorten loop appends. A burst
of opens serializes into a long queue of stale launches, each potentially
opening a browser window minutes after the click. A small cap (drop or
coalesce beyond N) would bound it. Local-only, low severity.

**11. [P4] Changelog/docs drift.**

CHANGELOG 0.2.0 says of remembered destinations "Nothing is removed
automatically"; `reload()` now auto-prunes dead-target entries
(Controller.cpp:668). CHANGELOG 0.1.0 still advertises rules matching "by
URL, window title, or source process"; title/process can never match
(SourceInfo.cpp). AGENTS.md is honest about it; the changelog is not.

## Considered and fine

**Flatpak app-id extraction is sound for real exports.**
`flatpakAppId` (discovery.cpp:110-119) takes the first non-`--` token
matching the reverse-DNS pattern. `flatpak run` puts the app id after all
options, and every option that takes a separate-token value
(`--command`, `--runtime`, `-d`) either uses `=` form or its value does
not match the dotted pattern. A bare `1.2`-looking token could
theoretically shadow the real id, but no real export emits one.

**`@@u` marker stripping is correct.** `isExecFieldCode`
(discovery.cpp:62-72) treats any `@@`-prefixed token as a marker, which
covers the unpaired `@@u` opener and the `@@` closer; the comment explains
why exact-match was wrong. Tested at test_discovery.cpp:287.

**`expandArgs` sequential replacement is safe in practice.** The `$url`
then `%u` passes could re-substitute if the URL itself contained a literal
`%u`, but `QUrl::fromUserInput` normalizes a bare `%` to `%25` before the
URL reaches `expandArgs` (verified against Qt 6: `a%ub` becomes `a%25ub`).
Unreachable.

**The nested-loop config swap is not UB.** `runPipeline` takes `const
Config &` bound to `m_config`; a `reload()` re-entered through
`unshortenSync`'s event loop assigns `m_config` in place, so the reference
stays valid and the pipeline finishes on the new config. `pickId` during
the same window still acts on the previous `m_click`, which is the click
whose picker is actually showing. Consistent.

**`m_pendingActivationToken` lifecycle is correct.** `openUrl` captures and
unsets the env token up front (349-352), stores it per-invocation (366),
and the queue drain restores each queued call's own token into the
environment so it is captured identically (394-399). A stale token cannot
leak across clicks.

**`moveIdAmongSiblings` matches the QML contract.** Sibling set is
`(kind, incognito)` (discovery.cpp:1011-1021), which is exactly what each
settings ListModel contains; `newIndex` from `onDropped` is an index into
that same set. Hidden rows are in the model and in the sibling set on both
sides, so indices agree. Clamping and unknown-id paths tested
(test_discovery.cpp:625-675).

**Rename path cannot inject.** `renameTarget` (603-634) writes
`targetAliases` keyed by an existing target id and re-syncs custom target
names; aliases for dead ids are inert. The 1ms `renameTimer` deferral
(TargetsPage.qml:40-46) avoids mutating the model inside
`editingFinished`; a second edit inside 1ms would overwrite the pending
rename, but that requires two focus-loss events inside a millisecond.

**UpdateChecker shell is correct.** Manual redirect policy, per-hop
https+non-private vetting, 3-hop cap, `deleteLater` before the stale-reply
check, `check()` refuses to re-enter while `Checking`. The stale-reply
guard (71) is defensive but unreachable today since `check()` early-returns.

**`isSafeOpenUrl` placement is right.** Private-host blocking applies to
unshorten redirects, update redirects, release URLs, `openExternalUrl`,
and `remembered` writes (pickId:419), but not to direct launches, so LAN
links still open. Userinfo is stripped by `sanitizedOpenUrl` before launch.

**Picker/hold QML load failure is loud enough.** `ensurePickerEngine`
logs a warning and leaves `m_pickerWindow` null; `showPicker` then emits
`pickerVisibleChanged(true)` with no window, so a click falls silent, but
only in a broken-install scenario where the QML module itself is missing.

**`isO365Wrapper` host check.** `endsWith(".safelinks.protection.outlook.com")`
misses the bare apex, but Microsoft only issues regional subdomains; the
apex does not serve safelinks.

**Incognito targets excluded from the picker is deliberate.**
`rankForPicker` drops `t.incognito` (router.cpp:95); private windows are
reachable via rules, remembered destinations, and the settings list. The
changelog documents the exclusion from drag ordering; the picker exclusion
keeps the row count sane.

**`route()` computes `rankForPicker` on every click** including
direct-launch decisions (router.cpp:124). Wasted work per click, but it is
a few dozen list appends; not worth restructuring the Decision shape.

**`version.cpp` prerelease compare** can overflow `toLongLong` on a
>19-digit numeric identifier and compare equal; semver-conformant but
absurd input. Not worth fixing.

**Autostart Exec line is unquoted** (Autostart.cpp:41-48). Breaks only if
`applicationFilePath()` contains spaces, which no packaged or normal
`~/.local` install produces. Cosmetic spec violation.

**`data/app.lane.Lane.desktop.in` registers `text/html`** so Lane is
offered for local .html files it then blocks as non-http(s). Minor UX
wart, pre-existing.

## Method

Read in full: `Controller.cpp` (991), `Controller.h`, `main.cpp`,
`UpdateChecker.cpp/.h`, `PickerModel`, `RuleModel`, `SourceInfo`,
`Autostart`, `TargetModel`; `config.cpp`, `discovery.cpp` (1037),
`launcher.cpp`, `router.cpp`, `pipeline.cpp`, `matcher.cpp`,
`destination.cpp`, `urlutil.cpp`, `unshorten.cpp`, `updatedecision.cpp`,
`updatemessages.cpp`, `version.cpp`, `types.h`, `repoinfo.h`;
`Picker.qml`, `Hold.qml`, `Settings.qml`, `TargetsPage.qml`,
`RulesPage.qml`, `OverviewPage.qml` (behavior paths); all 11 test files;
`data/` desktop/service/notifyrc/metainfo; `.github/workflows/ci.yml` and
`release.yml`; `CHANGELOG.md`, `AGENTS.md`, `docs/config.schema.json`,
`docs/designs/gstack-full-analysis.md`, prior `docs/reviews/eng.md`.

Verified empirically where cheap: `QUrl::fromUserInput` percent and
bare-word behavior via PyQt6 (the `%u` re-substitution path is
unreachable; bare `browser` becomes `http://browser`). Not run per
constraints: builds, ctest, the daemon, `lane <url>`, settings window.

Inferred vs verified: all file:line references verified by reading source.
Finding 1 (stale-target/wrong-URL launch) and finding 4 (dragId on
cancelled drop) are code-structure inferences; the Kirigami `dropped(-1)`
contract is per its documentation, not reproduced at runtime.
