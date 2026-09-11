# Tern: full gstack analysis

Synthesis of four independent reviews run 2026-09-11 against Tern v0.1.0
(`docs/reviews/eng.md`, `docs/reviews/ceo.md`, `docs/reviews/design.md`,
`docs/reviews/devex.md`). This document does not re-review the code; it
reconciles what the four reviewers already found, resolves the two places
they point in different directions, and orders everything into one fix
list. See `docs/designs/tern-office-hours.md` for the product-strategy
framing.

## Executive summary

Tern is a Qt 6 / Kirigami daemon that becomes the KDE Plasma default
browser handler and routes each `http`/`https` click to the right browser
profile, container, PWA, or custom handler, tagged v0.1.0 today with 5,474
lines under `src/`, 9 passing test suites, and zero outside users
(`docs/reviews/eng.md:245-250`, `docs/reviews/ceo.md:3`). The routing core
is genuinely well built - tested decision order, argv-only process launch,
a real Wayland layer-shell overlay - but three independent defects sit
between this and being safe to hand to a stranger: a hand-edited config
target skips the interpreter blocklist entirely, a config write that is
not atomic can silently wipe every rule on a bad-timed crash, and the
README's own build instructions fail on a clean Arch box because two
required packages are missing from both the README and the `PKGBUILD`
(`docs/reviews/eng.md:21-75`). That last bug was found independently by
two different reviewers, which is the strongest signal in this review
round: it is not a matter of taste, it breaks the first-run path for
every single new user until it is fixed. The single most important thing
to do next is the roughly one day of fixes at the top of the fix list
below, after which the CEO's recommendation to publish to AUR this week
becomes safe to execute rather than premature.

## Verdicts

| Review | Verdict | Reason |
|---|---|---|
| Eng (`docs/reviews/eng.md`) | **SHIP WITH FIXES** | Routing core (`router.cpp`, `destination.cpp`, `matcher.cpp`) is small, well-factored, and genuinely tested; three concrete blocking defects (interpreter blocklist gap, non-atomic config write, missing build deps) need to land before 0.1.1 (`eng.md:8-19`) |
| CEO (`docs/reviews/ceo.md`) | **NARROW THE WEDGE** | Engineering and taste are real, but there is zero outside validation, a licensing story that has never met a real buyer, and one already-visible instance of scope creep (containers); pick the one proven-desperate user, ship the one thing that gets them to switch, get five strangers using it (`ceo.md:7-9`) |
| Design (`docs/reviews/design.md`) | **Fix the hold default, then the scale problem** (review has no single verdict line; this is its own stated "the one thing") | Scores span 3/10 (accessibility) to 8/10 (visual identity); the headline issue is `holdAutoOpen` defaulting to true, which contradicts `DESIGN.md`'s own "either nothing visible" promise on every remembered/PWA/default open (`design.md:22-28`) |
| Devex (`docs/reviews/devex.md`) | **Buildable, but the docs misstate the first step** (review has no single verdict line; synthesized from its TTHW and ranked-fixes sections) | Clean build + test is ~42 seconds once dependencies are present, but the README's own `pacman` line omits two packages `CMakeLists.txt` requires, so a literal first-run fails at configure time (`devex.md:5-27`) |

## Cross-review consensus

Three findings were reached independently by different reviewers, working
from different parts of the codebase. Independent convergence is the
strongest kind of signal in a four-review set, so each is named here as
one theme rather than scattered across separate line items.

**Missing build dependencies - found separately by eng and devex.** The
eng review traced `CMakeLists.txt:39-49`'s `REQUIRED COMPONENTS` list
(which includes `ColorScheme` and `Crash`) against `README.md:158-160` and
`packaging/PKGBUILD:8-14` and found both omit the Arch packages
`kcolorscheme` and `kcrash` (`eng.md:60-75`). The devex review arrived at
the same two missing packages independently, by literally following the
README on a scratch build and cross-checking against what CI installs
(`devex.md:18-27`). Two reviewers, one root cause, and it is not a
cosmetic doc gap: it breaks the documented "clean clone on a fresh
machine" path - the exact scenario the README section is written for -
for every new user until it is fixed.

