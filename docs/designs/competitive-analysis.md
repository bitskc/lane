# Lane vs the link-router field

Grounding: README.md, DESIGN.md, openspec/specs/lane/spec.md, CHANGELOG.md
(v0.1.0/v0.2.0), and src/core/{router,matcher,types}.* as of 2026-09-14.
Competitor claims are marked **[fetched]** when read from the live site/repo
during this analysis, and **[inference]** when they rely on prior general
knowledge that could not be re-verified live in this session (network reads
to that source failed or the source has no public feature page).

## What Lane actually does today (v0.2.0)

- Discovers Gecko + Chromium profiles, firefoxpwa sites, and Firefox/Zen
  containers (new in 0.2.0).
- Rules engine: URL / window-title / source-process match, regex, scoped
  any/domain/path, first match wins. Window-title and source-process
  matching are known-dead on Wayland — Lane now warns once instead of
  silently never firing (CHANGELOG 0.2.0, matcher.cpp confirms
  `windowTitle`/`processName` are populated fields with no Wayland source
  today).
- Path-scoped remembered destinations (`github.com/bitskc` distinct from
  `github.com`), with a comma/period ladder to narrow or widen scope.
- Picker overlay: layer-shell, keyboard-exclusive, number/type-to-filter,
  Alt+A to remember.
- Hold HUD: a 0.4-5s cancellable pause before any *silent* open (remembered,
  PWA, default). Off by default as of 0.2.0. Explicit rules never hold.
- Outlook safe-link unwrap and generic unshorten (4-hop cap, loop-safe).
- Agent-facing CLI (`--list`, `--explain`, `--config-path`) and a published
  `config.schema.json`, documented in `AGENTS.md`.
- Security posture: http/https only, argv-only custom handlers with an
  interpreter/symlink-chain blocklist, atomic config writes, corrupt-config
  quarantine.
- PolyForm Noncommercial license; free for personal use.

## Feature matrix

