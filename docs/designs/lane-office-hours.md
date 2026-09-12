# Lane: office hours

Synthesis of the 2026-09-12 gstack review round (`docs/reviews/eng.md`,
`docs/reviews/ceo.md`, `docs/reviews/design.md`, `docs/reviews/devex.md`,
`docs/reviews/openspec.md`), framed as the questions a YC-style
office-hours session would ask before talking about roadmap. See
`docs/designs/gstack-full-analysis.md` for the fix list and the
contradictions between reviews.

## The idea

Lane is a resident Qt 6 / Kirigami daemon that becomes the KDE Plasma
default `http`/`https` handler and routes each click to the right browser
profile, Firefox/Zen container, `firefoxpwa` web app, or custom handler,
based on rules, remembered per-path choices, and a picker overlay for
anything new. It exists because one machine, Andy's, runs Zen for personal
use, Firefox for work, and thirteen `firefoxpwa` apps for separate client
GitHub orgs, and KDE only lets you register one default browser. The bet
is that "which app should this link open in" is a solved problem on Mac
(Velja, $8, ~130K users) and an unsolved one on Plasma.

## Six questions

**Is the demand real, or assumed?** Validated for the category, still
unvalidated for Lane. Junction (`sonnyp/Junction`, GPLv3, on Flathub) has
614 stars, 39 forks, and 45 open issues, proving that "pick which app
opens this link" is a category people want on Linux
(`docs/reviews/ceo.md:15`). Velja, the Mac equivalent, reports almost
130K users and charges $8, confirming the category supports a paid
native app (`docs/reviews/ceo.md:18`). Lane itself has 0 stars, 0 forks,
0 issues, 30 commits from one author, and has never been shown to anyone
outside Andy (`docs/reviews/ceo.md:3`). The wedge is narrower than last
round believed: Velja already does Firefox/Zen profiles, Chromium
profiles, containers via the same bridge extension Lane depends on,
custom rules with source-app matching, short-URL expansion, and
tracking-parameter removal. The category is not just validated, it is
competitive. Lane's wedge is "no native entry on Plasma," not "this
category is unproven."

**What is the status quo without Lane?** Three bad options, all manual:
set one default browser and eat the risk of a personal link opening in a
work window (or the reverse), keep every profile pinned to the taskbar
and middle-click the right icon by memory every time, or hand-roll a
`.desktop`-file redirect hack. New evidence since last round: Junction
issue #9, the four-year-old profile-routing request, is closed, not
reopened as the prior review claimed. But a user on Fedora 43 commented
on 2025-10-30 that the `.desktop` workaround does not work
(`docs/reviews/ceo.md:16`). The status quo is actively degrading, not
just inconvenient. None of the three options are automatic, and none
scale past "I remember which icon is which."

**Who is the most desperate user, concretely?** Andy himself, and by
extension any solo consultant or small IT shop running several client
tenants on one Plasma machine. He runs personal Zen, work Firefox, and
thirteen `firefoxpwa` apps for different client GitHub orgs. A misrouted
click there is not an annoyance; it is pushing to the wrong org,
answering a ticket from the wrong identity, or leaking one client's
session into another client's profile (`docs/reviews/ceo.md:25`).
Junction issue #9 is independent evidence: a user running 4 Firefox
profiles asked for exactly this in 2021, got a manual workaround, and
four years later a different user reports it broken on Fedora 43
(`docs/reviews/ceo.md:27`). The desperate user is narrower than the
container numbers suggest: the bridge extension Lane's container feature
depends on has 1.9K users out of 409K who have Multi-Account Containers
(`docs/reviews/ceo.md:19`). The core desperate user runs multiple
browser profiles, not necessarily containers.