**Nothing in the UI scales to the product's own success case.** The
design review scored Settings IA 4/10 and picker density 5/10, both
citing the same underlying problem: `TargetsPage.qml` and `Picker.qml`
were built and screenshotted against a handful of targets
(`design.md:12,15,94,108`). The eng review, working from the live daemon
on this machine, measured 45 real targets - browser profiles, containers,
and PWAs - via `tern --list` (`eng.md:256-259`). Tern's stated purpose is
to discover and route across every profile, container, and PWA on a
machine; the machine where it is actually running proves that once
discovery does its job well, the picker and settings pages it hands that
list to are the parts that break. The fix is the same UI investment
either way (section headers, search, collapse), so this is one theme, not
two separate findings from two reviewers.

**Tern fails silently in several distinct places.** Design blocking #1: a
launch that fails (missing browser, broken custom command) shows nothing
at all (`design.md:34`). Eng blocking #2: a crash mid-write can wipe the
whole config file with no warning (`eng.md:44-58`). Eng should-fix: a rule
or remembered mapping pointing at a target that no longer exists is
dropped with no signal back to the user (`eng.md:133-144`). Design
should-fix #6 and #7: a rejected custom-app command and a blocked URL both
land the user on a UI with no explanation of what happened
(`design.md:54,56`). Five separate line items across two reviews, but one
underlying product decision undecided: what does Tern owe the user when
it cannot do the thing it was asked to do. Right now the answer is
"nothing," in five different code paths, and it should be one deliberate
answer (a toast, a log line, a UI banner - something) applied
consistently, not five point fixes.

## Contradictions

**1. Publish the PKGBUILD to AUR now, or fix it first?** The CEO review's
top next-move is: publish the existing `PKGBUILD` to AUR this week,
because it already exists, AUR does not gate on license freedom, and it
is the cheapest way to get real users this week (`ceo.md:56`). The eng
review and this director's own read of `packaging/PKGBUILD` found the
artifact is not actually publishable as-is: `depends=` is missing
`kcolorscheme` and `kcrash` (same root cause as the README gap,
`eng.md:60-75`), and separately, `source=("$pkgname-$pkgver.tar.gz")` at
`packaging/PKGBUILD:16` is a bare local tarball name with
`sha256sums=('SKIP')` at line 17 - AUR requires a real fetchable source
(a GitHub release tag tarball URL) and a real checksum, and the CEO
review does not flag either gap. **Adjudication: the CEO's move is
correct, the artifact is not ready, and the gap between "correct move"
and "ready artifact" is about an hour of work** - add the two packages to
`depends=`, point `source=` at
`https://github.com/bitskc/tern/archive/refs/tags/v$pkgver.tar.gz`, and
generate a real `sha256sum` against that tarball. Do the AUR publish this
week as the CEO recommends, after that hour, not instead of it.

**2. Should Tern ship its own container-bridge WebExtension?**
`DESIGN.md:7` states a deliberate decision: "Tern never registers or
handles `ext+container` itself... that scheme is only ever argv to the
browser." The CEO review's 10-star recommendation is the opposite: ship
and maintain Tern's own small WebExtension so container routing reaches
Mozilla Multi-Account Containers' full 409K user base instead of the
roughly 0.5% of them who also happen to have the third-party "Open URL in
Container" extension (1.9K users) installed (`ceo.md:18,46-48`). This is
not a reviewer misreading the design - the CEO review brought new
evidence (the 409K vs. 1.9K adoption gap) that was not part of the
original design decision, so it is a genuine re-opening of a considered
choice, not a mistake to correct. **Adjudication: this is Andy's call, not
a finding to action automatically.** The real cost the CEO review does not
price in: shipping a browser extension means an AMO review pipeline, a
second release train independent of Tern's own CMake/CI/AUR cycle, and an
ongoing support surface in JavaScript and the WebExtensions API - a
language and ecosystem the rest of Tern (Qt6/C++/QML) does not otherwise
touch. That cost is real and ongoing, not a one-time build. Recommendation:
do not commit to this until move 2 in "Next three moves" below has
produced actual container-routing demand signal from outside users; right
now the 409K number is proof the upstream category is large, not proof
that Tern's specific users want Tern to be the one maintaining a browser
extension for it.

## Fix list

Ordered by user impact, not by which reviewer raised it loudest. AI-assisted
effort estimates assume an agent does the mechanical work and Andy reviews.

