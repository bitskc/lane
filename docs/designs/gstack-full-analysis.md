# Lane: full gstack analysis, round 2

Synthesis of four independent reviews run 2026-09-17 against Lane v0.2.0
(`docs/reviews/eng.md`, `docs/reviews/ceo.md`, `docs/reviews/design.md`,
`docs/reviews/devex.md`), five days after the tag. This document does not
re-review the code; it reconciles what the four reviewers already found,
resolves the places they pull in different directions, and orders
everything into one fix list. See `docs/designs/lane-office-hours.md`
for the product-strategy framing. The round-1 version of this file is in
git history.

## Executive summary

v0.2.0 shipped, the repo is `bitskc/lane`, the PKGBUILD has a real
checksum, CI is green, and every round-1 fix held. No review found a
regression and none found a ship-blocker. What remains is a short list
of real but narrow bugs (a wrong-destination launch race, a stale
picker, a `flatpak run --command=` hole in the blocklist), a pile of
docs that describe last week's defaults, and one action no agent can
take: Andy creating an AUR account. The distribution path is validated
end to end and still goes nowhere until `yay -S lane` works. The single
most important move is unchanged from round 1 and smaller than it was:
publish the package, post to r/kde pointing at Junction issue #9, then
stop building until a stranger says something.

## Verdicts

| Review | Verdict | Reason |
|---|---|---|
| Eng (`docs/reviews/eng.md`) | **SHIP** | Every round-1 fix verified in source and held. Flatpak discovery work is careful: `execPrefix` keeps `flatpak run ... <app-id>` in front of Lane's flags, `@@u` markers stripped, profile-store lookup prefers `~/.var/app/<app-id>` only when the Exec line invokes flatpak. Remaining findings are real but narrow: a wrong-destination launch race, a stale picker after a queued click, a `flatpak run --command=` blocklist hole. None block a release. |
| CEO (`docs/reviews/ceo.md`) | **THE ONLY MOVE LEFT IS ANDY'S TEN MINUTES ON AUR** | Everything round 1 asked engineering to do got done: rename, tag, release, update checker, checksummed PKGBUILD. The remaining blocker is not code. It is an AUR account and SSH key only Andy can create. Still 0 stars, 0 forks, 0 watchers; AUR query for `lane` returns zero results. |
| Design (`docs/reviews/design.md`) | **Ship for daily use; fix picker empty state and doc drift before pointing strangers at the README** | Net +5 across eight dimensions. Biggest gain is First 200ms (hold default flipped to opt-in). Biggest losses are stale screenshots still showing Tern branding and a picker filter that shows a dead row on zero matches. |
| Devex (`docs/reviews/devex.md`) | **Good enough for an Arch/KDE contributor; docs still drift** | README build instructions accurate, warm build under a minute, 11 tests green in CI. But README/AGENTS/DESIGN still describe hold as default-on, flatpak discovery is shipped and undocumented, and `appstreamtest` is still a false green outside CI. |

## Cross-review consensus

Three findings were reached independently by reviewers working from
different parts of the repo.

**Doc drift is the round-2 theme.** `holdAutoOpen` flipped to false in
code (`types.h:149`, `config.cpp:261`), schema, tests, changelog, and
metainfo, but `DESIGN.md:44-46` still says "on (default)",
`README.md:47-49` (and 92-94, 101-102) still describes the hold bar as
standard behavior, and `AGENTS.md:52` shows `"holdAutoOpen": true` in
the sample config. Design, devex, and CEO all flagged this
independently. The same shape repeats elsewhere: flatpak discovery
shipped on main with no mention in README, CONTRIBUTING, or AGENTS
(devex); the changelog still advertises title/process rules that can
never fire and says remembered entries are never removed automatically
when `reload()` now prunes them (eng); the screenshots still show Tern
(design). Round 1 flagged the openspec "out of scope" line silently
reversed by shipped code. This is now a pattern, not an accident: the
repo's prose cannot be trusted to match its behavior, which matters
more now that AGENTS.md is pitched at AI agents that read docs instead
of code. The CEO review's concrete suggestion is worth taking: add
"docs match behavior" to the release checklist in RELEASING.md.

