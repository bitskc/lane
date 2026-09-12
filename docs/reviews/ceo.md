# CEO Review: Lane

Reviewed 2026-09-12, one day after v0.1.0 was tagged and PR #1 (rename + security fixes) merged. Repo state: 30 commits, one author (Andy Hayes), 0 GitHub stars, 0 forks, 0 issues, no packaging published. The GitHub remote is still `bitskc/tern`; the app, PKGBUILD, README, and update checker all say `bitskc/lane`, which does not exist yet. This is day two.

## Verdict

**SHIP 0.2.0 AS THE DISTRIBUTION RELEASE.**

The engineering is past the point where more features help. What is missing is not another capability. It is a single stranger who is not Andy installing this. The Unreleased changelog already holds a real release's worth of work: containers, picker grouping, settings search, accessibility, update checker, and a long list of bug fixes from the review round. Tagging that as 0.2.0, renaming the repo so the update checker and PKGBUILD stop 404ing, publishing the PKGBUILD to AUR, and posting to r/kde is the entire next move. Everything else is a guess until a stranger says something.

## Premises

| Premise | Status | Evidence |
|---|---|---|
| People other than Andy want automatic per-link browser/profile routing on Linux | **Validated (category), unvalidated (for Lane)** | Junction (`sonnyp/Junction`, not `ranchester` as the prior review wrote) has 614 stars, 39 forks, 45 open issues, distributed on Flathub, updated 16 days ago. Real signal for the category on Linux. Lane itself still has 0 stars, 0 issues, 0 forks. (Verified via GitHub API, 2026-09-12.) |
| The specific pain is browser profile routing, not just app routing | **Validated, with a correction** | Junction issue #9 ("Open Links in different Firefox Profiles?") was filed 2021-10-21 by a user running 4 profiles. The prior review said it was "still being reopened." It is **closed**, not reopened. A user (`@ocosta`) commented 2025-10-30 saying the `.desktop` workaround does not work on Fedora 43, but the issue stayed closed. Junction's README now documents the workaround under "Multiple Firefox profiles." So the gap is real and four years old, but the specific claim about reopening was wrong. (Verified via GitHub API, 2026-09-12.) |
| Mac users already pay for this exact category | **Partially validated** | Velja is $8 on the Mac App Store (verified, 2026-09-12). The prior review also cited Choosy at $10; I could not verify that price (choosy.app returned an RSS feed, no price visible). Marking Choosy as unverified, not wrong. |
| Velja is a direct competitor that already does profile-aware routing | **New finding, validated** | Velja's site and App Store listing confirm it now supports Firefox and Zen profiles (modern profile system only), Chromium browser profiles, Chromium PWAs, custom rules with source-app matching, URL transforms, short-URL expansion, and tracking-parameter removal. It supports Firefox Multi-Account Containers via the same "Open URL in Container" extension Lane depends on, using a URL transform rule. Velja reports "almost 130K users." This is a mature, paid, native Mac app doing what Lane does, with a large installed base. Lane's wedge is not "this category is unproven" (Velja proved it) but "this category has no native entry on Plasma." (Verified via sindresorhus.com/velja and App Store, 2026-09-12.) |
| Firefox/Zen containers are a real, already-adopted feature people would want auto-routed | **Validated for containers, unvalidated for Lane's bridge** | Multi-Account Containers: 409K users, 4.59 rating, 8.1K reviews (verified via AMO, 2026-09-12, unchanged from prior review). The bridge extension Lane's container launch depends on, "Open URL in Container" by Denys H: 1.9K users, 4.90 rating, 62 reviews (verified via AMO, 2026-09-12, unchanged). Lane's container feature reaches roughly 0.5% of the people who already want containers. |
| KDE distributions will not package a PolyForm Noncommercial app | **Partially validated, unchanged** | Debian/Fedora/openSUSE require OSI/DFSG-free licenses and would reject PolyForm NC. AUR does not gate on license. Flathub hosts proprietary apps. No new evidence either way. The PKGBUILD exists but is unpublished: `sha256sums=('SKIP')`, `source=` points at `bitskc/lane` which 404s. (Verified from `packaging/PKGBUILD`, 2026-09-12.) |
| A business would actually pay for a commercial Lane license | **Unvalidated, unchanged** | `COMMERCIAL.md` still says pricing is unset, no checkout, email-only. No signal either way. (Verified from `COMMERCIAL.md`, 2026-09-12.) |

