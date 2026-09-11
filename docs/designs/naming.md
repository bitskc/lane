# Naming collision research

Research date: 2026-09-11. Product: KDE Plasma default-browser proxy (currently "Tern"). Each candidate checked against AUR pkgname, Flathub search API, npm/PyPI/crates.io exact-name packages, DNS for `NAME.app`, and known software collisions.

## Summary

### Clean

Only **lane** passes all three gates: AUR pkgname free (404), Flathub free (0 hits), no meaningful product collision in this category or on Linux desktops.

**sluice**, **ferry**, and **divert** are AUR/Flathub free but carry registry or technical-term baggage (see table). They are viable only if you accept that risk.

### Poisoned

| Name | Why |
|------|-----|
| **turn** | TURN relay protocol (RFC 8656); `coturn` in Arch repos; Rust `turn` crate (~6.7M downloads) is a TURN implementation |
| **steer** | npm/GitHub project explicitly controls Chrome; crates.io `steer` is a browser-coding-agent CLI |
| **tack** | AUR pkgname taken by the ncurses **tack** terminfo utility (invisible-island.net); would collide with `/usr/bin/tack` on Arch |
| **doorman** | **doorman-dev/doorman** (226 GitHub stars): enterprise AI/API gateway |
| **cairn** | AUR pkgname taken by a GNOME task manager; **Cairn RPG** (cairnrpg.com) is a well-known tabletop game |
| **shunt** | **rowanseymour/shunt**: macOS toolbar browser router, same product category |
| **switchyard** | AUR pkgname taken (SMTP-to-XMPP bridge); **jsommers/switchyard** (42 stars) is a Python networking teaching framework |

### Top three (product fit, among non-poisoned names)

1. **lane** — Only clean candidate; "pick the right lane" maps directly to per-profile routing; short, easy to spell, reads well as `app.lane.Lane`.
2. **sluice** — Water-control gate metaphor fits flow routing; AUR/Flathub free, but the Rust `sluice` crate has ~16.5M downloads and will dominate search for developers.
3. **ferry** — "Carry the link across" is intuitive; AUR/Flathub free, but **jhorey/ferry** (253 GitHub stars) owns "ferry" in the big-data/Docker niche.

## Candidates

| Name | Existing software | AUR | Flathub | npm / PyPI / crates | `NAME.app` | Verdict |
|------|-------------------|-----|---------|---------------------|------------|---------|
| usher | **yatisht/usher** (140*, genomics tree placement); **joshbuddy/usher** (122*, Ruby router); usherlabs referral platform; npm is an AWS Simple Workflow DSL | free | free | all three taken; npm ~76 dl/mo, real packages | registered (HTTP 200) | RISKY |
| cairn | AUR: GNOME task manager (tanji/cairn); **Cairn RPG** (cairnrpg.com); crates `cairn` is experimental Rust VCS | **taken** | free | all three taken | registered (HTTP 200) | POISONED |
| shunt | **rowanseymour/shunt**: macOS browser router (same category); PyPI routes LLM requests between models | free | free | all three taken; PyPI is active routing tool | registered (HTTP 200) | POISONED |
| lane | No Linux desktop app or browser router found; npm `lane` is an abandoned React router (last publish 2022); crates `lane` is a proxy/mirror CLI (~2.7k downloads) | free | free | npm + crates; PyPI free | registered (HTTP 200) | CLEAN |
| tack | **tack** ncurses terminfo verifier (invisible-island.net); PyPI says "Use tackdb instead" | **taken** | free | all three taken | registered (HTTP 403) | POISONED |
| ferry | **jhorey/ferry** (253*, Docker big-data orchestration on AWS/OpenStack) | free | free | all three taken | registered (HTTP 200) | RISKY |
| sluice | crates `sluice` ring-buffer crate (~16.5M downloads, widely depended on); PyPI ZFS snapshot tool (~300 dl/wk) | free | free | all three taken | no DNS / unreachable (likely unregistered) | RISKY |
| doorman | **doorman-dev/doorman** (226*, enterprise AI/API gateway) | free | free | all three taken; npm ~106 dl/mo | registered (DNS points to GitHub Pages; HTTP 404) | POISONED |
| turn | **TURN** protocol (RFC 8656); `coturn` in Arch official repos; crates `turn` is pure-Rust TURN (~6.7M downloads) | free (pkgname; `coturn` is separate) | free | all three taken | registered (HTTP 200) | POISONED |
| switchyard | AUR: SMTP-to-XMPP bridge; **jsommers/switchyard** (42*, Python networked-systems framework, used in university networking courses); PyPI same project | **taken** | free | all three taken; PyPI is the networking framework | no DNS / unreachable (likely unregistered) | POISONED |
| divert | npm/PyPI/crates packages exist (async flow, Recast bindings); **pf divert-to** / FreeBSD `divert(4)` socket is an established networking term | free | free | all three taken | registered (HTTP 200) | RISKY |
| steer | **AndreasMadsen/steer** (14*, "control your chrome (the browser)"); crates `steer` is CLI for "Steer coding agent"; npm same Chrome-control description | free | free | all three taken | registered (HTTP 403) | POISONED |

