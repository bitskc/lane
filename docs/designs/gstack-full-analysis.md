# Lane: full gstack analysis

Synthesis of five independent reviews run 2026-09-12 against Lane v0.1.0
(`docs/reviews/eng.md`, `docs/reviews/ceo.md`, `docs/reviews/design.md`,
`docs/reviews/devex.md`, `docs/reviews/openspec.md`). This document does
not re-review the code; it reconciles what the five reviewers already
found, resolves the three places they point in different directions,
and orders everything into one fix list. See
`docs/designs/lane-office-hours.md` for the product-strategy framing.

## Executive summary

One day after v0.1.0 was tagged and PR #1 (rename + security fixes)
merged, the picture is: the engineering is past the point where more
features help, and all five round-1 fixes held. What stands between this
and being safe to hand to a stranger is not a capability gap. It is two
crash-class bugs in a default-browser handler (re-entrancy through
unshorten's nested event loop, null deref on non-wlroots compositors),
a set of docs that describe features that do not exist (CLI flags the
binary rejects, rule locations the code stubs out, a schema minimum the
UI clamps above), and a GitHub repo rename that blocks the update
checker, the PKGBUILD checksum, and every doc link. The openspec change
is stale, was never promoted to canonical spec, and contradicts the
shipped product on containers, picker width, and memory scope. None of
this is hard to fix. The single most important move is: rename the repo,
land the two crash fixes and the doc fixes, tag 0.2.0, publish to AUR,
and post to r/kde pointing at Junction issue #9. Everything else is a
guess until a stranger installs it.

## Verdicts

| Review | Verdict | Reason |
|---|---|---|
| Eng (`docs/reviews/eng.md`) | **SHIP WITH FIXES** | Routing core is small, well-factored, and tested. All five round-1 fixes held. Re-entrancy through `unshortenSync`'s nested `QEventLoop` and the `LayerShellQt::Window::get()` null deref are crash-class in a default handler; close both before 0.2. Two network code paths (`UpdateChecker`, `unshorten`) have zero direct test coverage (`eng.md:13-22`) |
| CEO (`docs/reviews/ceo.md`) | **SHIP 0.2.0 AS THE DISTRIBUTION RELEASE** | Engineering is past the point where more features help. What is missing is not another capability; it is a single stranger who is not Andy. The Unreleased changelog already holds a release's worth of work. Rename the repo, publish the PKGBUILD to AUR, tag 0.2.0, post to r/kde (`ceo.md:7-9`) |
| Design (`docs/reviews/design.md`) | **Scores up; one thing: holdAutoOpen default** | Eight dimensions scored 5-8, all improved since last round. The headline issue is `holdAutoOpen` still defaulting to true at 1600ms, contradicting DESIGN.md's "either nothing visible" promise on every remembered/PWA/default open (`design.md:22-28`) |
| Devex (`docs/reviews/devex.md`) | **TTHW ~51s; docs describe features that do not exist** | Build with deps present is fast. But `lane --rediscover` and `lane --configure` exit 1 with "Unknown option" because `QCommandLineParser` never registers them. All user-facing URLs point at `bitskc/lane` which 404s. `appstreamtest` is a false green on the README path (`devex.md:53-55,56-59,18`) |
| Openspec (`docs/reviews/openspec.md`) | **STALE: spec contradicts shipped product** | 3 uncovered requirements, 8 shipped features with no task line, `openspec validate` fails (no deltas), `openspec/specs/` does not exist, change was never archived. The repo has no canonical spec (`openspec.md:7,47,107-117`) |

## Cross-review consensus

Three findings were reached independently by different reviewers working
from different parts of the codebase. Independent convergence is the
strongest kind of signal in a five-review set, so each is named here as
one theme rather than scattered across separate line items.

**Docs describe a product that does not exist.** The eng review found
that `SourceInfo.cpp:6-11` is a stub returning empty, so rules with
`location: "title"` or `location: "process"` can never match, yet
`AGENTS.md:103-104` documents them as working features
(`docs/reviews/eng.md:136-142`). The devex review found that
`lane --rediscover` and `lane --configure` are in README, AGENTS.md, and
CHANGELOG but `QCommandLineParser` (`main.cpp:90-103`) never registers
them, so they exit 1 with "Unknown option"
(`docs/reviews/devex.md:53-55`). The devex review also found that
`docs/config.schema.json:73` sets `holdMs` minimum to 0 while
`PreferencesPage.qml:59-60` clamps to 400-5000
(`docs/reviews/devex.md:79`). The openspec auditor found that the
proposal says "no containers" but containers shipped, the design doc
says "remembered host" but the code does path-scoped destination
ladders, and the design doc says 520px card but the picker is 440px
(`docs/reviews/openspec.md:51-67`). Three reviewers, one root cause:
docs were written for intent, not behavior.

**The GitHub repo rename blocks everything.** The CEO review's first
0.2.0 item is renaming the repo, because the update checker 404s on
every press and the PKGBUILD cannot produce a real checksum
(`docs/reviews/ceo.md:57`). The devex review found that every
user-facing URL (README, CHANGELOG, metainfo, PKGBUILD,
`test_version.cpp`) points at `bitskc/lane` which does not exist, while
the git remote is still `bitskc/tern` (`docs/reviews/devex.md:56-59,103`).
The eng review confirmed the update checker and repoinfo point at the
nonexistent repo. One decision unlocks the release, the AUR checksum,
the update checker, and every doc link. The code already committed to
the rename in PR #1; the GitHub side just has not happened yet.

**Containers: convergent scope-creep flag.** The CEO review found that
containers shipped after the 0.1.0 tag, before a single outside user
had touched the core product, and reach 1.9K bridge-extension users out
of 409K who have Multi-Account Containers (`docs/reviews/ceo.md:47-49`).
The openspec auditor independently found that the proposal explicitly
listed containers under "Out of scope" and the spec was never updated
when they shipped (`docs/reviews/openspec.md:24,51-55`). This is not a
call to rip containers out. It is a discipline call: stop adding
features that serve a near-empty audience until the core product has
real users. If 0.2.0 ships containers, the README should be honest
about the 1.9K-user bridge dependency, not advertise containers as a
top-line feature (`docs/reviews/ceo.md:51`).

## Round-1 fix verification

All five round-1 fixes held. Verified in source by the eng review
(`docs/reviews/eng.md:162-169`).

| Finding | Status | Evidence |
|---|---|---|
| Interpreter blocklist bypassed by absolute-path symlink | **Held** | `isBlockedInterpreterChain()` (launcher.cpp:112-142) walks `QFileInfo::symLinkTarget()` hop-by-hop. Tests: `launchTargetRejectsSymlinkToBlockedInterpreter` (test_launcher.cpp:144), `customTargetsDropSymlinkToBlockedInterpreter` (test_config.cpp:80). Both create a real symlink to `/bin/sh` and assert rejection. |
| Blocklist never inspected `target.args`; `/usr/bin/env bash -c` bypass | **Held** | `env` and 17 other re-exec wrappers added to `blockedInterpreters()` (launcher.cpp:57-75). Tests: `launchTargetRejectsEnvReExecWrapper` (test_launcher.cpp:162), `customTargetsDropEnvReExecWrapper` (test_config.cpp:119). Both set `exec="/usr/bin/env"` and assert rejection. |
| `Controller::persist()` discarded `saveConfig()`'s QSaveFile result | **Held** | `persist()` (Controller.cpp:582-600) checks `saveConfig()` return value; on failure fires `KNotification("save-failed")` with `setComponentName("app.lane.Lane")`. `data/app.lane.Lane.notifyrc` has matching `[Event/save-failed]` section. |
| No test for `migrateLegacyConfig()` | **Held** | Four tests in test_config.cpp: `migrateLegacyConfigFreshCopy` (line 193), `migrateLegacyConfigIdempotentOnSecondRun` (line 220), `migrateLegacyConfigDestinationExistsWins` (line 249), `migrateLegacyConfigFailureLeavesSourceIntact` (line 278). All call the two-arg overload directly. |
| Picker height hardcoded "2 section headers" | **Held** | `PickerModel::applyFilter()` (PickerModel.cpp:147) sets `m_sectionCount = sectionOrder.size()`. Exposed as `Q_PROPERTY(int sectionCount READ sectionCount NOTIFY countChanged)` (PickerModel.h:19). `Picker.qml:187` uses `controller.pickerModel.sectionCount` in the height formula. |

## Contradictions adjudicated

**1. holdAutoOpen default: off or holdMs ~400 (design) vs. keep true at 1600 (CEO).**

The design review says default `holdAutoOpen` to off or cut `holdMs` from
1600 to about 400, because `shouldHold()` runs the hold HUD on every
remembered, PWA, and default open, producing dozens of 1.6s interruptions
per day with trained memory (`docs/reviews/design.md:24-28`). The CEO
review says this is Andy's call and it is fine, because the hold is a
safety net for misrouted clicks and a consultant who loses client trust
from a wrong-identity open has a higher cost than 1.6s of friction
(`docs/reviews/ceo.md:72`). DESIGN.md line 5 promises "either nothing
visible," which supports the design position.

**Recommendation: default `holdAutoOpen` to false.** Remembered paths are
user-confirmed by definition. Rule matches already skip the hold
(`Controller.cpp:802-804`). The picker is the veto for ambiguous paths.
The hold on remembered paths protects against wrong memory, but if
memory is wrong, the user should fix the memory, not veto every open.
The hold HUD is still available for users who want it: Preferences
exposes the toggle and the duration slider (PreferencesPage.qml:54-65).
The cost: a user who never opens Preferences loses the hold veto on
silent opens. But they keep the picker for anything new, and rules still
launch immediately. This is the second consecutive review round flagging
this default against DESIGN.md's own contract. Set `holdAutoOpen = false`
in `types.h:150`.

**2. Ship 0.2.0 now (CEO) vs. ship with fixes first (eng).**

The CEO review says tag 0.2.0 now because the Unreleased changelog
already holds a release's worth of work and what is missing is users, not
code (`docs/reviews/ceo.md:7-9`). The eng review says SHIP WITH FIXES
because the re-entrancy window in `openUrl` and the `LayerShellQt` null
deref are crash-class bugs in a default-browser handler
(`docs/reviews/eng.md:13-22`).

**Recommendation: sequence them.** Land the two P0 fixes and the P1 doc
fixes first, then tag 0.2.0. The re-entrancy fix is a guard at the top of
`openUrl` (a bool or a deferred queue, Controller.cpp:330-364). The null
deref fix is a platform guard or null check in `configureLayerShell`
(Controller.cpp:770). Both are small, local changes. A segfault in a
default-browser handler is the worst possible first impression for a
stranger installing from AUR. The doc fixes (register CLI flags or
remove them, remove or implement dead rule locations, align schema
minimum) should land in the same commit because a stranger following
README instructions that produce "Unknown option" is nearly as bad.
Sequence: (1) rename repo to `bitskc/lane`, (2) land P0 + P1 fixes,
(3) regenerate PKGBUILD checksum, (4) tag 0.2.0, (5) publish to AUR,
(6) post to r/kde.

**3. OpenSpec repair: update-in-place then archive (auditor) vs. something else.**

The openspec auditor recommends updating `proposal.md`, `design.md`, and
`tasks.md` in place to match v0.1.0 shipped behavior, adding `specs/`
deltas or `skip_specs: true`, running `openspec validate` until clean,
then archiving to promote the first canonical spec. The auditor
explicitly recommends against archive-and-rewrite from scratch, because
the change ID `initial-tern` is already referenced by tooling and git
history, and a rewrite would orphan it without adding correctness
(`docs/reviews/openspec.md:134-167`).

**Endorse: update in place, then archive.** The shipped product is
coherent. The gap is documentation and OpenSpec structure, not unknown
requirements. The concrete steps are: move containers from out-of-scope
into scope in `proposal.md`, replace "remembered host" with
"path-scoped remembered destination" in `design.md`, fix the card width
to 440px, drop or implement the blur claim, expand `tasks.md` with the
8 shipped features that have no task line, mark task 10 as done, add
`specs/` deltas or `skip_specs: true`, add a "What Changes" section to
`proposal.md`, validate, and archive. Do not leave `initial-tern`
forever unarchived: as long as it sits in `changes/` with no `specs/`
promotion, every future OpenSpec change starts from zero canonical
baseline.

## Fix list

Ordered by user impact, not by which reviewer raised it loudest.

### P0: crash/data-loss class

| # | Finding | Source | File to change | Fix |
|---|---|---|---|---|
| 1 | `openUrl()` has no re-entrancy guard. `unshortenSync` runs a nested `QEventLoop::exec()` for up to 1800ms on shortener URLs. A second D-Bus `openRequested` during that loop re-enters `openUrl` and overwrites `m_click` mid-pipeline. Two quick clicks where one is a shortener URL is the trigger. | eng (should-fix #1) | `src/app/Controller.cpp:330-364`, `src/core/unshorten.cpp:31-37` | Add an in-flight guard (bool or deferred queue) at the top of `openUrl`, or move unshorten to async `QNetworkAccessManager` with a callback (like `UpdateChecker` already does). Async also fixes the 1.8s blocking UX: no user feedback during unshorten, which all five reviews missed. |
| 2 | `configureLayerShell()` dereferences `LayerShellQt::Window::get(window)` with no null check. Null on X11/non-wlroots. Crash in a default-browser handler: no picker, no error, just a segfault. | eng (should-fix #5) | `src/app/Controller.cpp:770` | Add a `platformName() != "wayland"` guard at startup that refuses to run with a clear stderr message, or null-check `ls` and fall back to a normal window. |

### P1: repo rename, doc fixes that lie to users

| # | Finding | Source | File to change | Fix |
|---|---|---|---|---|
| 3 | GitHub remote is `bitskc/tern`; app, PKGBUILD, README, CHANGELOG, metainfo, update checker all say `bitskc/lane` which does not exist. Update checker 404s on every press. PKGBUILD cannot get a real checksum. Every doc link is broken. | CEO (#1), devex (fix #2), eng | GitHub repo settings, then regenerate `packaging/PKGBUILD` checksum | Rename the repo to `bitskc/lane`. The code already committed to this in PR #1. One-way door, but already open. |
| 4 | `lane --rediscover` and `lane --configure` are in README, AGENTS.md, and CHANGELOG but `QCommandLineParser` (`main.cpp:90-103`) never registers them. They exit 1 with "Unknown option." Verified empirically. | devex (fix #1) | `src/app/main.cpp:90-103` | Register `--rediscover` and `--configure` in `QCommandLineParser` so `parser.process()` accepts them before `Controller::handleArgs` runs. Or remove them from all docs and document restart-only reload. |
| 5 | AGENTS.md documents `location: "title"` and `location: "process"` rule conditions. `SourceInfo.cpp:6-11` is a stub returning empty. These rules can never fire. The settings UI exposes them. | eng (follow-up) | `src/app/SourceInfo.cpp:6-11`, `AGENTS.md:103-104` | Implement `activeSource()` to populate `m_click.processName` and `m_click.windowTitle`, or remove the dead rule locations from the UI and docs. The in-code comment is honest; the docs are not. |
| 6 | `docs/config.schema.json:73` sets `holdMs` minimum to 0. `PreferencesPage.qml:59-60` clamps to 400-5000. A hand-edited `holdMs: 100` loads but the UI will not expose it. | devex (schema drift) | `docs/config.schema.json:73` | Set `minimum: 400` in the schema to match the UI clamp, or document the hand-edit vs UI range. |

### P2: rediscovery-on-mutation, test coverage, openspec repair, holdAutoOpen

| # | Finding | Source | File to change | Fix |
|---|---|---|---|---|
| 7 | `hideTarget`, `addCustomTarget`, `removeCustomTarget`, `renameTarget`, and `moveTarget` each call `discoverTargets(defaultDiscoveryPaths())`, re-scanning all desktop files, Gecko profiles, Chromium `Local State` files, and PWA manifests. Rename, reorder, and hide do not change what is installed. | eng (should-fix #2) | `src/app/Controller.cpp:448,516,532,552,561` | For mutations that do not change the installed set (rename, reorder, hide), call `applyConfigToTargets(m_targets, m_config)` on the existing target list. Reserve full `discoverTargets()` for `addCustomTarget`, `removeCustomTarget`, and `rediscover()`. |
| 8 | `UpdateChecker::handleReply()` has zero test coverage. 172 lines of network logic: redirect vetting, HTTP status branching, JSON parsing, version comparison. The stale-reply guard and redirect safety check are untested. | eng (should-fix #3) | `src/app/UpdateChecker.cpp:71-158`, new test in `tests/test_version.cpp` | Drive `handleReply()` with a local `QHttpServer` or inject a fake `QNetworkReply`. Test the redirect safety check, rate-limit parsing, and stale-reply guard. |
| 9 | `unshortenSync()` has zero direct test coverage. 63 lines of HTTP/redirect logic: HEAD request, manual redirect policy, three-tier `Location` header extraction, relative-URL resolution, redirect-target safety. | eng (should-fix #4) | `src/core/unshorten.cpp:15-63`, new test in `tests/test_pipeline.cpp` | Drive with a local `QHttpServer` or mock `QNetworkReply`. Test the raw-header fallback, redirect loop cap, and safety check. |
| 10 | OpenSpec change `initial-tern` is stale, never archived, has no `specs/` deltas, and `openspec validate` fails. The repo has no canonical spec. Proposal says "no containers" but containers shipped. Design says "remembered host" but code does path-scoped ladders. Design says 520px card but picker is 440px. 8 shipped features have no task line. | openspec (full audit) | `openspec/changes/initial-tern/proposal.md`, `design.md`, `tasks.md`, new `specs/` dir | Update proposal/design/tasks in place to match v0.1.0 shipped behavior. Add `specs/` deltas or `skip_specs: true`. Add "What Changes" section. Mark task 10 done. Run `openspec validate` until clean. Archive to promote first canonical spec. |
| 11 | `holdAutoOpen` still defaults to true at 1600ms. Second consecutive review round flagging this against DESIGN.md's "either nothing visible" contract. | design ("the one thing"), CEO (counter) | `src/core/types.h:150` | Default `holdAutoOpen` to false. See contradiction #1 adjudication above. |

### P3: rest

| # | Finding | Source | File to change | Fix |
|---|---|---|---|---|
| 12 | `remembered` map has no garbage collection. Dead entries (browser uninstalled, profile deleted) accumulate forever. `clearDeadRemembered()` exists but is manual-only. | eng (follow-up) | `src/core/config.cpp:252-255`, `src/app/Controller.cpp:488-499` | Prune dead entries on `reload()` or on target disappearance. |
| 13 | `isDefaultBrowser()` spawns `xdg-settings` on every property read. Each read starts a `QProcess` with a 1500ms timeout. | eng (follow-up) | `src/app/Controller.cpp:159-165` | Cache the result; re-check only on `makeDefaultBrowser()`. |
| 14 | Config schema not validated at runtime. `loadConfig()` silently ignores unknown keys. A hand-edited config with a typo (`"pickerPolcy"`) is silently treated as default. | eng (follow-up) | `src/core/config.cpp` | Add `qWarning()` on unrecognized top-level keys. |
| 15 | `urlInScope` uses prefix match, not path-segment match. `up.startsWith(sp)` matches `/bitskc/lane` against scope `/bits`. `destinationKeyMatches` does proper segment-aware matching. | eng (follow-up) | `src/core/urlutil.cpp:133` | Use segment-aware matching like `destination.cpp:159`. |
| 16 | `qputenv`/`qunsetenv` around `startDetached` is process-wide mutation. Safe today (single-threaded). Worth a comment if a worker thread is ever added. | eng (follow-up) | `src/core/launcher.cpp:250-260` | Add a comment noting the single-thread assumption. |
| 17 | DESIGN.md drift: says six visible rows and blur; code shows eight rows and a tint without blur. | design (should-fix #6) | `DESIGN.md:14-15` | Align doc to code, or accept intentional drift explicitly. |
| 18 | Stale screenshots: `docs/screenshots/*.png` still show "Tern", a six-row picker without section headers, and a flat Targets list without search/collapse. | design (should-fix #7) | `docs/screenshots/*.png` | Refresh after the next visual pass. |
| 19 | Comma/period ladder keys are invisible in the footer. Footer shows chevrons and an elided key but no `,` / `.` hints. | design (should-fix #2) | `src/qml/Picker.qml:324-348` | Add `,` / `.` hints next to the ladder, or tooltips on the chevrons. |
| 20 | Always checkbox omits the chosen target name. Labels `"Always for " + controller.currentDestinationKey` but not the highlighted row's `name`. | design (should-fix #3) | `src/qml/Picker.qml:318-320` | Append the current list selection: "Always open this path in Zen Work." |
| 21 | Settings sidebar nav items lack `Accessible.*` names. Picker and hold are labeled; the settings shell is not. | design (should-fix #5) | `src/qml/Settings.qml:55-97` | Add `Accessible.role` / `Accessible.name` on nav items; mark the active page. |
| 22 | CONTRIBUTING.md build block has only `cmake` commands. A stranger who reads CONTRIBUTING first hits configure failure on a clean box. | devex (fix #5) | `CONTRIBUTING.md:7-11` | Add "install deps from README" or repeat the `pacman` one-liner. |
| 23 | `appstreamtest` is a false green on the README path. Without `cmake --install`, the test logs "Not installed yet, skipping" and still passes. | devex (TTHW) | `README.md` build section | Add `cmake --install` to README build section, or document that `appstreamtest` needs it. |

## Deliberately not doing

Places the suite considered and rejected:

- **Ripping out containers.** The CEO review calls containers scope creep
  but explicitly says "this is not a call to rip containers out"
  (`docs/reviews/ceo.md:51`). The openspec auditor flags the spec
  contradiction but recommends updating the spec, not reverting the code.
  Containers are small, tested, and honestly documented in DESIGN.md.
  The call is to stop adding features for a near-empty audience, not to
  remove what already works.
- **Rewriting OpenSpec from scratch.** The auditor recommends
  update-in-place then archive, not archive-and-rewrite
  (`docs/reviews/openspec.md:134-141`). The change ID `initial-tern` is
  referenced by tooling and git history. A rewrite orphans it without
  adding correctness. The shipped product is coherent; the gap is
  documentation and structure.
- **Chasing distro packaging beyond AUR.** PolyForm Noncommercial blocks
  Debian/Fedora/openSUSE official repos (`docs/reviews/ceo.md:20`). AUR
  does not gate on license. Flathub hosts proprietary apps but adds a
  review queue. AUR is the zero-cost, zero-license-conflict path to the
  first stranger. Flathub can wait until AUR produces signal.
- **Shipping a container-bridge WebExtension.** The prior round's CEO
  review recommended this to reach the full 409K Multi-Account Containers
  user base. The cost the recommendation did not price in: an AMO review
  pipeline, a second release train independent of Lane's CMake/CI/AUR
  cycle, and an ongoing support surface in JavaScript and WebExtensions
  API, a language and ecosystem the rest of Lane does not touch. Wait
  for container-routing demand signal from outside users before
  committing.
- **Relicensing to MIT/GPL now.** No email has asked to pay. No distro
  has rejected the license. Relicensing is a one-way door that
  permanently forecloses the commercial track. Hold until there is real
  signal either way (`docs/reviews/ceo.md:73`).
- **Headless QML smoke test in CI.** The Kirigami 6.28 `borderColor`
  regression it would catch was already found and fixed by hand
  (`docs/reviews/devex.md:92`). Revisit if a second Kirigami API-drift
  bug ships before this test exists.
- **Adding Lua scripting.** Still out of scope. Browser Tamer's Lua
  scripts are a power-user feature that would expand the surface without
  expanding the audience (`docs/reviews/ceo.md:67`).

## What the repo already decided

Settled calls a future session must not silently reverse:

- **PolyForm Noncommercial + separate commercial track.** Free for
  personal/hobby use; commercial use requires contacting Andy. Pricing is
  unset, no checkout, email-only (`COMMERCIAL.md`, `README.md:176-180`).
  Relicensing is a one-way door; hold for signal.
- **Path-scoped memory, not just host.** `github.com/bitskc` can go
  somewhere different from `github.com`. The destination ladder
  (`destination.cpp:124-138`) and comma/period navigation in the picker
  expose this. This is the sharpest piece of routing logic and the one a
  competitor would get wrong (`docs/reviews/ceo.md:71`).
- **Hold HUD on silent opens only.** `shouldHold()`
  (`Controller.cpp:797-804`) runs the hold on remembered, PWA, and
  default opens. Rule matches skip the hold (`Controller.cpp:802-804`).
  The picker is the veto for ambiguous paths. The hold is a safety net
  for trained paths, not a confirmation step for new ones.
- **Absolute Exec paths in the .desktop file.** `app.lane.Lane.desktop.in`
  uses an absolute path so the KDE app menu can find the binary without
  PATH resolution. Required for D-Bus activation.
- **No default-browser theft.** Lane never calls `xdg-mime` or
  `xdg-settings` automatically. The only path to becoming the default
  is a button in Settings (`OverviewPage.qml:23-28`,
  `Controller.cpp:430-438`).
- **http/https only.** `file:`, `javascript:`, `data:`, and
  credentials-in-URL are refused. mailto and PDF are not stolen from
  their own handlers (`DESIGN.md:23,29`).
- **Custom handlers are argv, never a shell.** `QProcess::startDetached`
  with a real argv array. Interpreters (`bash -c`, `python -c`, `env`)
  are rejected by basename and by symlink-chain walk (`launcher.cpp`).
- **Resident daemon, not cold-start.** Cold-start Qt is too slow for a
  picker. The architecture depends on staying resident (`DESIGN.md:13`).
- **No Lua.** Explicitly out of scope, inherited from the Browser Tamer
  model (`DESIGN.md:52`, `proposal.md:11`).
- **Containers shipped but Lane never registers `ext+container` itself.**
  Lane wraps the URL as argv and hands it to the browser. Whether a
  container link lands in the container depends on a third-party
  protocol-handler extension with 1.9K users (`DESIGN.md:7`). The README
  should be honest about this dependency, not advertise containers as a
  top-line feature.