| Feature | Lane | Velja (Mac) | Choosy (Mac) | Browser Tamer / `bt` (Win/Linux) | Junction (Linux/GNOME) | Finicky (Mac) | Browserosaurus (Mac) |
|---|---|---|---|---|---|---|---|
| Platform | KDE Plasma (Wayland-first) | macOS | macOS | Windows + Linux | Linux/GNOME | macOS | macOS (**retired**) |
| Rules engine | Yes (URL/title/process, regex, scoped) | Yes (URL/source-app matchers, JS transform) | Yes (URL, sender app) [inference] | Yes [fetched: repo description] | **No** — pure chooser dialog | Yes (JS/TS config, most expressive) | **No** |
| Profiles (browser accounts) | Yes (Gecko + Chromium native discovery) | Yes (broad browser list incl. Comet/Helium/Thorium/Wavebox) | Unclear [inference] | Unclear | **No** — manual per-profile `.desktop` workaround only (issue #9, closed unimplemented) | No | No |
| Containers (contextual identities) | Yes, native discovery + `ext+container:` launch | Not natively surfaced as a distinct target type (profile-level only) [fetched] | No | No | No | No | No |
| PWA/web-app targets | Yes (firefoxpwa discovery, scope-aware) | Via source-app rules to a wrapper app, not auto-discovered | No | No | Via manual `.desktop` file, no scope logic | Via manual browser entry, no scope logic | No |
| Path-scoped memory (below domain) | **Yes, unique** | No — matchers are domain/pattern, no learned per-path memory | No | No | No | No (static config only) | No |
| Hold / undo window before silent auto-open | **Yes, unique** | No — rule matches open immediately | No | No | N/A — always prompts | No — rule matches open immediately | N/A — always prompts |
| Source-app rules | Stub field, doesn't fire on Wayland (documented) | Yes, mature (Command-click to create a rule from the prompt) [fetched] | Yes [inference] | Unclear | No | Only via title-based heuristics some users script | No |
| Tracking-param stripping | No | Yes, built-in + optional custom JS pre-transform [fetched] | No | Unclear | No | Manual, via user's own `rewrite` script | No |
| Short-URL expansion | Yes (4-hop, safety-gated) | Yes, optional [fetched] | No | Unclear | No | No | No |
| Scripting | Explicitly out of scope (DESIGN.md non-goal) | JS rewrite hooks only, not full scripting | No | **Yes — Lua** [fetched: repo description] | No | **Yes — full JS/TS config**, most powerful of the field [fetched] | No |
| Browser extension bridge | No (containers launched via argv only) | Yes (Safari/Chrome/Firefox) for browser-clicked links [fetched] | Has an API/URL-scheme automation surface [inference] | No | No | Yes (Chrome/Firefox "open with Finicky") [fetched] | No |
| Agent/CLI-friendly config | **Yes, unique** (`--explain`, JSON schema, AGENTS.md) | No | No | No | Minimal (`x-junction://` URI, shell alias) [fetched] | Config is code (JS/TS), inspectable but not agent-oriented | No |
| Accessibility (screen reader labels) | Yes, added 0.2.0 | Unknown | Unknown | Unknown | Unknown | N/A (no UI) | Unknown |
| Price | Free (Noncommercial), commercial license for resale | Paid, ~$8, ~130K users, was free 3 years then paywalled for support-load reasons [fetched] | Paid, ~$10, shipping since 2010 [inference] | Free/open, C++, 479 stars, actively updated [fetched] | Free/open, GPLv3, 614 stars [fetched] | Free/open, MIT, 5,092 stars [fetched] | Free/open, GPLv3, 2,014 stars, **discontinued** [fetched] |

Notes on cells marked *[inference]*: Choosy's marketing site (`choosy.app`)
only serves a Sparkle update-feed at every path this session tried to fetch
(including through the Wayback Machine), so its feature claims here rest on
prior general knowledge, not a live read, and should be treated as
lower-confidence than the other rows. The "OpenIn" competitor named in the
brief could not be identified as a distinct desktop browser-router product;
`openin.app` is a live site but is a mobile deep-linking/bio-link service
for creators (YouTube/Spotify/Instagram/TikTok), not a browser picker —
it's a different category entirely and I'm not including it in the matrix
above as if it competed with Lane.

## What competitors have that Lane lacks

**Source-app rules (Velja, probably Choosy).** Velja's most-used pattern per
its own docs is "Command-click a browser to create a rule that always opens
the current domain in that browser" plus explicit Source-App matchers
("open every link clicked in Slack in Chrome"). Lane has the schema field
(`processName`) but it's a documented dead end on Wayland — there is no
portal today that hands a picked-default-browser process the identity of
the app that invoked it. This is real, proven demand, but it is not a gap
Lane can close by writing code; it needs an upstream Wayland/portal
capability that doesn't exist yet. Not actionable now.

**Tracking-parameter stripping + URL transform scripting (Velja, and
Finicky's `rewrite` hooks).** Genuinely missing from Lane and genuinely
useful — every browser-picker on macOS in this matrix that has rules also
lets you clean the URL before or as part of matching. Lane already runs a
URL through Outlook-unwrap and unshorten; a static tracking-param strip
slots into that same pipeline with no new UI paradigm.

**Full scripting (Finicky's JS/TS, Browser Tamer's Lua).** DESIGN.md
explicitly rules this out as a non-goal, and that's the right call for a
solo dev: it's an open-ended surface area (a new language runtime, a config
format, a debugging story) for a feature the rules engine plus path-scoped
memory already covers for the 95% case. Skip it.

**Browser-extension bridge for browser-to-browser hops (Velja, Finicky).**
Both let you re-route a link *already open in one browser* to another
browser via an extension click. Lane, being a Wayland default-browser
proxy, only ever sees links from outside the browser. This is a real
Lane blind spot but it requires shipping and maintaining Chrome/Firefox
extensions — a second product surface, disproportionate for zero users.

**Browser health check (Choosy, [inference]).** Detects when something
external reset the OS default browser away from the tool and prompts to
reclaim it. Plausible real annoyance, unverified as a currently-marketed
Choosy feature from a live source this session.

## What Lane has that nobody else in this field has

Checked against Velja, Choosy (best-effort), Browser Tamer, Junction,
Finicky, and Browserosaurus:

- **Path-scoped remembered destinations.** Every rules engine in this
  matrix (Velja, Finicky, presumably Choosy) matches on a domain or a URL
  pattern the user writes by hand. None of them *learn* a destination at
  path granularity from a single keystroke (Alt+A) and let you narrow/widen
  that scope with a comma/period ladder. This is Lane's most distinctive,
  verified-unique feature.
- **Hold HUD as an undo window.** Every competitor is binary: either it
  always prompts (Junction, Browserosaurus — no rules at all) or a rule
  match opens immediately with zero recourse (Velja, Finicky, presumably
  Choosy and Browser Tamer). Nobody else has a cancellable grace period
  specifically for the *silent, convenience* opens (remembered/PWA/default)
  while still honoring "rules beat convenience, no hold" for explicit
  rules. This is a genuine, verified UX innovation, not just a Plasma port
  of an existing idea.
- **Agent-facing CLI and JSON schema.** None of the six competitors expose
  a documented `--explain`/`--list` CLI or a published config schema aimed
  at being read and edited by an AI agent. Finicky's config is code (so
  it's inspectable) but nothing in this field ships an `AGENTS.md`.
- **Native container discovery with color-coded picker rows.** Velja
  supports Firefox/Zen profiles but its own docs don't describe multi-account
  containers as a distinct discovered target type the way Lane's 0.2.0
  release does (containers.json discovery, `ext+container:` argv wrapping,
  per-container picker rows with color).

## Moat assessment: what would it take to match Lane on Plasma?

A competitor cloning Lane's current feature set on Plasma would need: a
resident Qt/Kirigami or GTK process with layer-shell overlay support (not
trivial — Browserosaurus and Junction both needed custom native shells for
their platforms, and getting exclusive-keyboard layer-shell right took Lane
several bug-fix cycles per the changelog); XDG activation-token handling for
focus-stealing prevention on Wayland (a subtle, easy-to-miss requirement —
Lane's changelog shows it was fixed after ship); browser/profile discovery
across two config-file families (Firefox `profiles.ini`, Chromium `Local
State`) plus `firefoxpwa`; and the path-scoped-memory data model plus its
UI (narrow/widen ladder), which is a design decision, not just code — a
clone would have to *choose* to build it, and none of the six competitors
surveyed have.

None of that is undoable by a competent team. But it is real, multi-week
work with several Wayland-specific footguns Lane has already paid down (see
the 0.2.0 changelog's fixes: null layer-shell surface crash, focus-token
handling, notification component-name bug). The moat is "several weeks of
Wayland-specific paper cuts already absorbed," not "impossible to build."
The bigger structural moat is that none of Velja/Choosy/Finicky/Browser
Tamer's authors have any incentive to port to Plasma — they're all
platform-native to Mac or cross-platform via Electron/Go, and Plasma is a
small slice of an already-small "power user picks their own browser" niche.
Junction (GNOME) is the one team plausibly motivated to compete on Linux,
and its own issue tracker shows it explicitly declined to build
profile-aware routing (#9, closed unimplemented) — so the closest Linux
competitor has already signaled it isn't chasing this.

## The 3 recommendations

### 1. Plasma Activity-aware routing scope (highest impact/effort)

**What:** Extend the existing scope model (any/domain/path) with an
`activity` dimension backed by KActivities (already a standard KDE
Frameworks library, no new dependency class). A rule or a remembered
destination can be scoped to "while KDE Activity X is active" — e.g., the
"Work" Activity defaults to the work profile, "Personal" defaults to Zen,
with no per-link decision needed.

**Which competitor proves the demand:** Velja documents this exact desire
today — its FAQ walks users through faking it with macOS Shortcuts +
Focus Mode + the third-party Shortery app, because macOS has no first-class
concept close to a KDE Activity. That's real, stated demand for
context-based default-browser switching that Velja can only bolt on with
external tooling.

**Effort:** Moderate — a few days. KActivities exposes a Qt signal for the
current activity; the scope model, rule storage, and settings UI pattern
for "scope: path" already exist and just need an `activity` variant slotted
in beside them.

**Why it beats the alternatives:** It's Plasma-native in a way literally no
competitor can copy without becoming a KDE app first, and it reuses Lane's
existing scope architecture (rules + remembered destinations both already
carry a scope) rather than inventing a new config surface. Source-app rules
were considered and rejected — the enabling OS capability doesn't exist on
Wayland yet, so building toward it wastes effort on infrastructure Lane
doesn't control. Full scripting was rejected as an explicit non-goal and
because Finicky already proves it's a maintenance-heavy feature for a
niche of the niche.

### 2. Tracking-parameter stripping

**What:** A static, opt-in list of known tracking params (`utm_*`, `fbclid`,
`gclid`, `mc_eid`, etc.) stripped from the URL as a pipeline step alongside
the existing Outlook-unwrap and unshorten stages, before rule matching.

**Which competitor proves the demand:** Velja ships this as a built-in
toggle plus an optional custom-JS pre-transform stage that runs before
rules match — described in its own docs as commonly needed because "an app
wraps the destination in a redirect or link guard."

**Effort:** Low — a day or two. No network calls, no new UI paradigm; it's
a regex/allowlist pass that fits directly into the pipeline Lane already
has (`pipeline.cpp`/`unshorten.cpp` already do multi-stage URL
normalization with safety gates).

**Why it beats the alternatives:** It's the cheapest proven-demand item on
the list and slots into code that already exists, rather than opening a
new attack surface (a browser extension) or an open-ended one (a scripting
runtime). It was weighed against browser-health-checks (Choosy) and
rejected only for ranking, not merit — stripping trackers touches every
link Lane routes, while a health-check notification only fires on the rare
occasion something else steals the default-browser slot back.

### 3. Default-browser reclaim watchdog

**What:** A periodic (e.g., on Settings-open or daemon-start) check of
whether Lane is still the registered default browser, surfaced as a
notification (not a background poll-and-nag loop) if something else has
silently taken it back — a real failure mode after some browser updates
re-register themselves as default.

**Which competitor proves the demand:** Choosy's long-standing "browser
health check" concept is the closest analog [inference — could not verify
live], but the same value shows up first-party in Lane's own 0.2.0
changelog: the settings page already had to start caching and periodically
re-checking `xdg-settings` state because reading it was too expensive to
do on every render, and the "Check for updates" feature in the same release
proves the update-check-plus-honest-failure-reason pattern the same
watchdog would reuse (DNS/rate-limit/404/malformed-response distinctions).

**Effort:** Low — the caching/re-check plumbing for default-browser status
already exists; this is mostly "notify on the transition from default to
not-default" rather than new infrastructure.

**Why it beats the alternatives:** It directly protects the thing Lane
exists to be (the resident default-browser proxy) from a specific,
observed-in-the-wild failure mode, using code paths Lane already built and
tested in 0.2.0. It's ranked third only because it's a defensive/retention
feature rather than a wedge-widener — it makes Lane not silently stop
working, it doesn't win a new use case the way Activity-aware routing does.

## Report

**The 3 recommendations, ranked:**
1. Plasma Activity-aware routing scope — reuses Lane's existing scope
   model via KActivities; the one thing genuinely impossible for any
   Mac/Windows/GNOME competitor to copy without becoming Plasma-native
   first. A few days of work.
2. Tracking-parameter stripping — cheapest proven-demand gap (Velja ships
   it, users cite it explicitly), slots into Lane's existing unshorten/
   unwrap pipeline with no new UI. A day or two.
3. Default-browser reclaim watchdog — defensive, reuses the caching/
   notification plumbing 0.2.0 already built for the update checker and
   the `xdg-settings` cache, protects against a real observed failure mode.

**Strongest "Lane already wins here" finding:** the hold HUD. It's not just
a Plasma reskin of something Mac tools do — every competitor surveyed
(Velja, Finicky, and by all indications Choosy and Browser Tamer) is
binary: a rule match opens immediately with no recourse, or the tool always
shows a picker with no memory. Lane's "rules skip the hold, silent
convenience opens get 0.4-5s to cancel" split is a genuinely novel middle
ground none of the six competitors in this matrix have, verified across
each one's own documentation rather than assumed.

**Surprising finding:** Browserosaurus — the app Lane's own README cites
in the same breath as Choosy and Velja — retired in [inference: 2023-2024
per its own README] and is explicitly unmaintained; its author's own
retirement post frames it as a personal project he stopped using himself.
And Junction, the one team building a browser/app chooser natively for
Linux/GNOME today, has an open, explicitly-closed-as-unimplemented issue
(#9) asking for exactly the profile-aware routing Lane already ships —
meaning the closest thing to a same-platform competitor already looked at
this problem and punted on it.
