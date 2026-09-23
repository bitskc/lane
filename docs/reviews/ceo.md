# CEO Review: Lane (round 3)

Reviewed 2026-09-18. Repo state: `bitskc/lane` @ 1de0d59, 53 commits since v0.2.0, all fixes and polish. Still 0 stars, 0 forks, 0 watchers. AUR RPC for `lane` still returns zero results. Six auto-detected issues open, one of them a P1 regression in the picker. No stranger has installed this yet.

## Verdict

**THE WEDGE IS REAL. THE PRODUCT STILL HAS NO DOOR.**

Three consecutive reviews have now said the same thing: publish to AUR, post to r/kde, stop building until a stranger responds. The code has gotten better every round. The distribution has not moved. The wedge question is settled: "Plasma-native Choosy" is real because Junction explicitly refused profile routing (issue #9, closed unimplemented) and every other competitor is platform-locked to Mac or Windows. The niche is small but it is unowned. What is not settled is whether anyone in the niche will ever find Lane, because `yay -S lane` still does not work.

## Round-2 fix verification

| # | Round-2 finding | Verified status |
|---|---|---|
| 1 | AUR publish blocked on Andy's account | **Not done.** AUR still has no `lane` package. Third review running. |
| 2 | Windows port plan: keep doc, don't build | **Held.** No port code landed. |
| 3 | Doc drift on holdAutoOpen default | **Fixed.** DESIGN.md:44-46 now says "off by default," AGENTS.md:57 shows `"holdAutoOpen": false`, README:49 says "Optional hold bar." All three spots corrected. |
| 4 | Issue #12 argv fix in next release | **Fixed.** #12 closed; the fix is in Unreleased (native Exec flags no longer leak into launch args). |
| 5 | Ship small 0.2.1/0.3.0, don't let it grow | **At risk.** Unreleased has 53 commits of fixes. That is no longer a small release, and one of the fixes introduced a new P1 (issue #33). |

## Findings, ranked by impact

### 1. AUR is still the whole ballgame and still blocked on ten minutes of Andy's time

Same finding as rounds 1 and 2, now with more evidence that nothing else matters. The PKGBUILD is done and has survived two review waves. The repo is renamed, tagged, checksummed. The only missing step is a human creating an AUR account and pushing. Every fix merged since 0.2.0 improves a product with zero users and zero distribution. This is not a criticism of the fixes; they were worth doing. It is a statement about sequencing: the marginal user gained by any further code work is exactly zero until the package exists in a place a Plasma user would look.

### 2. v0.3.0 is pending but not actually staged, and it carries a self-inflicted P1

`CMakeLists.txt:2` still says `project(lane VERSION 0.2.0)` and `packaging/PKGBUILD:2` still says `pkgver=0.2.0`. The Unreleased changelog is large and good (flatpak discovery, in-sandbox profile paths, launch security hardening, picker ordering). But issue #33 is a P1 the project gave itself: the fixed picker section order (Web apps first) moved the ranked leader off row 0, so Enter and shortcut 1 can open the wrong target. Row 0 is load-bearing in three places (pre-selection, Enter, shortcut 1). This must be fixed or the section order reverted before tagging. Shipping 0.3.0 with a picker that opens the wrong destination on the most common keystroke would be worse than not shipping.

Also open and worth triaging before tag: #35 (flatpak gate unenforced dangerousOpts, security-labeled, medium) and #36 (env unwrap ghost targets, medium). #34, #37, #38 are low-risk and can ride a later release.

### 3. The release is ready to ask users to install it, once #33 lands

Answering the readiness question directly: yes, with one condition. The changelog shows a product that has absorbed real Wayland paper cuts (activation tokens, layer-shell nulls, flatpak remoting, notification component names) and fixed them. The security posture is stated and enforced. The docs now match behavior. The one thing standing between "ready" and "not ready" is the row-0 regression, because it sits on the product's core interaction. Fix #33, bump the version in both places, tag, publish, post. In that order.

### 4. Licensing: PolyForm NC holds up fine; pricing-unset is not a blocker

PolyForm Noncommercial is the right call for a KDE utility at this stage and there is no evidence against it. It does not block AUR (custom licenses are routine there), does not block Flathub later, and preserves the only plausible revenue path (someone wrapping Lane in a paid product or distro offering). The commercial path is credible in shape even if thin in volume: the buyer is a company shipping a Linux product or internal tooling that wants to embed or rebrand the router, not individual users. Choosy and Velja prove individuals will pay $8-10 for this category on Mac, but Lane's license already gives personal use away free, so the paid tier is correctly aimed at commercial use only. Pricing-unset is fine at zero users; "email me and we'll figure it out" is the correct price for a product with no commercial inbound. Revisit only when the first commercial inquiry arrives.

### 5. Moat: execution plus absorbed pain, and that is enough for this niche

There is no structural moat. The config format is JSON, the rules engine is conventional, and a competent team could clone the feature list in weeks. What Lane has is three things competitors demonstrably have not built (path-scoped memory, the hold HUD, agent-facing CLI/schema, all verified unique across six competitors in `docs/designs/competitive-analysis.md`) plus a pile of Wayland-specific fixes that represent absorbed cost. Junction, the only motivated Linux competitor, looked at profile routing and declined. The moat is that nobody else wants this niche badly enough to pay the entry cost, and Lane already paid it. That is a real moat for a niche product. It does not need to be bigger.

## What blocks adoption, ranked

1. **No AUR package.** The single channel where the target user (Arch-based Plasma power user) actually looks. Blocked on Andy's account creation. Everything else is noise until this exists.
2. **v0.3.0 not tagged.** Version still 0.2.0 in CMake and PKGBUILD; the flatpak work and 50+ fixes are unreleased. And it cannot be tagged until issue #33 (P1, Enter opens wrong target) is resolved.
3. **No announcement.** No r/kde post, no KDE Discourse post, no Planet KDE. Zero stars is partly a discovery problem, not only a distribution problem. The post writes itself: lead with path-scoped memory and the hold HUD, cite Junction #9.
4. **Plasma-only, Wayland-first.** A real constraint but a chosen one, and correct. GNOME users have Junction; chasing them costs the wedge.
5. **Source-only for non-Arch users.** No deb/rpm/Flathub. Secondary to AUR; revisit only if the r/kde post draws non-Arch interest.

## Deliberately not doing

- **Windows/macOS port.** Browser Tamer is free and maintained on Windows; Choosy/Velja own Mac. Porting trades the only wedge for a parity fight. The port assessment's own recommendation is "do not port." Keep the docs, build nothing.
- **Browser extension bridge.** A second product surface (Chrome + Firefox extension stores, review processes, versioning) for zero users. Revisit only if users ask for browser-to-browser hops.
- **Scripting (Lua/JS).** Explicit non-goal, correct. Finicky proves it is a maintenance-heavy feature for a niche of the niche.
- **Source-app rules.** Impossible on Wayland today; no portal hands Lane the caller's identity. Building toward it is wasted infrastructure.
- **GNOME/GTK version.** Junction exists and declined this exact feature set. Let them have it.
- **Setting a commercial price now.** No inbound, no signal. Pricing-unset is honest and correct.
- **Relicensing to GPL/MIT.** One-way door, no reason to walk through it at zero users.
- **KActivities routing, tracking-param list, reclaim watchdog.** All three are good, verified recommendations from the competitive analysis. All three wait until a stranger installs Lane. Features for an audience of zero are still features for an audience of zero.

## Considered and fine

- **Flatpak discovery.** Justified: real user (Andy on beelink), discovery-layer only, removes an install-time failure for exactly the AUR audience.
- **The 53-commit fix wave.** Not scope creep; it is the product getting honest. The auto-review loop found real bugs (stale picker rows, argv leaks, flatpak remoting) and fixed them.
- **Hold HUD off by default.** Right call; docs now agree with code.
- **Opt-in default browser, http/https only.** Correct scope and correct security posture.
- **Agent-facing CLI and AGENTS.md.** Cheap, unique, and increasingly how config files get written.

## Next 30 days

1. **Fix or revert issue #33** (picker row-0 regression). Triage #35 and #36 for the same release.
2. **Bump to 0.3.0** in CMakeLists.txt and PKGBUILD, tag, release.
3. **Andy: create the AUR account, publish `lane`.** Third time asking. Ten minutes.
4. **Post to r/kde and KDE Discourse** with the AUR install line, leading with the two verified-unique features.
5. **Then stop.** No new features until a stranger says something.

## Method

Read from the repo (2026-09-18): README.md, DESIGN.md, COMMERCIAL.md, CHANGELOG.md, packaging/PKGBUILD, CMakeLists.txt, docs/designs/competitive-analysis.md, prior docs/reviews/ceo.md, AGENTS.md (hold config section). `git log`, `git tag`, `gh issue list`, `gh issue view 33`. Verified live: GitHub API repo stats (0 stars/forks/watchers), AUR RPC info+search for `lane` (0 results).