## Notes

### Method

- AUR: `https://aur.archlinux.org/packages/NAME` (404 = pkgname free).
- Flathub: `https://flathub.org/api/v2/search?q=NAME` (0 hits for every candidate).
- Registries: exact-name lookup on npm, PyPI, crates.io APIs.
- Domains: `dig NAME.app` plus HTTP fetch; "unregistered" only where both DNS and HTTP failed.

### Technical-term collisions (confirmed or corrected)

- **turn**: Confirmed. Not a `/usr/bin/turn` on typical Linux (coturn installs `turnserver`), but "TURN" is unambiguous in VoIP/WebRTC and Rust networking.
- **switchyard**: Confirmed. jsommers/switchyard is a networking-course framework; separate AUR package is an SMTP/XMPP adapter. Not a shell command.
- **tack**: Confirmed. Long-standing ncurses companion binary; AUR pkgname `tack` would fight user expectations on Arch.
- **divert**: `divert-to` appears in pf/ipfilter docs; no common Linux `/usr/bin/divert`, but network admins will recognize the word.
- **shunt**: Electrical/rail metaphor fits routing, but the macOS browser-router repo makes this name unusable despite 1 GitHub star.
- **steer**: Worse than generic "steering": existing tools literally control Chrome and a coding agent named Steer.

### Domain notes

- **switchyard.app** and **sluice.app** had no DNS A/AAAA records at research time (HTTP connection failed). May be purchasable; not verified via WHOIS (tool unavailable).
- Most other `.app` domains resolve and serve content; registering any of them would require buying from the current holder or picking a different TLD.

### Registry squatting

Every candidate except **lane** has a PyPI package; every candidate has npm and crates.io packages. For a C++/Qt daemon this is mostly SEO noise unless you plan to publish libraries. The exceptions that matter:

- **sluice** and **turn** Rust crates have millions of downloads.
- **steer** npm/crates descriptions collide with browser automation.
- **shunt** PyPI package is an LLM request router (routing metaphor, active project).

### Names not in the candidate list

No additions recommended. The candidate list already covers the routing metaphor space; the cleanest options are either taken (shunt, switchyard) or poisoned by exact-category or protocol collisions.

## Usher check and alternates

Follow-up research date: 2026-09-11. Focus: quantify the Usher Raymond collision for a Linux/KDE default-browser proxy, and re-check four same-metaphor alternates.

### Q1: Musician collision cost

#### Google first-page dominance (2026-09-11, google.com, first ~8 results)