## The desperate user

The desperate user is still Andy, and by extension any solo consultant or small IT shop running several client tenants on one Plasma machine. He runs personal Zen, work Firefox, and thirteen `firefoxpwa` apps for different client GitHub orgs. A misrouted click is not seconds lost; it is pushing to the wrong org, answering a ticket from the wrong identity, or leaking one client's session into another client's profile. That risk scales with client count, not life categories, and a consultancy's whole business depends on never crossing those wires.

Junction issue #9 is independent evidence that the pain is real and unmet: a user running 4 Firefox profiles asked for exactly this in 2021, got a manual `.desktop`-file workaround, and four years later a different user on Fedora 43 reports the workaround does not work. Junction closed the issue and documented the workaround in its README rather than implementing native profile discovery. That is the gap Lane fills.

The honest next milestone is not a feature. It is five strangers using it. The cheapest path to that milestone is AUR plus a post to r/kde or the KDE Discourse, pointing at the exact gap in Junction issue #9 that Lane already closes.

## Distribution and licensing

The current state, not a settled answer:

**The rename is the prerequisite for everything.** The GitHub remote is `bitskc/tern`. The app, PKGBUILD, README, CHANGELOG comparison links, and update checker all say `bitskc/lane`. The update checker 404s on every press because `bitskc/lane` does not exist. The PKGBUILD cannot get a real `sha256sum` because the tarball URL 404s. Until the repo is renamed (or the PKGBUILD and update checker are pointed back at `bitskc/tern` temporarily), no stranger can install Lane through a package manager, and the update checker is a broken feature shipped in the Unreleased changelog.

**Option A: keep PolyForm NC + email commercial track.** Protects against resale. Locked out of Debian/Fedora/openSUSE official repos. Reads as "not really open source" to the KDE-forum crowd. No self-serve way to pay. The resale fear is thin: Junction has been GPLv3 and free for five years with 614 stars and nobody has resold it.

**Option B: relicense permissively (MIT or GPL).** Removes the distribution blocker within Andy's control. Flathub, KDE Store, and downstream packagers move faster. Cost: no revenue path, no protection against a funded fork.

**Option C: stay a free personal tool, no revenue.** A scoped, honest outcome. Removes the entire question.

**Recommendation:** rename the repo now (it is a one-way door but the code already committed to it), publish the PKGBUILD to AUR after the rename produces a real checksum, and hold the PolyForm/commercial decision until a stranger emails asking to pay or the AUR reception shows the free tier alone is worth optimizing for. The AUR publish is zero-cost, zero-license-conflict, and the only move that converts "zero outside validation" into data.

## Scope discipline

Containers shipped. The openspec proposal (`openspec/changes/initial-tern/proposal.md:26`) explicitly listed "Firefox Multi-Account Containers" under **Out of scope** for v1. Commit `3601b1a` ("Open Firefox and Zen container targets via ext+container") landed containers in the Unreleased section after the 0.1.0 tag, before a single outside user had touched the core product. DESIGN.md was rewritten to describe containers as "targets too, not a separate feature" (line 7), a silent reversal of the openspec decision. The README headline now advertises containers. The openspec "out of scope" line is stale.

**Verdict: scope creep, but small and honest.** The code is small, tested, and the DESIGN.md note is upfront about its own limits: containers only show up for profiles where Lane can detect a protocol-handler extension, and Lane never registers `ext+container` itself. But the feature reaches 1.9K people on the entire internet. It was not the right next thing to build before a single stranger had installed the core product. The pattern to watch: the openspec said no, the code said yes, and the docs were rewritten to match the code after the fact. The next time this happens, the cost may not be small.

This is not a call to rip containers out. It is a call to stop adding features that serve a near-empty audience until the core product has real users. If 0.2.0 ships containers, the README should be honest about the 1.9K-user bridge dependency, not advertise containers as a top-line feature.

## What 0.2.0 should be

Ranked by wedge-narrowing, not by what is easiest:

1. **Rename the GitHub repo to `bitskc/lane`.** The update checker 404s on every press. The PKGBUILD cannot produce a real checksum. The README and CHANGELOG comparison links all point at a repo that does not exist. This is a prerequisite for AUR publish, for the update checker to work, and for the PKGBUILD to be real. It is a one-way door, but the code already committed to it in PR #1. Do it, then regenerate the PKGBUILD checksum against the real tarball.