**What is the narrowest wedge worth paying for?** Not "choose an app for
a link" in general. Velja proved that is a paid category with 130K users
on Mac (`docs/reviews/ceo.md:18`). Not "profile routing" specifically;
Velja does Firefox/Zen profiles and containers. The wedge is native
Plasma integration: Wayland layer-shell overlays with exclusive keyboard
grab, path-scoped memory (`github.com/bitskc` can go somewhere different
from `github.com`), hold-to-veto on silent opens, and a resident daemon
for sub-200ms picker response. That is the part no competitor has on
Linux (`docs/reviews/ceo.md:71`). Containers are a convergent
scope-creep flag, not the wedge: both the CEO and the openspec auditor
independently flagged that containers shipped before a single outside
user and reach 1.9K bridge-extension users (`docs/reviews/ceo.md:47-49`,
`docs/reviews/openspec.md:24`). The wedge is the Plasma-native routing
core, not the feature count.

**What can be observed today versus what is still assumed?** Observed:
real engineering quality. All five round-1 fixes held
(`docs/reviews/eng.md:162-169`). 10/10 tests pass in 1.37s
(`docs/reviews/eng.md:6-9`). Build with deps present takes ~51s
(`docs/reviews/devex.md:14`). 44 targets discovered on this machine
(`docs/reviews/eng.md:8`). Observed: docs describe features that do not
exist. `lane --rediscover` and `lane --configure` are in README,
AGENTS.md, and CHANGELOG but rejected by `QCommandLineParser` with
"Unknown option" (`docs/reviews/devex.md:53-55`). `location: "title"`
and `location: "process"` rule conditions are documented in AGENTS.md
but `SourceInfo.cpp:6-11` is a stub returning empty
(`docs/reviews/eng.md:136-142`). The openspec proposal says "no
containers" but containers shipped (`docs/reviews/openspec.md:24`).
Observed: the GitHub repo rename blocks everything. The remote is still
`bitskc/tern`; the app, PKGBUILD, README, CHANGELOG, metainfo, and
update checker all say `bitskc/lane`, which does not exist. The update
checker 404s on every press. The PKGBUILD cannot get a real checksum
(`docs/reviews/ceo.md:35`, `docs/reviews/devex.md:56-59`). Assumed: that
a KDE user who is not Andy would install this. That containers are
wanted by Lane's users (1.9K bridge users, not 409K). That a business
would pay for a commercial license (`COMMERCIAL.md` pricing is unset, no
inquiries, `docs/reviews/ceo.md:21`).

**Does this survive two to three years?** Conditionally. Platform risk
is real, not hypothetical. `LayerShellQt` is a hard `REQUIRED` build
dependency (`CMakeLists.txt:53-54`), so the app is Wayland-layer-shell-only
by construction. KF6 dependency churn already tripped this release:
`kcrash` and `kcolorscheme` were missing from README and PKGBUILD, now
fixed (`docs/reviews/devex.md:26-31`). A compositor or KF6 API change is
a plausible future break. Competitive risk is higher than last round:
Velja has 130K users and already does profiles and containers on Mac. If
Velja or a similar app ports to Linux, Lane's wedge narrows to
Plasma-native integration alone. Bus factor is one maintainer
(`docs/reviews/ceo.md:3`), a real risk for "the thing that decides where
every link goes." Against that: the core routing logic is small,
well-tested, and depends on nothing exotic (`docs/reviews/eng.md:13-18`).
KDE Plasma on Wayland is a growing surface. This survives if Andy keeps
maintaining it or if adoption grows enough that someone else would step
in. It does not survive neglect the way a declarative config file would.

## The assignment

One thing: get a stranger to install it. The prerequisite is the repo
rename, because the update checker 404s and the PKGBUILD cannot produce a
checksum until `bitskc/lane` exists. Land the two crash-class fixes
(re-entrancy guard in `openUrl`, null check in `configureLayerShell`)
before the tag, because a segfault in a default-browser handler is the
worst possible first impression. Fix the docs that lie (register the CLI
flags or remove them, remove or implement the dead rule locations, align
the schema minimum) in the same commit. Tag 0.2.0, publish the PKGBUILD
to AUR, and post to r/kde pointing at Junction issue #9. Everything else
is a guess until a stranger says something.