| Query | Verdict | Top results (quoted) |
|-------|---------|----------------------|
| `usher linux` | **Software** | "usher - bioconda"; "Installation — usher_wiki 0.0.4 documentation - UShER Wiki"; "nexustar/usher"; "Usher: An Extensible Framework for Managing Clusters of ..." |
| `usher app` | **Mixed** | "Usher" (musician site); "Ushers - App Store - Apple"; "Download Usher for Mac \| MacUpdate"; "Usher - Apps on Google Play" (event/venue apps) |
| `usher kde` | **Mixed (musician + KDE noise)** | "Usher (musician)"; "Usher (@usher) • Instagram"; "Will anybody be trying the KDE distro ..."; "KDE Personnel Services MOA" (Kentucky Dept. of Education, not Plasma) |
| `usher browser` | **Software** | "UCSC UShER: Upload"; "yatisht/usher: Ultrafast Sample Placement on Existing Trees"; "UShER — usher_wiki 0.0.4 documentation"; "Releasing Usher Core Technology as Open Source" |
| `usher github` | **Software** | "yatisht/usher"; "rohitsinghlab/usher"; "findmypast/usher: A command line app ..."; "usher - bioconda" |
| `usher linux app` | **Mixed (software + Mac)** | "Download Usher for Mac \| MacUpdate"; "findmypast/usher: A command line app ..."; "How to usher in the year of the Linux desktop? : r/System76" (verb, not product name) |
| `usher default browser` | **Irrelevant / mixed** | "UCSC UShER: Upload"; "Make Chrome your default browser - Computer"; "What default browser was chosen for your Organization? ..."; "Watch The Fall of the House of Usher \| Netflix" |

Bare `usher` (no qualifier) is **musician-dominated**: first hit is Wikipedia "Usher (musician)", followed by Instagram @usher, usher.com, and Usher Raymond IV.

#### Qualifier rescue

Users hunting a Linux/KDE tool add context. Results shift away from the musician:

- **`usher linux app`**: genomics/bioconda UShER, findmypast/usher CLI, and a Reddit post using "usher" as a verb. Musician appears only indirectly (generic "Usher" links), not as the dominant interpretation.
- **`usher default browser`**: no browser-router product; top hits are genomics UShER upload, generic default-browser help articles, and unrelated fiction streaming. The musician does not own this query.

**Practical cost:** low for in-category discovery (`linux`, `browser`, `github`, `kde` + routing context). The musician mainly taxes bare-word brand search and app-store-style queries (`usher app`).

#### Precedent: software sharing a famous musician/band name

Verified projects that appear unharmed despite the homonym:

| Software | Collision | Evidence it is real and used |
|----------|-----------|------------------------------|
| **CakePHP** | Band **Cake** | Official framework repo `cakephp/cakephp` (8.8k GitHub stars); widely deployed PHP MVC stack since 2005 |
| **Phoenix Framework** | Band **Phoenix** | Official repo `phoenixframework/phoenix` (23k stars); default Elixir web framework |
| **Nirvana** (Caicloud) | Band **Nirvana** | `caicloud/nirvana` (518 stars): "Golang Restful API Framework for Productivity" |
| **PrinceXML** | Musician **Prince** | `princexml.com` serves a commercial CSS-to-PDF engine (HTTP 200); long-running document tool, distinct category |

Also noted: **Genesis** (`Genesis-Embodied-AI/genesis-world`, 30k stars robotics sim) vs band Genesis — same pattern, not counted above to keep the list tight.

#### Trademark (USPTO TESS, wordmark "USHER", 2026-09-11)

Searched the USPTO public trademark search (`tmsearch.uspto.gov`). Findings in **International Class 009** (computer/software goods):