**The only remaining strategic blocker is Andy's AUR account.** The
CEO review's top finding is that `yay -S lane` does not work because
nobody has run the ten-minute account setup and `git push` to
`aur.archlinux.org/lane.git`. Devex independently confirmed the
PKGBUILD is structurally sound (real sha256, correct depends, Release
build, license install). A separate distribution check confirmed the
PKGBUILD builds clean, package contents are correct, the release
workflow produced v0.2.0 correctly, and the update checker returns
v0.2.0. Every other finding in every review is secondary to the fact
that no stranger can install the product.

**No regressions.** All four reviews re-verified the round-1 fixes
against source and found them held. The eng review's eleven-row table
is the detailed evidence; design and devex confirm on their own
subsets. The two round-1 items still open are the same two that were
open at the tag: stale screenshots and the `appstreamtest` false green.

## Round-2 fix verification

The full round-1 fix list, re-verified by the round-2 reviews. Statuses
are as reported by the reviewer who checked each item; openspec status
verified directly (`openspec/changes/archive/2026-09-14-initial-tern/`,
canonical `openspec/specs/lane/spec.md`).

| # | Round-1 fix | Status | Evidence |
|---|---|---|---|
| 1 | `openUrl()` re-entrancy via `unshortenSync` nested QEventLoop | **Held** | `m_inOpenUrl` guard at Controller.cpp:360-364 queues the second call into `m_pendingUrls` with its own activation token; queue drains after `applyDecision` (eng #1). |
| 2 | `configureLayerShell()` null deref on non-wlroots | **Held** | Null check at Controller.cpp:862-866 with fallback to a normal window (eng #2). |
| 3 | Repo rename to `bitskc/lane` | **Held** | Remote is `bitskc/lane`, releases API 200, all user-facing URLs resolve, old `bitskc/tern` 301-redirects (ceo #1, devex #2). |
| 4 | `lane --rediscover` / `--configure` unregistered | **Fixed** | Both registered in `QCommandLineParser` at main.cpp:98-106 (devex #1). |
| 5 | `location: "title"` / `"process"` documented but dead | **Partially fixed** | Rules page no longer offers them, matcher warns once, AGENTS.md honest. `SourceInfo.cpp` still a stub; schema still lists both values; hand-edited configs silently never match (eng #11, devex #6). |
| 6 | Schema `holdMs` minimum 0 vs UI clamp 400 | **Fixed** | Schema `minimum: 400`. Residual: controller setter clamps 200-10000, wider than UI/schema (devex #3). |
| 7 | Rediscovery on every mutation | **Held** | All five mutators call `applyConfigToTargets` on the live list; `discoverTargets` only runs in `reload()` (eng #3). |
| 8 | UpdateChecker untested | **Held, partially** | `test_updatechecker.cpp` covers `decodeUpdateReply` (10 cases). The `handleReply` shell (redirect loop, stale-reply guard, hop cap) still untested (eng #4). |
| 9 | unshorten untested | **Held, partially** | `test_unshorten.cpp` covers the pre-network gates; the live HEAD-request path remains untested without a mock endpoint (eng #5). |
| 10 | OpenSpec stale, never archived | **Done** | `initial-tern` archived as `2026-09-14-initial-tern`; canonical spec exists at `openspec/specs/lane/spec.md`. |
| 11 | `holdAutoOpen` default true | **Code fixed, docs regressed** | `types.h:149` and `config.cpp:261` default false; schema, tests, changelog, metainfo agree. DESIGN.md, README, AGENTS.md still describe default-on (ceo #5, design #5, devex #1/#4). |
| 12 | `remembered` map no GC | **Fixed** | `reload()` calls `clearDeadRemembered()` at Controller.cpp:668 after `setRules()` so persist cannot clobber (eng #6). |
| 13 | `isDefaultBrowser()` spawns xdg-settings per read | **Held** | `m_isDefaultBrowser` cache; refresh only from `reload()`, `makeDefaultBrowser()`, `openSettings()` (eng #7). |
| 14 | Config silently ignores unknown keys | **Fixed** | `knownKeys` set logs `qWarning` per unrecognized key (config.cpp:231-248). Comment at 225-227 still says "silently ignored"; stale, harmless (eng #8). |
| 15 | `urlInScope` prefix match not segment-aware | **Fixed** | Normalizes trailing slashes, requires exact or `scope + '/'` boundary; tested (eng #9). |
| 16 | qputenv/qunsetenv around startDetached | **Addressed** | launcher.cpp:238-253 documents why the env mutation is safe and why `setProcessEnvironment` cannot be used with the detached overload (eng #10). |
| 17 | DESIGN.md drift (6 rows + blur vs 8 rows + tint) | **Partially fixed** | Row count and tint now match code. Hold default still wrong (design #5). |
| 18 | Stale screenshots show Tern branding | **Never fixed** | `docs/screenshots/*.png` last modified 2026-09-10; README still embeds them (design #6). |
| 19 | Ladder keys invisible in footer | **Fixed** | Footer shows `. widen` / `, narrow` (design #2). |
| 20 | Always checkbox omits target name | **Fixed** | Appends `in <name>` when a row is highlighted (design #3). |
| 21 | Settings sidebar lacks Accessible names | **Fixed** | `Accessible.role`/`name`/`selected` on nav delegates (design #4). |
| 22 | CONTRIBUTING lacks dep install | **Fixed** | Points at the README `pacman` line (devex #4). |
| 23 | `appstreamtest` false green without install | **Never fixed** | README documents the skip; RELEASING.md and PKGBUILD `check()` run `ctest` without install; only CI installs first (devex #5). |

## Contradictions adjudicated

**1. The eng P3s are real bugs; are they ship-blockers?**

The eng review found a wrong-destination launch race
(`requestActivationAndLaunch` captures `target` but `launch()` reads
`m_click.openUrl` at fire time, so a second click inside the ~300ms
token-request window pairs the old target with the new URL), a stale
picker that silently retargets after a queued click, and a
`flatpak run --command=` path around the interpreter blocklist. The
verdict is still SHIP. Rank them honestly: the race is the worst of
them because a wrong-destination open is the one thing a link router
must not do, but it needs two clicks inside a sub-second window and the
fix is a captured URL or generation counter in `finish`. The flatpak
hole needs write access to the config file, which is a local-user
threat model; it is the same shape as the `env bash -c` bypass round 1
closed, one argv level down, and worth closing for symmetry with
`flatpak-spawn` already being blocked. Fix-worthy, scheduled for 0.2.1,
not a reason to hold the AUR publish. A package nobody can install
fixes nothing.

**2. Picker filter empty state: 0.2.1 or 0.3.0?**

The design review's top finding is that a filter with zero matches
shows a dead 48px row and Enter does nothing, while its verdict is
"ship for daily use." Resolve it as a 0.2.1 fix. The change is a "No
matching destinations" label plus dropping the fake row height, it is
user-facing every time a filter misses, and 0.2.1 already exists as a
vehicle (issue #12 argv fix plus the doc-drift sweep). There is no
reason to hold a one-label fix for a feature release.

**3. Does flatpak support violate "no new features until a stranger installs"?**

No. The CEO review already adjudicated this: the user it serves is real
(Andy's second machine), the change is discovery-layer only, and it
removes an install-time failure for exactly the audience an AUR package
attracts. The no-features rule is about building for a hypothetical
audience (the Windows port, KActivities routing), not about making the
product work on the maintainer's own hardware. The rule stands; flatpak
discovery does not break it.

## Fix list

Ordered by user impact, not by which reviewer raised it loudest.

### P0: none

No crash-class or data-loss findings this round. The two round-1 P0s
held.

### P1: a stranger hits these on day one

| # | Finding | Source | Fix |
|---|---|---|---|
| 1 | `yay -S lane` does not work. PKGBUILD is done and validated; the only missing step is Andy's AUR account, SSH key, and `git push` to `aur.archlinux.org/lane.git`. Then the r/kde + KDE Discourse post leading with path-scoped memory and the hold HUD, pointing at Junction issue #9. | ceo #1, devex (PKGBUILD), distribution check | Andy: ten minutes of account setup, then push. No agent can do this. |
| 2 | Hold-default doc drift. `DESIGN.md:44-46` says `holdAutoOpen` on by default; `README.md:47-49`, `92-94`, `101-102` describe the hold bar as standard behavior; `AGENTS.md:52` shows `"holdAutoOpen": true` in the sample config. Code, schema, tests, changelog, and metainfo all say opt-in. New readers expect a HUD they will not see. | design #3, devex #1/#4, ceo #5 | Update the three files to describe hold as opt-in; add "docs match behavior" to the RELEASING.md checklist. |
| 3 | README screenshots predate the rename. `docs/screenshots/*.png` show Tern branding, a six-row picker without section headers, and a flat Targets list. First-time users judge the product from these images. | design #4 | Re-capture picker, hold, and settings against v0.2.0. |
| 4 | Issue #12: native-browser argv silently changes when the desktop Exec carries extra non-field-code flags. Auto-detected, but exactly the bug class a real user hits and cannot diagnose. | ceo #5, issue #12 | Fix and include in 0.2.1. |
| 5 | Flatpak discovery and the system-install path are invisible in docs. Flatpak profile discovery shipped in Unreleased with no mention in README, CONTRIBUTING, or AGENTS; the README also never mentions the PKGBUILD/`yay -S lane` route that is about to become the primary install path. | devex #3, distribution check | Add a flatpak note to README/AGENTS and an AUR install line to README once the package is published. |

### P2: daily-use friction and silent wrongness

| # | Finding | Source | Fix |
|---|---|---|---|
| 6 | Picker filter with zero matches shows a dead 48px row; Enter does nothing. | design #1 | "No matching destinations" label when the filtered model is empty; drop the fake row height. 0.2.1. |
| 7 | `Exec=env FOO=1 firefox %u` desktop files are discovered with `exec=env`, which the blocklist then rejects at launch. The target shows in settings and the picker and silently fails every time. | eng #3 (flip side) | Unwrap leading `env VAR=...` assignments at discovery, or drop the entry. |
| 8 | `dragId` not cleared on a cancelled drop (TargetsPage.qml:79-89 and three siblings). Next drag moves the grabbed row visually but persists a move for the previous row's id; list and `targetOrder` diverge. | eng #4 | Clear `dragId` unconditionally in `onDropped`. One line. |
| 9 | `appstreamtest` is a false green outside CI. README, RELEASING.md, and PKGBUILD `check()` all run `ctest` without `cmake --install`; the test logs "Not installed yet, skipping" and passes. Carried from round 1. | devex #2 | Add the install step CI already runs to RELEASING.md and the README happy path; PKGBUILD `check()` can install to a throwaway prefix like CI does. |
| 10 | Picker section-header height over-counts: card is sized for every section in the filtered model, not the headers visible in the eight-row viewport. Round-1 carry, unchanged. | design #2 | Count only headers inside the viewport. |
| 11 | `holdMs` clamp triple mismatch: schema min 400 no max, UI 400-5000, controller setter 200-10000. Hand-edited values outside the UI range load but cannot be set from the UI. | devex #5 | Pick one range and apply it in all three places. |

### P3: real but narrow

| # | Finding | Source | Fix |
|---|---|---|---|
| 12 | Wrong-destination launch race: `requestActivationAndLaunch` captures `target` in `finish` but `launch()` reads `m_click.openUrl` at fire time (Controller.cpp:744,748,768-794). A second click inside the token-request window pairs the old target with the new URL. Same shape via `confirmHold` (939-948). Worst-when-hit of the P3s; narrow window. | eng #1 | Capture the click's URL (or a generation counter) in `finish` and bail if `m_click` moved on. |
| 13 | Queued click deciding Launch/Hold leaves the picker up showing the previous click's rows; a later `pick()` retargets to the new `m_click` (Controller.cpp:694-717). | eng #2 | `hidePicker()` in `openUrl` before `applyDecision`, or in `launch()`/`startHold()`. |
| 14 | `flatpak run --command=sh app.id` bypasses the interpreter blocklist: `flatpak` is not in `blockedInterpreters()` (it must not be) and nothing inspects args. Same shape as the round-1 `env bash -c` bypass, one argv level down. Needs config write access; defense-in-depth. | eng #3 | Scan args for `--command`/`--talk-name` escapes, or reject `flatpak run` custom targets without a dotted app id. |
| 15 | Dead config fields `recentTargetIds` and `toastMs`: written and loaded, never read. `recentTargetIds` also means every link open writes the config file. | eng #8 | Wire them up or drop them from config and schema. |
| 16 | Changelog drift: 0.2.0 says remembered entries are never removed automatically (`reload()` now prunes dead-target ones); 0.1.0 advertises title/process rules that can never fire. | eng #11 | Correct both entries. |
| 17 | `handleArgs` strips argv[0] by `contains("lane")`; a renamed binary treats argv[0] as a URL. Dead code at line 335. | eng #5 | Strip argv[0] unconditionally for D-Bus Activate; remove the dead check. |
| 18 | Update release URL and redirects not host-pinned to GitHub; any 403 reported as rate limiting. | eng #6 | Pin to `github.com`; narrow the 403 message. |
| 19 | Dead check in `chromiumProfiles::addProfile` (discovery.cpp:724-727): empty `if` body, missing `continue`; stale `info_cache` entries become dead targets. | eng #7 | Restore the `continue` or delete the check. |
| 20 | `execPrefix` splits args on spaces without honoring quotes (discovery.cpp:74-102); `--command="zen browser"` corrupts the rebuilt argv. Rare in real exports. | eng #9 | Honor quotes in the arg split, matching `firstToken`'s rules. |
| 21 | `m_pendingUrls` unbounded: a burst of opens during a nested unshorten loop serializes into stale launches minutes later. | eng #10 | Cap or coalesce the queue. |
| 22 | Design follow-ups: Targets search silent on zero hits; per-row Default buttons duplicate the global combo; section collapse chevrons, ladder chevrons, and picker section headers lack Accessible names; inline rename fields look like static labels; blocked URL is notification-only. | design #6-12 | Batch with the next settings/picker pass. |
| 23 | Devex lows: CI builds Debug while README/RELEASING use Release; `lane --rediscover` constructs the full daemon rather than a one-shot CLI; no issue/PR templates. | devex #6-8 | Align build types in docs; document `--rediscover` honestly; templates can wait for the first outside PR. |

## Deliberately not doing

Places the suite considered and rejected:

- **Starting the Windows port.** `docs/designs/windows-port-plan.md` is
  thorough and its own assessment recommends against porting. It trades
  Lane's only wedge (Plasma-native, the thing no competitor can copy)
  for a parity fight against Browser Tamer, which is actively
  maintained and free. Keep the doc, do not start P0. Revisit only if a
  paying Windows user appears (`docs/reviews/ceo.md:27-29`).
- **Building KActivities-scoped routing yet.** The competitive analysis
  is right that it is the best next feature: Plasma-only, uncopyable,
  reuses the existing scope model. It is still a feature for an
  audience of zero. It is the first thing to build if a stranger
  responds to the r/kde post, not before (`docs/reviews/ceo.md:34-37`).
- **Gating the AUR publish or 0.2.1 on the eng P3s.** They are real and
  scheduled, but the trigger conditions are narrow (sub-second race,
  hand-edited config) and a package nobody can install fixes nothing.
- **Implementing `SourceInfo` title/process matching.** The docs are
  now honest, the UI no longer offers the dead locations, and the
  matcher warns. Real implementation needs compositor-specific APIs for
  a feature nobody has asked for. Keep the stub and the warnings.
- **Ripping out containers.** Carried from round 1: small, tested,
  honestly documented. The call is to stop adding features for a
  near-empty audience, not to remove what works.
- **Shipping a container-bridge WebExtension.** Carried: AMO review
  pipeline, a second release train, and a JavaScript support surface.
  Wait for outside demand.
- **Chasing distro packaging beyond AUR.** Carried: PolyForm
  Noncommercial blocks the official Debian/Fedora/openSUSE repos. AUR
  first; Flathub can wait for signal.
- **Relicensing.** Carried: one-way door, no signal either way.
- **Headless QML smoke test in CI.** Carried: revisit if a second
  Kirigami API-drift bug ships.
- **Lua scripting.** Carried: still out of scope.

## What the repo already decided

Settled calls a future session must not silently reverse. Carried from
round 1 unless marked new.

- **PolyForm Noncommercial + separate commercial track.** Free for
  personal/hobby use; commercial use requires contacting Andy. Pricing
  unset, email-only. Relicensing is a one-way door; hold for signal.
- **The repo is `bitskc/lane`; the Tern name is retired.** (New.)
  Remote, releases, metainfo, PKGBUILD, tests, and docs all agree. Old
  `bitskc/tern` URLs redirect.
- **`holdAutoOpen` defaults to false.** (New.) Code, schema, tests,
  changelog, and metainfo all agree; the hold HUD is opt-in via
  Preferences. Rules still skip hold. The docs catching up is a fix-list
  item, not an open question.
- **Flatpak browser support is in scope.** (New.) Discovery-layer only,
  shipped in Unreleased, justified by a real user (Andy's second
  machine) and by the AUR audience it removes an install-time failure
  for.
- **The Windows port is deferred by user decision.** (New.) The plan
  doc stays; P0 does not start without a paying Windows user.
- **OpenSpec has a canonical spec.** (New.) `initial-tern` is archived
  and `openspec/specs/lane/spec.md` exists. Future changes delta
  against it.
- **Path-scoped memory, not just host.** `github.com/bitskc` can go
  somewhere different from `github.com`. The destination ladder and
  comma/period navigation expose this. Verified unique across six
  competitors by the competitive analysis; lead with it in the r/kde
  post.
- **Hold HUD on silent opens only, when enabled.** `shouldHold()` runs
  the hold on remembered, PWA, and default opens; rule matches skip it.
  The picker is the veto for ambiguous paths.
- **Absolute Exec paths in the .desktop file.** Required for the KDE
  app menu and D-Bus activation.
- **No default-browser theft.** The only path to becoming default is
  the button in Settings.
- **http/https only.** `file:`, `javascript:`, `data:`, and
  credentials-in-URL are refused; mailto and PDF keep their own
  handlers.
- **Custom handlers are argv, never a shell.** Interpreters are
  rejected by basename and symlink-chain walk. The flatpak `--command=`
  gap is a fix-list item, not a change to this rule.
- **Resident daemon, not cold-start.** The architecture depends on
  staying resident.
- **No Lua.** Explicitly out of scope.
- **Containers shipped but Lane never registers `ext+container`
  itself.** The URL is wrapped as argv and handed to the browser;
  landing in the container depends on a third-party protocol-handler
  extension. README is now honest about the dependency.
