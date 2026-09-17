# Lane: office hours, round 2

Synthesis of the 2026-09-17 gstack review round (`docs/reviews/eng.md`,
`docs/reviews/ceo.md`, `docs/reviews/design.md`,
`docs/reviews/devex.md`), five days after v0.2.0 shipped, framed as the
questions a YC-style office-hours session would ask before talking
about roadmap. See `docs/designs/gstack-full-analysis.md` for the fix
list and the contradictions between reviews. The round-1 version of
this file is in git history.

## The idea

Lane is a resident Qt 6 / Kirigami daemon that becomes the KDE Plasma
default `http`/`https` handler and routes each click to the right
browser profile, Firefox/Zen container, `firefoxpwa` web app, flatpak
browser, or custom handler, based on rules, remembered per-path
choices, and a picker overlay for anything new. It exists because one
machine, Andy's, runs Zen for personal use, Firefox for work, and
thirteen `firefoxpwa` apps for separate client GitHub orgs, and KDE
only lets you register one default browser. The bet is that "which app
should this link open in" is a solved problem on Mac (Velja, $8, ~130K
users) and an unsolved one on Plasma.

## Six questions

**Is the demand real, or assumed?** Same answer as round 1, sharper.
The category is validated: Junction has hundreds of stars and Velja
reports ~130K paid users on Mac. Lane itself still has 0 stars, 0
forks, 0 watchers, and zero installs by anyone who is not Andy
(`docs/reviews/ceo.md:3`). What changed is that the last excuse is
gone: the repo is renamed, v0.2.0 is tagged and released, the update
checker works, and the PKGBUILD builds clean with a real checksum. The
only thing between Lane and its first stranger is an AUR account that
does not exist yet. Demand is still assumed, but it is now one
ten-minute human action away from being testable.

**What is the status quo without Lane?** Unchanged: one default browser
and eat the misrouted clicks, taskbar icons by memory, or a hand-rolled
`.desktop` hack that a Fedora 43 user reported broken in October 2025.
Junction issue #9, the four-year-old profile-routing request, is still
closed-unimplemented and still the best single piece of evidence that a
Plasma user wants exactly this. The status quo is degrading, not just
inconvenient.

**Who is the most desperate user, concretely?** Still Andy, and by
extension any solo consultant running several client tenants on one
Plasma machine. Round 2 added a second concrete instance: Andy's
beelink, which runs flatpak browsers and is why flatpak discovery
shipped. That is the correct kind of user to build for right now: real,
present, and representative of the AUR audience. The desperate user
runs multiple browser profiles; containers remain a narrower niche
behind a 1.9K-user bridge extension.

**What is the narrowest wedge worth paying for?** Confirmed by the new
competitive analysis (`docs/designs/competitive-analysis.md`): the moat
is Plasma-native integration plus the features nobody can copy without
becoming a KDE app. Path-scoped memory and the hold HUD are verified
unique across six competitors. The wedge is not feature count; it is
several weeks of Wayland paper cuts already absorbed. The analysis's
top recommendation, KActivities-scoped routing, is the right next
feature and should still wait for a stranger.

**What can be observed today versus what is still assumed?** Observed:
all round-1 fixes held across four independent re-verifications; 11
tests green in CI on an Arch container; the release workflow produced
v0.2.0 correctly; the update checker returns v0.2.0; the PKGBUILD
package contents are correct. Observed: doc drift is a pattern, not an
accident. `holdAutoOpen` flipped to opt-in in code, schema, tests,
changelog, and metainfo, but DESIGN.md, README, and AGENTS.md still
describe the old default. Flatpak discovery shipped undocumented. The
screenshots still show Tern. Observed: the remaining bugs are narrow,
not structural (a sub-second launch race, a stale picker, a flatpak
`--command=` blocklist hole). Assumed: that a KDE user who is not Andy
would install this. That assumption is still untested because the AUR
package still does not exist.

**Does this survive two to three years?** Same conditional answer.
Platform risk is real (LayerShellQt is a hard dependency; KF6 churn
already tripped one release), bus factor is one, and Velja porting to
Linux would narrow the wedge to Plasma-native alone. What improved: the
repo now has a canonical openspec, an honest changelog, a working
release pipeline, and a competitive analysis that names the moat
precisely. What did not: there is still no distribution channel, so
none of it matters yet.

## The assignment

One thing, and it is smaller than last round: Andy creates an AUR
account and SSH key and pushes the PKGBUILD to `aur.archlinux.org/
lane.git`. Ten minutes of human action that no agent can do. Then post
to r/kde and KDE Discourse leading with the two verified-unique
features (path-scoped memory, hold HUD) and pointing at Junction issue
#9. Then ship 0.2.1 with flatpak discovery, the issue #12 argv fix, the
picker empty state, and the doc-drift sweep. Then stop building until a
stranger says something. If a stranger responds, KActivities routing is
the first thing to build. If nobody responds, nothing else matters.
