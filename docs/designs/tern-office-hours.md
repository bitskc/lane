# Tern: office hours

Synthesis of the 2026-09-11 gstack review round (`docs/reviews/eng.md`,
`docs/reviews/ceo.md`, `docs/reviews/design.md`, `docs/reviews/devex.md`),
framed as the questions a YC-style office-hours session would ask before
talking about roadmap. See `docs/designs/gstack-full-analysis.md` for the
fix list and the contradictions between reviews.

## The idea

Tern is a resident Qt 6 / Kirigami daemon that becomes the KDE Plasma
default `http`/`https` handler and routes each click to the right browser
profile, Firefox/Zen container, `firefoxpwa` web app, or custom handler,
based on rules, remembered per-path choices, and a picker overlay for
anything new (`README.md:1-12`, `DESIGN.md:1-7`). It exists because one
machine - Andy's - runs Zen for personal use, Firefox for work, Brave, and
thirteen `firefoxpwa` apps for separate client GitHub orgs, and KDE only
lets you register one default browser (`openspec/changes/initial-tern/proposal.md:5-7`).
The bet is that "which app should this link open in" is a solved problem on
Mac (Choosy, Velja) and an unsolved one on Plasma.

## Six questions

**Is the demand real, or assumed?** Partially real, mostly assumed. Junction
(the closest GNOME/GTK equivalent, GPLv3, on Flathub) has 614 stars, 39
forks, and 45 open issues - real proof that "pick which app opens this
link" is a category people want on Linux (`docs/reviews/ceo.md:15`). Tern
itself has 0 stars, 0 forks, 0 issues, and has never been shown to anyone
outside Andy (`docs/reviews/ceo.md:3,15`). The category is validated; the
product is not.

**What is the status quo without Tern?** Three bad options, all manual:
set one default browser and eat the risk of a personal link opening in a
work window (or the reverse), keep every profile pinned to the taskbar and
middle-click the right icon by memory every time, or hand-roll a
`.desktop`-file redirect hack - which the Junction community has already
tried and documented as breaking on newer distro versions for no obvious
reason (`docs/reviews/ceo.md:24`). None of the three scale past "I
remember which icon is which," and none are automatic.

**Who is the most desperate user, concretely?** Andy himself, and by
extension any solo consultant or small IT shop running several client
tenants on one machine. He runs personal Zen, work Firefox, and thirteen
`firefoxpwa` apps for different client GitHub orgs; a misrouted click there
is not an annoyance, it is pushing to the wrong org or leaking one client's
session into another client's profile (`docs/reviews/ceo.md:26`). Junction
issue #9, open since 2021 and still being reopened by a different user as
of October 2025, is independent evidence that "route links by profile" is a
four-year-old unmet need in the adjacent category (`docs/reviews/ceo.md:16`).

**What is the narrowest wedge worth paying for?** Not "choose an app for a
link" in general - that is a solved, largely free-to-clone category (any
competent KDE developer reproduces the picker and hold UI in a weekend,
per `docs/reviews/ceo.md:44`). The wedge is profile- and container-aware
routing specifically: two independent Mac apps (Choosy, $10 one-time since
2010; Velja, $8 one-time since roughly 2025) already clear the bar for a
paid indie utility in exactly this category (`docs/reviews/ceo.md:17`), and
Junction's four-year-old profile-routing request shows the Linux side of
that same wedge is unmet, not undesired.

**What can be observed today versus what is still assumed?** Observed:
real engineering quality (5,474 LOC under `src/`, 9 passing test suites,
a tested decision order, argv-only launch with no shell injection path -
`docs/reviews/eng.md:245-250,205-213`), a working `PKGBUILD` already sitting
in the repo (`docs/reviews/ceo.md:19,56`), and zero GitHub stars, forks,
issues, or packaging anywhere (`docs/reviews/ceo.md:3`). Assumed: that a
KDE user who is not Andy would actually install this, and that a business
would pay for a commercial license - `COMMERCIAL.md:25` states pricing is
explicitly unset and the email-in flow has never been used by anyone
outside Andy (`docs/reviews/ceo.md:20`).

**Does this survive two to three years?** Conditionally. The platform risk
is real, not hypothetical: this exact release already tripped on KF6
dependency churn - `CMakeLists.txt:39-49` requires `ColorScheme` and
`Crash` components that CI knew about but the README and `PKGBUILD` did
not (`docs/reviews/eng.md:60-75`, `docs/reviews/devex.md:18-27`), and the
whole app hard-requires `LayerShellQt` and Wayland layer-shell support at
build time (`CMakeLists.txt:53-54`), so a compositor or KF6 API change is
a plausible future break, not a remote one. It is also a single-maintainer
project (`docs/reviews/ceo.md:3`), which is a real bus-factor risk for
"the thing that decides where every link in my browser goes." Against
that: the core routing/discovery logic is small, well-tested, and doesn't
depend on anything exotic (`docs/reviews/eng.md:8-19`), and KDE Plasma on
Wayland is a growing, not shrinking, surface. This survives if Andy keeps
maintaining it or if it gets enough outside adoption that someone else
would step in; it does not survive neglect the way a purely declarative
config file would.

## Premises

Pulled directly from `docs/reviews/ceo.md`'s premises table; not
re-derived here.