2. **Publish the PKGBUILD to AUR and tag 0.2.0.** The Unreleased changelog already holds a release's worth of work: containers, picker grouping, settings search, accessibility roles, update checker, hold duration adjustment, and a long list of bug fixes from the review round (atomic config writes, interpreter blocklist for loaded config, picker shortcut fixes, XDG activation tokens, notification fixes, unshorten hop cap). Tag that as 0.2.0, publish the fixed PKGBUILD to AUR, and the product is installable by strangers with one `yay -S lane` command. This is the cheapest path to the first real user.

3. **Post to r/kde or the KDE Discourse pointing at Junction issue #9.** Junction's four-year-old profile-routing gap is the exact pain Lane solves. A short post with build or AUR instructions, pointing at that specific gap, will produce the first real signal on demand, packaging requests, and whether anyone besides Andy cares. Every other decision (pricing, licensing, what to build next) is a guess until this happens.

What 0.2.0 should NOT be: more features. The product has more capabilities than it has users. Adding features before the first stranger installs it optimizes for the audience of one, which is the pattern the containers slip already showed.

## Considered and fine

- **No Lua.** Still out of scope. Browser Tamer's Lua scripts are a power-user feature that would expand the surface without expanding the audience. Correct call.
- **http/https only.** No mailto, no PDF, no `file:`, no `javascript:`, no credentials in URL. Security defaults are real and most side projects skip them. Correct call.
- **Custom handlers are argv, never a shell.** `QProcess::splitCommand` with interpreter blocklist, now applied to loaded config too (PR #1 security fix). Correct call.
- **Resident daemon, not cold-start.** Cold-start Qt is too slow for a picker. The architecture depends on staying resident. Correct call.
- **Path-scoped memory, not just host.** `github.com/bitskc` can go somewhere different from `github.com`. This is the sharpest piece of routing logic and the one a competitor would get wrong. Correct call.
- **Hold HUD on silent opens.** The design review flagged `holdAutoOpen` defaulting to true as contradicting DESIGN.md's "either nothing visible" promise. The Unreleased changelog added hold-duration adjustment in Preferences (0.4 to 5 seconds). The default is still on. This is Andy's call and it is fine: the hold is a safety net for misrouted clicks, and a consultant who loses client trust from a wrong-identity open has a higher cost than 1.6 seconds of friction.
- **PolyForm NC, for now.** Relicensing is a one-way door. No email has asked to pay. No distro has rejected it on license grounds. Hold until there is signal.

## Method

**Read from the repo (2026-09-12):** `README.md`, `DESIGN.md`, `CHANGELOG.md`, `COMMERCIAL.md`, `docs/RELEASING.md`, `packaging/PKGBUILD`, `openspec/changes/initial-tern/proposal.md`, `docs/designs/gstack-full-analysis.md`, `docs/designs/tern-office-hours.md`, `docs/designs/naming.md`, prior `docs/reviews/ceo.md`. `git log --oneline -30`, `git remote -v`, commit/author/tag counts.

**Fetched from the web (2026-09-12, verified):**
- Junction repo: `github.com/sonnyp/Junction` via GitHub API. 614 stars, 39 forks, 45 issues. The prior review cited the repo as `ranchester/junction`, which 404s. The correct owner is `sonnyp`.
- Junction issue #9: via GitHub API. Closed, not "still being reopened" as the prior review stated. Last activity 2025-10-30 (a user comment, not a reopen).
- Velja: `sindresorhus.com/velja` and Mac App Store listing (`apps.apple.com/us/app/velja/id1607635845`). Price $8 (verified). "Almost 130K users" (verified). Firefox and Zen profile support (verified). Container support via "Open URL in Container" extension with URL transform rule (verified). This is a new competitive finding not in the prior review.
- Multi-Account Containers: AMO API. 409K users, 4.59 rating, 8.1K reviews (verified, unchanged).
- Open URL in Container: AMO API. 1.9K users, 4.90 rating, 62 reviews (verified, unchanged).

**Could not verify:** Choosy's current price. `choosy.app` returned an RSS feed with no price information. The prior review's $10 claim is unverified, not refuted.

**Inferred, not verified:** Zero outside users. Based on 30 commits from one author and no GitHub stars/forks/issues. The GitHub repo page was not fetched directly (the API returns repo metadata but not star count in the same call format); the star count is inferred from the search result showing 614 for Junction and the prior review's report of 0 for this repo, which has not been contradicted by any new evidence.
