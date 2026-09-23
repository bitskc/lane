# Lane: full gstack analysis, round 3

Synthesis of four independent reviews run 2026-09-18 against Lane main
@ `1de0d59`, 53 commits past v0.2.0 (`docs/reviews/eng.md`,
`docs/reviews/ceo.md`, `docs/reviews/design.md`,
`docs/reviews/devex.md`). This document does not re-review the code; it
reconciles what the four reviewers found, resolves the places they pull
in different directions, and orders everything into one fix list. The
round-2 version of this file is in git history.

## Executive summary

The 53-commit fix wave since v0.2.0 is real hardening, not scope creep:
flatpak discovery, a click snapshot that kills the wrong-URL launch
race, a position-independent `flatpak run` argv gate, and config
durability (quarantine, atomic write, failure notification). Every
round-2 fix re-verified in source and held. But the wave introduced one
self-inflicted P1: the fixed picker section order moved the ranked
leader off row 0, so Enter and digit-1 can open the wrong target
(issue #33). That bug sits on the product's core interaction and must
be fixed or the section order reverted before v0.3.0 is tagged. The
strategic picture is unchanged for the third round running: the wedge
is real (Junction declined profile routing at issue #9), the product
still has no door (`yay -S lane` returns nothing), and the only move
that matters is Andy's ten minutes on AUR, then the r/kde post, then
stop building.

## Verdicts

| Review | Verdict | Reason |
|---|---|---|
| Eng (`docs/reviews/eng.md`) | **SHIP** | Every round-2 fix held in source; wave-3 hardening is real and tested (12/12 ctest). Remaining: a silent flatpak discovery gap when the daemon lacks `XDG_DATA_DIRS` (observed in production on beelink), an X11 picker whose `closeOnFocusLoss` never observes focus loss, a flatpak Edge fingerprint miss, and doc/comment drift. |
| CEO (`docs/reviews/ceo.md`) | **THE WEDGE IS REAL. THE PRODUCT STILL HAS NO DOOR.** | Third consecutive review saying publish to AUR and post to r/kde. Code improves every round; distribution has not moved. v0.3.0 cannot tag until #33 is fixed. |
| Design (`docs/reviews/design.md`) | **Daily-driver ready; refresh screenshots before sharing; ranked suggestion buried at 40+ targets** | Net -4 on picker density and marketing assets, +1 accessibility. Independently arrived at the #33 row-0 problem from the UX side. |
| Devex (`docs/reviews/devex.md`) | **Cold Arch clone builds and tests cleanly; docs still drift; no merge-conflict guard** | TTHW ~5-10 min cold, under a minute warm. README tutorial still teaches hold-bar as default-on, zero non-Arch build guidance, and conflict-marker junk has landed in CHANGELOG three times with no automated guard. |

## Round-3 fix verification

The director-verified table, adjudicated against the round-2 fix list
and the reviews' own verification rows. All eleven checks held; none
required re-derivation.

| # | Check | Status | Notes |
|---|---|---|---|
| 1 | `flatpakRunArgsBlocked` scans dangerous opts position-independently | **Held** | launcher.cpp:107-123 scans every token before the subcommand check; first non-option token must be `run` plus a dotted app id. Tested (test_launcher.cpp:289-352). Closes round-2 fix-list #14. |
| 2 | Symlink-to-flatpak canonical basename checked | **Held** | launcher.cpp:315-318 checks both the link name and the canonical basename before the argv gate. Tested. |
| 3 | Click snapshot: launch paths carry Click by value | **Held** | `finish` captures `click` by value (Controller.cpp:803-810); `launch()` never reads `m_click`. `stop()` on the hold animation does not emit `finished()` on Qt6, verified empirically, so a new click cancels a hold cleanly. Closes round-2 #12 and #13, the two worst P3s. |
| 4 | `execPrefix` env-unwrap handles `-i`/`-u`/`-C`/`-S`/`--`; residual `-a`/`--argv0` fails closed | **Held** | discovery.cpp:113-159. `env -a name firefox` still unwraps to program=`name`, producing a dead or wrong target, never a shell. Fail-closed; P4 residue, not a hole. Closes round-2 #7. |
| 5 | Dead config keys `recentTargetIds`/`toastMs` gone | **Held** | Zero occurrences in src, tests, schema, AGENTS.md. Closes round-2 #15. |
| 6 | No `org.kde.layershell` QML import; CMake links the lib only | **Held** | `configureLayerShell` null-checks and falls back to a normal window. |
| 7 | README deps match `find_package` (`kcrash`, `kcolorscheme`) | **Held** | README.md:186, CMakeLists.txt:51-54, ci.yml:24-27 all agree. |
| 8 | Schema matches `config.cpp` known keys; `holdMs` 400-5000 everywhere | **Held** | 22 keys, zero drift. Closes round-2 #11. |
| 9 | AGENTS.md sample matches schema and code | **Held** | `hiddenTargetIds` present, `holdAutoOpen: false`, `--list` kind column documented. |
| 10 | PKGBUILD: `appstream` makedep, `check()` installs to throwaway prefix, `layer-shell-qt` dep | **Held** | Closes the PKGBUILD half of round-2 #9 (appstreamtest false green). |
| 11 | CI Release build, install before ctest, CHANGELOG clean | **Held** | ci.yml:29-44; no conflict markers in CHANGELOG.md. |

Round-2 items still open after this wave: stale screenshots (design,
carried twice now), the README/RELEASING half of the appstreamtest
false green (CONTRIBUTING still runs ctest without install), and the
schema/changelog rows that still advertise dead `title`/`process` rule
locations.

## Cross-review consensus

**Issue #33 is the strongest signal this suite has produced.** CEO and
Design independently arrived at the same defect from opposite ends:
CEO from the release-readiness side (row 0 is load-bearing for
pre-selection, Enter, and shortcut 1), Design from the UX side (fixed
section bucketing buries the ranked suggestion below the eight-row
viewport at 40+ targets). The mechanism is confirmed in source:
`PickerModel::applyFilter` (src/app/PickerModel.cpp:135-153) regroups
`rankForPicker`'s ordering into fixed `kSectionOrder` buckets, so row 0
is the first Web-apps row, not the top-ranked target. A remembered or
suggested destination can sit at row 15+ while Enter opens whatever
the bucketing put first. Two reviewers, two methods, one bug. Fix or
revert before tagging 0.3.0.

**Doc drift is a theme, not a list.** Eng and DevEx each found pieces;
grouped, they are one finding: the repo's prose trails its behavior
after every fast fix wave. The instances: launcher.cpp:100 comment
overclaims what the dangerous-option scan blocks; config.cpp:225 still
says unrecognized keys are "silently ignored" (they warn);
unshorten.cpp:27 hardcodes UA `Lane/0.1`; the schema still lists dead
`title`/`process` rule locations; CHANGELOG 0.1.0 still advertises
matching "by URL, window title, or source process"; the README
tutorial teaches the hold bar as default-on while `holdAutoOpen`
defaults false; CONTRIBUTING runs `ctest` without install so
`appstreamtest` false-greens; no openSUSE build docs exist. Round 2
flagged the same pattern on `holdAutoOpen`. The fix is the same
recommendation, now with more evidence: a "docs match behavior" step
in RELEASING.md, plus a cheap CI grep for conflict markers (below).

**Merge-conflict junk is a recurring bug class with zero guard.**
DevEx found two cleanup commits for CHANGELOG contamination
(`bef9c23`, `3fde33f`) and prior rounds cite a third. No hook, no CI
step greps for `<<<<`, `>>>>`, `=======`, or `conflict://`. One CI
grep line ends the class permanently.

**The distribution blocker is unchanged and still human.** Third round
running: PKGBUILD validated, repo renamed and tagged, AUR RPC for
`lane` returns zero results. Every other finding is secondary to the
fact that no stranger can install the product.

## Contradictions adjudicated

**1. Pin the ranked leader, or keep pure section order?**

Design's finding implies the router's top pick should be reachable at
row 0 or via a `suggested` badge; the shipped section order exists so
containers are not buried below every browser profile (CHANGELOG,
Unreleased). These are not in conflict once you separate the leader
from the rest. Resolve: pin `rankForPicker`'s top target to row 0
regardless of section (or render the bound-but-unused `suggested` role
as a distinct first row), then keep `kSectionOrder` for the remaining
rows. That preserves the anti-burying intent of the section order and
restores the invariant that Enter/digit-1 opens the best target. A
full revert to pure ranked order re-buries containers, which is what
the section order was shipped to fix.

**2. Is the flatpak `XDG_DATA_DIRS` gap a P2 or a doc note?**

Eng ranks it P2 because it already bit in production on beelink: a
daemon started without the variable silently loses every flatpak
target, no log line, no UI signal, and the settings-open rescan
re-reads the same dead environment. Agreed, P2. The fix is two cheap
moves, both worth doing: append `/var/lib/flatpak/exports/share` and
`~/.local/share/flatpak/exports/share` unconditionally (stable paths,
not env-derived), and `qWarning` when a flatpak executable exists but
zero flatpak desktop entries were scanned. A doc note alone leaves the
silent-failure shape intact.

**3. X11 focus loss: wire `onActiveChanged` or accept the platform limit?**

Eng's finding is a code-structure inference, not a live-X11 repro, but
the structure is unambiguous: `closeOnFocusLoss` is wired only to the
backdrop MouseArea (Picker.qml:63) and nothing connects
`onActiveChanged` to `cancelPicker`. On the layer-shell null path the
picker is a normal always-on-top window; Alt-Tab leaves it covering
the screen, and if `requestActivate()` is refused the shortcuts and
filter are dead. Design flagged the same X11 fallback from the UX side
(focus can leak to windows behind the card). Resolve: bind
`onActiveChanged` on the non-layer-shell path. It is a small fix on a
supported fallback, not a platform limitation to document away.

**4. Does the dangerous-option scan need widening?**

Eng finding 4: the comment overclaims; `--filesystem=host`,
`--socket=session-bus`, `--device=all`, and `--talk-name` pass
unchecked because only `--command` with an interpreter-basename value
is gated. Adjudicated as defense-in-depth, P4: reaching it needs write
access to config.json, and a discovered desktop file can already name
any binary. Fix the comment to match what the gate does (the doc-drift
item), and optionally block the widening options for `Kind::Custom`
targets. Do not hold the release for it.

**5. Zombie remoting: fix or document?**

A crashed flatpak browser whose xdg-dbus-proxy still owns the remoting
name reports launch success and shows the browser's own
not-responding dialog. There is no cheap correct fix from Lane's side
(the dialog lives in the browser; detection means watching a window
Lane did not spawn). Resolve as a known-limitations note in AGENTS.md
so the next reviewer does not re-investigate. Do not build detection.

## Fix list

Ordered by user impact, not reviewer loudness.

### P1: blocks the release or the door

| # | Finding | Source | Fix |
|---|---|---|---|
| 1 | **#33: ranked leader buried by section bucketing.** Row 0 is the first Web-apps row, not `rankForPicker`'s top pick; Enter/digit-1 can open the wrong target and the remembered destination can sit below the viewport. Self-inflicted by the picker reorder in this wave. | issue #33; ceo #2; design #2; PickerModel.cpp:135-153 | Pin the ranked leader to row 0 (or a `suggested` first row), keep `kSectionOrder` for the rest. Gate for tagging 0.3.0. |
| 2 | **`yay -S lane` still does not work.** PKGBUILD validated three rounds running; the missing step is Andy's AUR account, SSH key, and push to `aur.archlinux.org/lane.git`. Then the r/kde + KDE Discourse post leading with path-scoped memory and the hold HUD, citing Junction #9. | ceo #1 (third round running) | Andy: ten minutes. No agent can do this. |
| 3 | **v0.3.0 not staged.** `CMakeLists.txt:2` and `packaging/PKGBUILD:2` still say 0.2.0; 53 commits sit in Unreleased. Cannot tag until #33 lands; triage #35 (flatpak gate unenforced dangerousOpts) and #36 (env unwrap ghost targets) for the same release. | ceo #2 | Fix #33, bump both files, tag, release, publish, post. In that order. |

### P2: silent wrongness a real user hits

| # | Finding | Source | Fix |
|---|---|---|---|
| 4 | **Daemon without `XDG_DATA_DIRS` loses every flatpak target silently.** Observed in production on beelink; no log, no UI signal, rescan does not help. | eng #1; discovery.cpp:989-997 | Append the two well-known flatpak export dirs unconditionally; `qWarning` when flatpak exists but zero exports scanned. |
| 5 | **README screenshots still show Tern branding** and a six-row picker with no section headers. Carried from rounds 1 and 2; first-time readers judge the product from these. | design #1 | Re-capture picker, hold, settings against current main. Do it before the r/kde post, not after. |
| 6 | **README tutorial teaches hold-bar as default-on.** "When Lane opens a link... it shows a hold bar" and step 5 "Turn the hold off" contradict `holdAutoOpen: false`. | devex #1 | Rewrite the tutorial to start from enabling "Pause before opening" in Preferences. |
| 7 | **X11 picker never observes focus loss.** `closeOnFocusLoss` wired only to backdrop click; Alt-Tab leaves a screen-covering window; refused focus kills shortcuts and filter. Same shape in Hold.qml. | eng #2; design #10 | Bind `onActiveChanged` to `cancelPicker` on the non-layer-shell path. |
| 8 | **Empty picker is a dead end.** "No matching destinations" covers both filter-miss and zero-configured-targets; no path to Settings or Rediscover on first run. | design #4 | Distinguish the two states; link the empty state to Settings/Rediscover. |

### P3: real, narrow, cheap

| # | Finding | Source | Fix |
|---|---|---|---|
| 9 | Flatpak Edge fingerprints as Generic: blob check wants `microsoft-edge`/`msedge` but "Microsoft Edge" lowercases to `microsoft edge`. No profiles, no incognito row, no `~/.var/app` dir. Any flatpak browser whose Name lacks the hyphenated brand hits the same wall. | eng #3; discovery.cpp:454 | Match the flatpak app id when `isFlatpak`, or add `microsoft edge` plus word-boundary `edge`. |
| 10 | `suggested` role bound but never rendered; Alt+A not in footer. The smart default has no visual cue beyond whatever bucketing put first; the only keyboard path to Always is unlabeled. | design #3, #5 | Render `suggested` (badge/weight) once #33 pins it to row 0; add Alt+A to the footer. |
| 11 | Doc-drift sweep (one theme): launcher.cpp:100 comment overclaims; config.cpp:225 "silently ignored" stale; unshorten.cpp:27 UA `Lane/0.1`; schema lists dead `title`/`process` locations; CHANGELOG 0.1.0 overclaims; CONTRIBUTING ctest-without-install false-greens appstreamtest; no openSUSE build docs. | eng #10; devex #1, #2, #4, #5 | One pass over comments, schema, CONTRIBUTING, README; add "docs match behavior" to RELEASING.md. |
| 12 | No guard against merge-conflict junk in committed files. Three CHANGELOG contaminations across rounds; no hook, no CI grep. | devex #3 | One CI step grepping `<<<<`, `>>>>`, `=======`, `conflict://`. Ends the class. |
| 13 | `env -a`/`--argv0` unwraps to the wrong program (dead or wrong target, never a shell). Fail-closed, P4. | eng #6 | Add `-a`/`--argv0` to `envOptWithValue`. |
| 14 | Activation-token fallback discards the click's own token: `finish(QString())` instead of `finish(m_pendingActivationToken)` when a Lane window exists. Rare, low-cost. | eng #8 | Pass the pending token through. |
| 15 | Stale firefoxpwa config produces dead PWA rows when the binary is absent. | eng #9 | Skip PWA discovery when `firefoxpwa` does not resolve. |
| 16 | `isPrivateOrLocalHost` misses integer/hex IPv4 literals (`http://2130706433/`, `http://0x7f000001/`). Bounded impact; intent defeated. | eng #7 | Parse bare-integer/`0x` hosts before the QHostAddress attempt. |
| 17 | Settings section order disagrees with picker section order; per-row Default buttons duplicate the global fallback combo; regret path for Always is still a settings dig; hold progress bar not exposed to AT. | design #6-9 | Batch with the next settings/picker pass. |

## What the repo already decided

Settled calls a future session must not silently reverse. Carried from
round 2 unless marked new.

- **PolyForm Noncommercial + separate commercial track.** Holds up;
  does not block AUR or Flathub; pricing-unset is honest at zero
  users. Relicensing is a one-way door.
- **The repo is `bitskc/lane`; Tern is retired.**
- **`holdAutoOpen` defaults to false.** Code, schema, tests, changelog,
  metainfo, and now DESIGN.md/AGENTS.md all agree. The README tutorial
  is the last stale surface.
- **Flatpak browser support is in scope.** Discovery-layer only;
  justified by a real user and by the AUR audience.
- **The Windows port is deferred by user decision.** Doc stays; no
  code without a paying Windows user.
- **OpenSpec has a canonical spec** at `openspec/specs/lane/spec.md`.
- **Path-scoped memory, not just host.** Verified unique across six
  competitors; lead with it in the r/kde post.
- **Hold HUD on silent opens only, when enabled.** Rules skip hold.
- **Absolute Exec paths in the .desktop file.**
- **No default-browser theft.** Opt-in via the Settings button only.
- **http/https only.** file:, javascript:, data:, and
  credentials-in-URL refused.
- **Custom handlers are argv, never a shell.** Interpreter blocklist
  plus symlink-chain walk; the flatpak argv gate now scans
  position-independently.
- **Resident daemon, not cold-start.**
- **No Lua.**
- **Containers shipped; Lane never registers `ext+container` itself.**
- **X11 is a supported fallback, not a target.** (New.) Layer-shell is
  Wayland-only by design; the normal-window path must still behave
  (focus-loss cancel is a fix-list item, not a scope change).
- **Picker section order exists to keep containers visible.** (New.)
  The fix for #33 pins the ranked leader above the sections; it does
  not revert the section order.

## Deliberately not doing

- **Zombie-remoting detection.** No cheap correct fix exists from
  Lane's side; the dialog lives in the browser. Document as a known
  limitation in AGENTS.md and stop re-investigating it (eng #5).
- **Browser extension bridge.** Second product surface, two store
  review pipelines, zero users. Revisit on outside demand.
- **Scripting (Lua/JS).** Explicit non-goal; Finicky proves the
  maintenance cost.
- **Source-app rules.** Impossible on Wayland; no portal hands Lane
  the caller's identity. Schema/docs honesty is the fix, not the
  feature.
- **GNOME/GTK version.** Junction exists and declined this feature
  set. Chasing GNOME costs the wedge.
- **Setting a commercial price now.** No inbound, no signal.
- **Relicensing to GPL/MIT.** One-way door, no reason at zero users.
- **KActivities routing, tracking-param list, reclaim watchdog.** All
  verified-good competitive-analysis recommendations; all wait for a
  stranger to install Lane first.
- **Distro packaging beyond AUR.** PolyForm NC blocks official repos;
  deb/rpm/Flathub wait for non-Arch signal from the announcement.
- **Widening the flatpak dangerous-option gate beyond `Kind::Custom`.**
  Defense-in-depth only; fix the comment, optionally gate custom
  targets, do not hold the release.
- **New features of any kind until a stranger responds.** Third round
  running. Fix #33, tag 0.3.0, publish AUR, post, stop.

## Next 30 days

1. Fix or revert #33 (pin ranked leader, keep section order). Triage
   #35 and #36 for the same release.
2. Bump to 0.3.0 in CMakeLists.txt and PKGBUILD, tag, release.
3. Andy: create the AUR account, publish `lane`. Third time asking.
4. Re-capture the three README screenshots before posting anywhere.
5. Post to r/kde and KDE Discourse with the AUR install line, leading
   with path-scoped memory and the hold HUD, citing Junction #9.
6. Then stop. No new features until a stranger says something.