| Premise | Status | What would validate it |
|---|---|---|
| People other than Andy want automatic per-link browser/profile routing on Linux | Validated (adjacent category), unvalidated (for Tern specifically) | Junction's 614 stars/39 forks/45 issues prove the category; Tern has 0 stars/0 issues/0 forks (`ceo.md:15`) |
| The specific pain is browser *profile* and *container* routing, not just app routing | Validated | Junction issue #9, a 2021 request still being reopened in October 2025, shows a real unsolved multi-profile gap (`ceo.md:16`) |
| Mac users already pay for this exact category | Validated | Choosy ($10, since 2010) and Velja ($8, since ~2025) both clear the bar for a paid indie Mac utility in this category (`ceo.md:17`) |
| Firefox/Zen containers are a real, adopted feature people would want auto-routed | Validated for containers, unvalidated for Tern's bridge to them | Multi-Account Containers: 409K users, 4.59/5 over 8.1K reviews. The extension Tern's container launch depends on to actually act on the link, "Open URL in Container," has 1.9K users (`ceo.md:18`) |
| KDE distributions will not package a PolyForm Noncommercial app | Partially validated | Debian/Fedora/openSUSE require OSI/DFSG-free licenses and would reject it; AUR does not gate on license freedom and already has a working `PKGBUILD`; Flathub hosts proprietary apps too, just with slower review (`ceo.md:19`) |
| A business would actually pay for a commercial Tern license | Unvalidated | `COMMERCIAL.md` pricing is explicitly unset, no checkout exists, and the ask-by-email flow has never been used by anyone outside Andy (`ceo.md:20`) |

## Approaches

**(A) Stay a free personal tool, no distribution push.** Effort: zero -
this is the current state. Risk: none. Tradeoff: permanently solves
Andy's own multi-tenant workflow, which is a legitimate and sufficient
outcome on its own (`docs/reviews/ceo.md:36`), but the PolyForm/commercial
apparatus already built (`COMMERCIAL.md`, license header in `README.md:178`)
goes unused, and the real engineering quality here never gets outside
signal.

**(B) Free and packaged, with adoption as the goal.** Effort: roughly a
day total - fix the `PKGBUILD` (see fix list, ~1 hour), publish to AUR,
and post to r/kde or the KDE Discourse pointing at the Junction issue #9
gap Tern already closes (`docs/reviews/ceo.md:56-57`). Risk: moderate -
real strangers on day one means the config-write non-atomicity finding
(eng blocking #2) stops being theoretical, and it means running with the
PolyForm Noncommercial license a little longer even though the CEO review
flags it as friction for exactly the KDE-forum crowd this move targets
(`docs/reviews/ceo.md:32`). Tradeoff: this is the one move every premise
in the table above actually supports today - the artifact already exists,
the target user is already visible in someone else's issue tracker, and it
is the only way to convert "zero outside validation" into real data.

**(C) Paid commercial track.** Effort: high - a real price, a checkout or
at least a working sales motion, and probably a firmer commercial license
than the current email-and-figure-it-out flow (`COMMERCIAL.md:25`). Risk:
high, and mostly unvalidated risk: there is currently no evidence anyone
would pay (`docs/reviews/ceo.md:20`), so this is spending real effort
against a premise the review explicitly marks unvalidated. Tradeoff: this
should not be the next move. It only becomes a reasonable bet once (B) has
produced either an inbound "can I pay for this" email or a large enough
free user base to make packaging/support costs worth offsetting.

## What the repo already decided

Deliberate decisions visible in the source, so a future session does not
reverse them by accident:

- **Tern never registers or handles `ext+container` itself.** It only
  wraps the URL as argv and hands it to the browser; whether a container
  link actually lands in the container depends on a third-party
  protocol-handler extension (`DESIGN.md:7`). The CEO review's 10-star
  recommendation proposes reversing this; see
  `docs/designs/gstack-full-analysis.md#contradictions` - it is a live
  decision point, not settled either way.
- **http and https only.** `file:`, `javascript:`, `data:`, and
  credentials-in-URL are refused outright; mailto and PDF are explicitly
  not stolen from their own handlers (`DESIGN.md:23,29`;
  `openspec/changes/initial-tern/proposal.md:27`).
- **Custom handlers are argv, never a shell.** `QProcess::splitCommand`,
  with interpreters (`bash -c`, `python -c`, …) rejected
  (`DESIGN.md:27`).
- **Rules beat convenience, unconditionally in the stated design.**
  Explicit rules win over remembered destinations, unique PWA scope,
  picker policy, and default, in that order (`DESIGN.md:32-40`).
- **No Lua.** Explicitly out of scope, inherited from the "Browser Tamer
  is the strategic model" framing (`DESIGN.md:52`;
  `openspec/changes/initial-tern/proposal.md:25`).
- **Firefox Multi-Account Containers were explicitly out of scope for
  v1** (`openspec/changes/initial-tern/proposal.md:26`) - and then shipped
  anyway in the first `Unreleased` entry after the 0.1.0 tag
  (`CHANGELOG.md:10-21`). This is a decision that was silently reversed,
  not one that is still in force; treat the openspec "out of scope" line
  as stale on this point rather than authoritative, and see the fix list
  for what to do about it.
- **Resident daemon, not cold-start.** Cold-start Qt is documented as too
  slow for a picker; the whole architecture depends on staying resident
  (`DESIGN.md:13`).
- **Settings is a persistent two-pane window, no hamburger.**
  (`DESIGN.md:19`). The design review confirms this shell decision is
  sound; the problem it found is page content length at scale, not the
  nav pattern (`docs/reviews/design.md:80`).
- **Dual licensing: PolyForm Noncommercial + a separate, unpriced
  commercial track** (`README.md:176-180`, `COMMERCIAL.md`). Free for
  personal/hobby use; commercial use requires contacting Andy directly.