- **Live, musician-linked:** Serial **78735747**, mark **USHER**, owner **Fast Pace Holdings, LLC** (Usher Raymond's company), Class 009: "series of musical sound recordings, and musical video recordings". Status: LIVE / REGISTERED.
- **Live, unrelated:** Serial **86328395**, mark **USHER**, owner **Strategy Inc**, Class 009/042: "computer programs, downloadable computer programs and mobile ...".
- **Dead software marks:** e.g. serial 75720755 "DOWNLOAD USHER" (computer software, NetMessiah, abandoned); serial 75762913 "USHER" (computer software for live video, Intervu, abandoned).

No live registration was found whose goods description is a **desktop browser router** or **KDE system utility**. The musician's live Class 009 mark covers **recorded music/video goods**, not general application software — but it is the same Nice class. **Risk for this rename: unknown-to-low for trademark conflict; not a blocker on the evidence available, but not zero in Class 009.**

#### Musician collision verdict

**Quantified cost: low-to-moderate, not poisoned.** The musician owns bare `usher` and pollutes `usher app`, but target users searching with Linux/browser/github qualifiers land on genomics UShER, bioconda, or unrelated software — not Usher Raymond. Precedent says the homonym has not prevented other software from thriving when the product category differs. Residual costs: generic SEO, possible App Store noise, and a live Class 009 recording mark under Fast Pace Holdings.

---

### Q2: Same-metaphor alternates

Metaphor preserved: "shows each thing to the right place." Checked the same sources as the main table (AUR pkgname URL, Flathub API, npm/PyPI/crates exact name, GitHub notability, technical-term collisions).

| Name | Existing notable software | AUR | Flathub | npm / PyPI / crates | Technical-term collision | Verdict |
|------|---------------------------|-----|---------|---------------------|--------------------------|---------|
| **steward** | AUR: **brooqs/steward** AI personal assistant (pkgname `steward`); npm desktop-change watcher; PyPI data-structure library; crates `steward` task/process manager (~10.6k downloads); **scala-steward** (1.2k stars, dependency bot) | **taken** | free (0 hits) | all three taken | Generic English word; no hard protocol collision | **POISONED** (AUR pkgname taken by another desktop app) |
| **marshal** | npm **marshal**: "Parse Ruby's Marshal strings into JavaScript"; crates `marshal`: Git workspace manager (tiny, 20 total downloads); no dominant GitHub app in this category | free | free (0 hits) | npm + crates; PyPI free | **Confirmed hard collision:** Python stdlib `marshal` module (binary serialization); Ruby **Marshal** format; Go ecosystem **`json.Marshal` / `xml.Marshal`** ubiquity | **POISONED** |
| **herald** | AUR: **vianney/herald** systemd journal-to-XMPP bridge (pkgname `herald`); **etsy/nagios-herald** (320 stars); npm Selenium test helper; PyPI Transmission/uTorrent notifications | **taken** | free (0 hits) | all three taken | "Herald" as notification/announcement pattern in ops tooling | **POISONED** (AUR pkgname taken) |
| **warden** | **wardencommunity/warden** (2.5k stars): "General Rack Authentication Framework"; **warden-protocol/wardenprotocol** (2.2k stars); npm Panopticon metrics wrapper; PyPI Sentry/metrics monitor | free | free (0 hits) | all three taken | Rack/Warden auth is entrenched in Ruby web stacks | **POISONED** |

**Marshal note (confirmed):** the user's suspicion is correct. Even though AUR/Flathub pkgnames are free, `marshal` is overloaded by serialization in Python (`import marshal`), Ruby Marshal, and Go's `Marshal` naming convention. A developer-facing C++/Qt project would fight that noise constantly.

**Alternates summary:** none of the four are **CLEAN**. All are **POISONED** for this product: three by established same-name software (two with AUR pkgname conflicts), one (`marshal`) by serialization terminology.

#### Comparison to **usher**

Usher remains **RISKY** (per main table): registry squatting and genomics UShER dominate developer search, but AUR/Flathub pkgnames are free and the musician collision is **query-dependent**, not absolute. Among names with the usher metaphor, **usher still beats all four alternates** on collision surface — unless the owner wants to avoid the musician entirely, in which case the earlier **lane** candidate (CLEAN in main table) is the safer non-metaphor option.

## Lane deep check

Research date: 2026-09-11. Second pass before a project-wide rename. Goal: try to disprove the earlier **CLEAN** verdict, not confirm it. Methods: `pacman -F` / `pacman -Ss`, AUR RPC + pkgname URL, Flathub `POST /api/v2/search`, npm/PyPI/crates.io APIs, GitHub search API, Bing first-page scrape, targeted web search, USPTO third-party indexes, DNS/HTTP for `lane.app`.

### 1. Linux binary name (`/usr/bin/lane`)

| Check | Result |
|-------|--------|
| `pacman -F '/usr/bin/lane'` | **No matches** in Arch official or CachyOS repos (2026-09-11). |
| `pacman -F lane` | No file named `lane`; only unrelated substring hits (`libliftoff`, `lua*-lanes`, `sqlite-doc`, etc.). |
| `type lane` / shell builtin | **Not found**; not a bash builtin or common coreutils name. |
| `command -v lane` on this system | Empty. |

**Hard blocker status: clear.** No distro package currently ships `/usr/bin/lane`.

**Soft blocker (developer installs):** `CliffHan/lane` is a Rust CLI published on crates.io as the exact crate/binary name `lane` (proxy/mirror manager for curl, git, cargo). Total downloads 2,723; 11 in the last ~90 days (crates.io API, 2026-09-11). A user who runs `cargo install lane` gets that tool, not a browser router. It is tiny and stale (last push 2022-07) but it is a real command named `lane` in the networking-adjacent space.

### 2. AUR

| Check | Result |
|-------|--------|
| `https://aur.archlinux.org/packages/lane` | **HTTP 404** (pkgname free). |
| AUR RPC `search by name: lane` | **0** packages with exact pkgname `lane`; **58** packages whose name merely contains `lane`. |

What an AUR searcher sees (selected, not exhaustive):

| Package | Why it appears |
|---------|----------------|
| `fastlane` | iOS/Android CI tool; uses "lane" as a core user-facing concept |
| `docklane-bin` | Local HTTPS gateway for Docker (Traefik) |
| `aws-lanes` | AWS server "lanes" utility |
| `chromium-extension-dashlane` | Dashlane password manager extension |
| `azurlaneautoscript` | Azur Lane game bot |
| `lua-lanes` / `lua*-lanes` | Lua multithreading library (unrelated) |
| `openlane2` | Open-source chip-design flow (RTL to GDSII) |

None of these block the `lane` pkgname, but `pacman -Ss lane` on Arch is noisy.

### 3. Flathub

`POST https://flathub.org/api/v2/search` with `{"query":"lane"}` returned **1 hit**, a false positive:

- **Newelle** (`io.github.qwersyk.Newelle`) — matched only because the fuzzy index contains the substring "lane" inside unrelated keywords; the app name and ID do not contain `lane`.

**No Flathub app named Lane or shipping a `lane` binary was found.**

### 4. Notable software named Lane

#### GitHub repos with exact name `lane` (traction)

| Repo | Stars | What it is | Confusion risk for a KDE browser proxy |
|------|-------|------------|----------------------------------------|
| **oleiade/lane** | 894 | Go generic Queue/Stack/Deque library (since 2013) | **High for `lane github` search**; zero product overlap |
| **lukeed/lane** | 137 | Copy-on-write git worktrees with persistent memory | Medium for developers; no product overlap |
| **CliffHan/lane** | 0 | Rust CLI: `lane show-proxy`, `lane set-proxy`, etc. | **Medium**: same binary name, proxy/networking adjacency |
| npm `lane` | n/a | Abandoned React page-transition router (last publish 2022-06) | Low; ~30 npm downloads/month |

No GitHub repo was found for a **Linux desktop default-browser router** named `lane`. GitHub code search for `lane default browser`, `lane browser router`, `lane plasma`, and `lane kde` returned **0** relevant hits (2026-09-11).

#### Adjacent products (not exact name `lane`, but relevant)

| Product | Traction | Category | Confusion risk |
|---------|----------|----------|----------------|
| **Lanes Desktop** (`lanes-sh/app`, lanes.sh) | 271 GitHub stars | Tauri desktop app: parallel AI coding agents in "lanes" (worktrees, issue board) | **Medium**: desktop app, "pick your lane" metaphor, growing brand; different function |
| **Lane** (laneapp.co) | Active SaaS | B2B product-intelligence / feedback platform | **High for `lane app` web search**; unrelated category |
| **Lane Technologies Inc.** | USPTO pending | Mobile app suite for real estate / building management | Trademark pending on mark **LANE** (Class 009); see section 7 |
| **synthetixis/lanes** | 8 stars | macOS menu-bar launcher for AI desktop apps (work/personal profiles) | **High category adjacency** (profile routing), but macOS-only and plural name |
| **MemoLanes** | 86 stars | Route/footprint tracking app | Low; "lanes" plural, travel niche |
| **fastlane** (`fastlane/fastlane`) | 42,102 stars | Mobile CI/CD; build steps called "lanes" | **Low** for a KDE end-user desktop app: different audience, different install path (`gem`/`AUR fastlane`), users say "fastlane" not "lane". Developers already using fastlane may hear "lane" and think build pipeline, but they are unlikely to confuse it with a browser handler. |
| **OpenLane** / lane-detection ML repos | 1933+ stars (OpenLane) | Chip design / computer-vision "traffic lane" detection | Dominates `lane` + `linux` GitHub search; no product confusion |

**Browser/routing/CI summary:** No same-category competitor on Linux. The closest functional neighbor is **synthetixis/lanes** (macOS profile launcher). The closest **namespace** neighbors are **oleiade/lane** (GitHub), **Lane** SaaS (laneapp.co), and **Lanes Desktop** (lanes.sh).

### 5. Registries (exact name `lane`)

| Registry | Taken? | Usage signal | Notes |
|----------|--------|--------------|-------|
| **npm** | Yes | **~30 downloads/month** (npm downloads API, 2026-08-12 to 2026-09-10) | Abandoned React router; last modified 2022-06-19 |
| **PyPI** | **No** | HTTP 404 on `pypi.org/pypi/lane/json` | Free |
| **crates.io** | Yes | **2,723 total** downloads; **11** recent; last release 2022-07 | `CliffHan/lane` proxy/mirror CLI |

For a C++/Qt daemon shipped via distro packages, registry noise is mostly SEO. The crates.io collision matters only if users install via `cargo install lane`.

### 6. Search reality

Searches run 2026-09-11. Bing first-page scrape (US-facing) and supplemental web search. This word is extremely generic; expect heavy noise.

#### Bing first page (representative titles; all five queries)

For **`lane linux`**, **`lane app`**, **`lane kde`**, **`lane browser`**, and **`lane github`**, Bing's first six organic results were effectively the same generic set:

1. **Lane Bryant** — `lanebryant.com`
2. **Wikipedia** — "Lane" (surname / common noun disambiguation)
3. **Merriam-Webster** — dictionary entry for "lane"
4. **Cambridge Dictionary** — definition of "lane"
5. **The Free Dictionary** — definition of "lane"
6. **Cambridge Dictionary (US)** — duplicate dictionary entry

No Linux software, KDE project, or browser-router product appeared on page one for any of those five queries in the Bing scrape.

#### Targeted web search (qualifier rescue attempt)

| Query | What actually ranks | Can a user find this project? |
|-------|---------------------|-------------------------------|
| `lane linux` | **LaneLinux** repos (OpenCV lane-departure warning, unrelated); Arch forum posts using "lane" as an English word ("triple-lane Arch setup") | **No** |
| `lane app` | **laneapp.co** (B2B product-intelligence SaaS); software review aggregators | **No**; wrong "Lane" product dominates |
| `lane kde` | **system-monitor-lanes** (KDE Plasma 6 CPU widget with per-core "lanes"); car lane-assist manuals; Plasma Handbook PDF (incidental word "lane" in prose) | **No** |
| `lane browser` | Generic "how to set default browser on Linux" guides (mimeapps.list, `xdg-mime`); no product named Lane | **No** |
| `lane github` | **oleiade/lane** (894-star Go library) is the clear software winner in web/GitHub search | **No** for a browser proxy |

#### Discoverability verdict

A user told **"install lane"** on Arch will find `pacman -Ss lane` noise (fastlane, docklane, lua-lanes, etc.) but not this project until it has an AUR package and README SEO. A user told **"search for lane browser linux"** will not find the project on the first page of general web search today. Discovery requires **direct links** (GitHub URL, AUR pkgname, docs site) or a owned domain with content. The name is **effectively unsearchable** without qualifiers you do not have yet (`lane browser router`, `app.lane.Lane`, etc.).

### 7. Trademark (US)

Searched USPTO indexes via third-party mirrors (uspto.report, furm.com, markinton.com). **Not a lawyer; not exhaustive.**

| Mark | Owner | Class | Status | Goods (summary) | Relevance |
|------|-------|-------|--------|-----------------|-----------|
| **LANE** | Lane Technologies Inc. | 009 | **Live / pending** (serials 90664853, 90527839) | Downloadable app software for real estate, building access, office management | Same Nice class as desktop apps; different field of use |
| **SWIFTLANE** | SwiftLane Inc. | 009 / 042 | **Registered** | Building access / security apps | Contains "lane"; not standalone |
| **LIVELANES** | EBlock Corporation | 009 / 035 / 042 | **Registered** (2026-03) | Vehicle-auction software | Plural; unrelated |
| **LICENSE LANE** | (applicant unnamed in snippet) | 009 / 042 | Pending (2026-02 filing) | Professional license compliance SaaS | Compound mark |

No live **registered** standalone wordmark **LANE** for a **desktop browser handler** or **KDE utility** was found. Risk is **unknown**, not zero: Class 009 is crowded, and Lane Technologies has a **live pending** application for a mobile app mark spelled exactly **LANE**.

### 8. Domain (`lane.app`)

| Check | Result |
|-------|--------|
| DNS | Resolves (A records present, 2026-09-11) |
| `https://lane.app` | HTTP 200; minimal HTML redirecting to `/lander` |
| `https://lane.app/lander` | GoDaddy "for sale" / access-denied interstitial (not project content) |

The domain is **not available** for a new project without purchase. It does not currently host a competing product.

### Judgment

#### Does `lane` have no real competition?

**Product category (Linux KDE default-browser proxy): Yes.** No confusable same-category product owns the name on Linux. No Flathub app, no AUR pkgname collision, no `/usr/bin/lane` in Arch repos, no Choosy/Junction/Velja-class competitor found.

**Namespace / name ownership: No.** Other software already uses this name or a near variant with real traction or intent:

- **oleiade/lane** (894 stars) owns `lane github` search.
- **Lane** at laneapp.co owns `lane app` search.
- **Lanes Desktop** (271 stars) is an active desktop-app brand built on the lane metaphor.
- **CliffHan/lane** occupies the exact `lane` binary name on crates.io (tiny, but real).
- **lane.app** is taken/parked.

If the owner's condition is strictly "no software a user could confuse with this project," the honest answer is **no** for web/app discovery (laneapp.co) and **no** for GitHub discovery (oleiade/lane). If the condition is strictly "no competing browser router," the answer is **yes**.

#### Genericness

**Fails.** This is the larger problem. Bare `lane` plus any common qualifier (`linux`, `app`, `kde`, `browser`, `github`) returns dictionaries, Lane Bryant, unrelated ML/automotive "lane" projects, or wrong software named Lane. The project will not be findable by word of mouth alone until it has strong secondary identifiers (reverse-DNS `app.lane.Lane`, GitHub org, docs URL).

#### fastlane

**Unlikely to confuse** KDE/desktop users installing a browser handler. fastlane's "lane" is an internal build-step noun inside a Ruby tool mobile developers already call **fastlane**. Collision surface is developer jargon, not end-user product identity. Listed for completeness because AUR already ships a `fastlane` package and `pacman -Ss lane` surfaces it.

#### Single biggest risk if the owner proceeds

**Genericness and search invisibility**, not a same-category competitor. A user told "install lane" or who Googles "lane linux" will not find this browser proxy on the first page; they may hit Lane Bryant, laneapp.co, oleiade/lane, or Arch packaging noise. Plan on owning discovery through `app.lane.Lane`, a GitHub org/repo URL, and (if budget allows) buying `lane.app` or using a non-`.app` domain. The technical namespace (AUR pkgname, binary name on Arch) is clean; the human namespace is not.