| Rank | Finding | Source review | File to change | Effort |
|---|---|---|---|---|
| 1 | README `pacman` line and `PKGBUILD` `depends=` both omit `kcolorscheme` and `kcrash`, so a documented clean build fails at `cmake` configure | eng (blocking #3), devex (fix #1) | `README.md:158-160`, `packaging/PKGBUILD:8-14` | 5 min |
| 2 | Hand-edited/loaded custom targets in `config.json` skip the interpreter blocklist entirely - `targetFromJson` validates nothing and `launchTarget` never calls `isBlockedInterpreter` | eng (blocking #1) | `src/core/config.cpp:105-124`, `src/core/launcher.cpp:134-151` | 1-2 hrs |
| 3 | `saveConfig()` writes `WriteOnly \| Truncate` directly to the live path with no temp file, no rename, no fsync; a bad-timed crash wipes rules/remembered/custom targets with no warning | eng (blocking #2) | `src/core/config.cpp:271-276` | 1-2 hrs |
| 4 | `Picker.qml`'s `Repeater { model: 9 }` binds a working "9" shortcut to an index the UI never renders, since `maxRows` is 8 - pressing 9 silently picks whatever target sits at index 8 | director finding, echoed by devex doc fix #7 | `src/qml/Picker.qml:22,52-58` | 15-30 min |
| 5 | `launchTarget()` returning false (missing browser, broken custom command) produces no user-visible feedback anywhere in the call chain | design (blocking #1) | `src/core/launcher.cpp:134-151`, `src/app/Controller.cpp` (launch path) | 1-2 hrs |
| 6 | `PKGBUILD`'s `source=` is a bare local tarball name with `sha256sums=('SKIP')`; not fetchable by `makepkg`/AUR as shipped | director finding (not caught by eng or CEO specifically as a publish-blocker) | `packaging/PKGBUILD:16-17` | ~1 hr |
| 7 | `holdAutoOpen` defaults to true, so every remembered/PWA/default open shows a 1.6s hold bar, contradicting `DESIGN.md`'s "either nothing visible" promise | design ("the one thing") | `src/core/types.h` | 5 min code change; needs Andy's call on the new default |
| 8 | Conflicting rules under `pickerPolicy: never` fall through silently instead of honoring "first match," contradicting `DESIGN.md:34`'s documented decision order | eng (should-fix) | `src/core/router.cpp:136-157` | ~1 hr |
| 9 | Blocked URLs (`javascript:`, etc.) show a bare picker with no banner explaining why, unlike the launch path which does notify on unsafe opens | design (should-fix #7) | routing/`applyDecision` path around `reason: "blocked"` | 1-2 hrs |
| 10 | Rules and remembered mappings pointing at a deleted/uninstalled target are dropped with no signal; `Controller::displayNameFor` falls back to a raw internal id as the only hint | eng (should-fix), design (should-fix #4, related) | `src/core/router.cpp:59-72,159-168`, `src/app/Controller.cpp:356-362` | half day (a "broken rule" indicator) |
| 11 | Rejected custom-app commands in Settings log a `qWarning` and give no inline UI error | design (should-fix #6) | `src/qml/TargetsPage.qml` (Add custom app flow) | ~1 hr |
| 12 | `TargetsPage.qml` does not scale past roughly ten targets; five flat `ListView`s at 48px/row make forty-four targets an unusable scroll | design (should-fix #5), consensus item | `src/qml/TargetsPage.qml` | 1-2 days (search field, collapsible sections) |
| 13 | Picker has no section headers or pinning; forty-plus targets are only reachable via filter, and sixteen same-pattern containers are name-only to tell apart | design (should-fix #1), consensus item | `src/qml/Picker.qml`, `PickerModel`, `rankForPicker()` | ~1 day |
| 14 | No accessibility tree on picker/hold overlays - zero `Accessible` properties, so screen readers get an unnamed fullscreen window | design (blocking #3) | `src/qml/Picker.qml`, `src/qml/Hold.qml` | ~1 day |
| 15 | Documented CLI/config surface has real gaps (`--version`, `--settings`, `-p`, `--rediscover`, `--configure` undocumented in places; `kind` field written but ignored on load) | devex (fixes #2, #4) | `README.md`, `AGENTS.md`, `docs/config.schema.json` | 30-45 min |
| 16 | `docs/RELEASING.md` step 5 never reconfigures after the version bump, so `tern --version` can ship stale until someone re-runs `cmake` | devex (fix #3) | `docs/RELEASING.md` | 10 min |
| 17 | Unshorten resolves only one redirect hop, so a chained shortener leaves rules/picker looking at the second-hop shortener host instead of the real destination | eng (should-fix) | `src/core/pipeline.cpp:16`, `src/core/unshorten.cpp:16-21` | ~1 hr (loop with a hop cap) |
| 18 | No CONTRIBUTING section for "adding a browser family"; a new contributor has to reverse-engineer `discovery.cpp`'s fingerprint order | devex (fix #5) | new `CONTRIBUTING.md` section, points at `src/core/discovery.cpp` | 1-2 hrs |

## Deliberately not doing

Places to push back on the reviews rather than action them as-is:

- **Splitting Containers into its own top-level Settings nav item**
  (design should-fix #5's secondary suggestion). Decline for now. The nav
  is already sparse at four items (`DESIGN.md:19`), and containers only
  appear for profiles where Tern can detect a protocol-handler extension
  (`DESIGN.md:7`) - most users will see zero or a handful, not sixteen.
  Fix the search/collapse mechanism in `TargetsPage.qml` first (fix list
  #12); only revisit a dedicated nav item if collapse alone does not make
  the Containers section manageable once it's built.
- **A headless QML smoke test in CI** (devex fix #6). Worth having
  eventually, but not urgent right now: the specific Kirigami 6.28
  regression it is meant to catch (`borderColor`/`borderWidth` vs. grouped
  `border.color`/`border.width`) was already found and fixed by hand
  before this review round (`CHANGELOG.md:29-34`). Revisit if a second
  Kirigami-API-drift bug ships before this test exists.
- **A `platformName() != "wayland"` startup guard for non-wlroots
  compositors** (eng should-fix). Real hardening, but `LayerShellQt` is
  already a hard `REQUIRED` build dependency (`CMakeLists.txt:53-54`) and
  the entire target audience is KDE Plasma on Wayland. This is backlog
  work, not something that blocks the fix list above.
- **Relicensing to MIT/GPL right now** (CEO review's Option B). Decline
  until there is a concrete reason to: zero emails have asked to pay and
  zero distro packaging requests have been rejected on license grounds
  yet (`docs/reviews/ceo.md:20`). Relicensing is a one-way door - it
  permanently forecloses the commercial track - and should wait for
  either a real "someone wants to pay" signal or a real "a distro said no
  because of the license" signal, not be done preemptively.
- **"Undo" on the post-open toast for a forgotten Always choice** (design
  should-fix #4). A real usability nit, but `forgetHost` in
  `RulesPage.qml` already gives a one-click path to fix a wrong memory,
  and this is a low-frequency action relative to everything ranked above
  it. Fine to leave for a later pass.

## Next three moves

1. **Land fix-list items 1-5 (roughly one day of AI-assisted work).**
   Why now: these are what stand between "the engineering is good" and
   "safe to hand to a stranger" - a build that fails on a clean box, a
   security check that silently doesn't apply to hand-edited config (which
   matters because `AGENTS.md:19` explicitly invites agents to edit that
   file), a crash that can wipe a user's config with zero warning, and a
   picker shortcut that silently does the wrong thing. Every other move on
   this list assumes these are already fixed.
2. **Fix and publish the `PKGBUILD` to AUR, then post to r/kde or the KDE
   Discourse pointing at Junction issue #9.** Why now: this is the one
   move every validated premise in the CEO review actually supports today
   (`docs/designs/tern-office-hours.md#premises`) - the artifact is an
   hour from ready (fix list #6), the target user already showed up in
   someone else's issue tracker four years ago, and it is the only way to
   turn "zero outside validation" into real data instead of another
   internal review.
3. **Hold the WebExtension and relicensing decisions until move 2
   produces real signal.** Why now: both are large, one-way commitments -
   an AMO review pipeline and a second release train for the extension,
   a foreclosed commercial track for relicensing - being considered
   against premises both reviews mark unvalidated
   (`docs/reviews/ceo.md:20`, contradiction #2 above). Move 2 is what
   converts either from a guess into a decision backed by an actual
   stranger's request.
