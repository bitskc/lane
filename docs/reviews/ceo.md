# CEO Review: Lane (round 2)

Reviewed 2026-09-17, five days after v0.2.0 shipped. Repo state: remote is now `bitskc/lane` (verified: `git remote -v`, GitHub releases API returns 200), tags v0.1.0 and v0.2.0 exist, PKGBUILD carries a real sha256. Still 0 stars, 0 forks, 0 watchers. One open issue (#12, an auto-detected argv bug). AUR query for `lane` returns zero results. No stranger has installed this yet.

## Verdict

**THE ONLY MOVE LEFT IS ANDY'S TEN MINUTES ON AUR.**

Everything the last review asked engineering to do got done: repo renamed, 0.2.0 tagged and released, update checker live, PKGBUILD checksummed. The remaining blocker is not code. It is an AUR account and an SSH key, which only Andy can create. Until that happens, Lane is a product with no distribution channel and no users, and every hour spent on features, ports, or docs is spent on an audience of one. Publish the package, post it to r/kde pointing at Junction issue #9, then stop building until a stranger says something.

## Round-1 fix verification

| # | Round-1 finding | Claimed status | Verified status |
|---|---|---|---|
| 1 | Rename repo to bitskc/lane | Done | **Held.** Remote is `bitskc/lane`, releases API returns 200, PKGBUILD `source=` resolves and `sha256sums` is a real hash (`packaging/PKGBUILD:16-17`). |
| 2 | Tag 0.2.0, publish AUR, post r/kde | Partially | **Held, half done.** v0.2.0 tagged and released. AUR has no `lane` package (RPC query returns 0 results, 2026-09-17). r/kde post not made. The two items that produce users are the two that did not happen. |
| 3 | Containers = scope creep; README should be honest about bridge dependency | Unknown | **Mostly held.** README:36-38 now says containers appear "wherever `containers.json` exists and the browser has a protocol-handler extension installed to act on them." That is honest about the dependency without naming the 1.9K-user figure. Acceptable. |
| 4 | Stop adding features until real users | Partially | **Held with a caveat.** Flatpak discovery shipped (justified: Andy's beelink is a real user). The Windows port plan is docs only, no code, and the port assessment itself recommends against porting. No feature code landed for a hypothetical audience. |
| 5 | holdAutoOpen default contradicts DESIGN.md | Fixed | **Code fixed, docs regressed.** `types.h:149` and `config.cpp:261` default to false; schema and tests agree. But `DESIGN.md:44-46` still says "on (default)", `AGENTS.md:52` shows `"holdAutoOpen": true` in the example config, and `README.md:47-49` describes the hold bar as standard behavior with no opt-in note. The fix landed in code and drifted in docs. |

## Findings, ranked by impact

### 1. AUR publish is blocked on a ten-minute account creation, and it is the entire distribution strategy

The PKGBUILD is done: real checksum, license install, offscreen tests. The only step left is `ssh aur@aur.archlinux.org` account setup and `git push` to `aur.archlinux.org/lane.git`. No agent can do this; it needs Andy's AUR account and SSH key. This has been the blocker since before 0.2.0 shipped and it is still the blocker. Every other finding in this document is secondary to the fact that `yay -S lane` does not work yet. This is now the second consecutive review where the top recommendation is "publish the package," and the work remaining is smaller than it was last time.

### 2. The Windows port plan is a well-written answer to a question nobody asked

`docs/designs/windows-port-plan.md` is thorough: a real `Platform` interface, phased P0-P3, honest risk table. It is also 10-14 person-weeks of work aimed at a market where Browser Tamer is actively maintained (commits today, 2026-09-17; release 6.2.3 on Sep 1, verified via GitHub API) and free. The port assessment's own recommendation is "do not port." The plan existing as a deferred option is fine and cheap. What would not be fine is treating it as the next thing to build: it trades Lane's only wedge (Plasma-native, the thing no competitor can copy) for a parity fight against a maintained incumbent, before a single Linux stranger has installed the product. Keep the doc. Do not start P0. Revisit only if a paying Windows user appears, which is the scenario COMMERCIAL.md exists for.

### 3. Documentation drift is becoming a pattern, not an accident

Round 1 flagged the openspec "out of scope" line silently reversed by shipped code. Round 2 finds the same shape again: the holdAutoOpen default flipped in code, changelog, schema, and tests, but DESIGN.md, AGENTS.md, and the README product summary still describe the old default. Individually this is a five-minute fix. As a pattern it means the repo's prose cannot be trusted to match its behavior, which matters more now that AGENTS.md is explicitly pitched at AI agents that will read the docs instead of the code. Fix the three spots and add "docs match behavior" to the release checklist in RELEASING.md.

### 4. The competitive analysis is the best strategic document in the repo; its #1 recommendation should wait for users anyway

`docs/designs/competitive-analysis.md` is genuinely good work: verified claims, honest inference tags, and a correct read that Lane's moat is "several weeks of Wayland paper cuts already absorbed" plus features nobody can copy without becoming a KDE app. Its top recommendation (KActivities-scoped routing) is the right kind of feature: Plasma-only, uncopyable, reuses the existing scope model. But it is still a feature for an audience of zero. The correct sequencing is: AUR, r/kde post, then if any stranger responds, KActivities is the first thing to build because it deepens the wedge. If nobody responds, it changes nothing.

### 5. Issue #12 is the first real bug report shaped like a user problem

The open issue (native-browser argv silently changes when the desktop Exec carries extra non-field-code flags) is auto-detected, not user-filed, but it is exactly the class of bug a real user hits and cannot diagnose: Lane launches the browser with subtly wrong arguments. It belongs in the next release alongside flatpak discovery. A 0.2.1 or 0.3.0 with flatpak support plus this fix is a reasonable, small release. Do not let it grow.

## Considered and fine

- **PolyForm NC + email commercial track.** Still right. Zero users means zero signal to change it, and the license does not block AUR or Flathub. Junction has been GPLv3 for five years and nobody resold it, but relicensing is a one-way door and there is no reason to walk through it today.
- **Flatpak discovery in Unreleased.** Justified scope: the user it serves (Andy on beelink) is real, the change is discovery-layer only, and it removes an install-time failure for the exact audience an AUR package attracts.
- **Windows port deferred by user decision.** Correct call, matches the port assessment's own recommendation.
- **No Lua, no scripting, no browser extension bridge.** Still correct. Each is a second product surface for zero users.
- **The hold HUD and path-scoped memory as differentiators.** The competitive analysis verified both are unique across six competitors. These are the things to lead with in the r/kde post, not the feature checklist.

## Next 30 days

1. **Andy: create an AUR account and SSH key, publish `lane` to AUR.** Ten minutes of human action, then `yay -S lane` works. This is the whole ballgame.
2. **Post to r/kde and KDE Discourse.** Lead with the two verified-unique features (path-scoped memory, hold HUD) and point at Junction issue #9, the four-year-old closed-unimplemented request for exactly what Lane ships. Include the AUR install line.
3. **Ship 0.2.1 or 0.3.0 with flatpak discovery, the issue #12 argv fix, and the three doc-drift fixes** (DESIGN.md:44-46, AGENTS.md:52, README.md:47-49). Small, honest, done.
4. **Do not start the Windows port, do not build KActivities routing, do not add features** unless a stranger installs Lane and asks for something. Then build that.

## Method

Read from the repo (2026-09-17): `README.md`, `COMMERCIAL.md`, `CHANGELOG.md`, `packaging/PKGBUILD`, `docs/RELEASING.md`, `docs/designs/competitive-analysis.md`, `docs/designs/port-assessment.md`, `docs/designs/windows-port-plan.md`, prior `docs/reviews/ceo.md`, `docs/designs/gstack-full-analysis.md`, `docs/reviews/design.md`. `git remote -v`, `git tag`, `git log`. Grep for `holdAutoOpen` across code, docs, schema, tests.

Verified live (2026-09-17): GitHub API for `bitskc/lane` (exists, 0 stars/forks/watchers, releases API 200), AUR RPC for `lane` (0 results), `aloneguid/bt` commits (today) and latest release (6.2.3, 2026-09-01), open issue #12.
