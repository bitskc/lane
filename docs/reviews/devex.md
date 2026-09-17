# Developer experience review

Review date: 2026-09-17. Repo: `bitskc/lane` v0.2.0 (tagged and released). Method: read README, CONTRIBUTING, AGENTS.md, schema, CI/release workflows, PKGBUILD, and source for drift; verify round-1 claims against current files; `curl -sI` on GitHub URLs; `gh api` for CI/release run status. No builds, installs, or Lane execution (per review constraints). TTHW timings are carried forward from round 1 (2026-09-12 scratch build) because this round could not re-measure.

**Verdict: Good enough for an Arch/KDE contributor who already has deps; docs still drift on hold-bar defaults and Flatpak discovery, and local `ctest` is a weaker gate than CI.**

## TTHW

| Step | Wall time | Notes |
|------|-----------|-------|
| `git clone` | not measured | One-time |
| README `pacman -S` on a clean Arch/CachyOS box | **~2-5 min** | Round 1 estimate; 20 packages, network-bound |
| `cmake` + `ninja` + `ctest` (deps present) | **~51 s** | Round 1 measured on Ryzen 5 5500U: configure 4.2 s, build 46 s, ctest 0.55 s (11 tests today vs 10 in round 1) |
| `cmake --install` + `update-desktop-database` | not measured | README steps 177-179; required for a working menu entry and for honest `appstreamtest` |

**Cold stranger on Arch/CachyOS (clone + pacman + build + test, deps absent): ~5-10 min.** **Warm repeat build with deps: under a minute.** README build instructions are accurate for CachyOS/Arch: one `pacman` line, Ninja, `$HOME/.local` prefix, and `update-desktop-database` call (`README.md:170-187`).

## Round-1 fix verification

| # | Round-1 finding | Status | Evidence |
|---|-----------------|--------|----------|
| 1 | `lane --rediscover` / `--configure` documented but unregistered (exit 1) | **Fixed** | `src/app/main.cpp:98-106` registers both in `QCommandLineParser` (`settings`+`configure` alias, `rediscover`). `Controller::handleArgs` still handles them at `src/app/Controller.cpp:323-326`. Landed in `fc57e99` per `docs/reviews/retro-main.md:27`. |
| 2 | User-facing URLs pointed at nonexistent `bitskc/lane` | **Fixed** | `git remote` is `bitskc/lane`. `curl -sI` returns 200 for `https://github.com/bitskc/lane` and `/releases`. README (`README.md:25`), CHANGELOG links (`CHANGELOG.md:324-326`), metainfo (`data/app.lane.Lane.metainfo.xml:8-10,33`), PKGBUILD (`packaging/PKGBUILD:6,16`), and `tests/test_version.cpp:88-101` all use `bitskc/lane`. Old `bitskc/tern` 301-redirects. |
| 3 | `config.schema.json` `holdMs` minimum 0 vs UI clamp 400-5000 | **Fixed** | Schema `minimum: 400` at `docs/config.schema.json:73`. UI `from: 400` / `to: 5000` at `src/qml/pages/PreferencesPage.qml:59-60`. Minor residual drift: `Controller::setHoldMs` clamps 200-10000 at `src/app/Controller.cpp:282`, wider than UI/schema. |
| 4 | CONTRIBUTING.md lacks dep install instructions | **Fixed** | `CONTRIBUTING.md:7-8` sends readers to the README `pacman` line before the cmake block. |
| 5 | `appstreamtest` false green without `cmake --install` | **Never fixed** | README still documents the skip (`README.md:181-182`). CI installs first (`/.github/workflows/ci.yml:32-42`). `docs/RELEASING.md:82-85` and `packaging/PKGBUILD:26-28` run `ctest` without install, so packagers and releasers hit the same gap. |
| 6 | AGENTS.md documents `location: "title"` / `"process"` as if they work | **Fixed (docs)** | `AGENTS.md:103-105` says they are parsed but cannot match on Wayland. `src/app/SourceInfo.cpp:8-10` is still a stub returning `{}`. `src/core/matcher.cpp:37-45` warns once and non-`url` locations never match when caller identity is empty. |

**Regressions from round 1:** none. Items 1-4 and 6 stayed fixed. Item 5 was acknowledged in round 1 and is still open.

## Findings (ranked by severity)

### 1. README still describes the hold bar as default-on (v0.2.0 made it opt-in)

`holdAutoOpen` defaults to `false` in code (`src/core/config.cpp:261`, `src/core/types.h:149`) and schema (`docs/config.schema.json:65-68`). v0.2.0 metainfo calls this out (`data/app.lane.Lane.metainfo.xml:54`). README still tells strangers they will see a hold bar on every remembered/PWA/default open:

- `README.md:47-49` ("shows a short hold bar (about 1.6 seconds)")
- `README.md:92-94` (same, tutorial section)
- `README.md:101-102` ("Turn the hold off" implies it is on by default)

A fresh install opens silently unless the user enables "Pause before opening" in Preferences. Tutorial and "What it does" sections need a lead-in that hold is opt-in.

### 2. `appstreamtest` honesty gap persists outside CI

ECM registers `appstreamtest` when `appstream` is installed. Without `cmake --install` to a real prefix, the test logs "Not installed yet, skipping" and passes. Three contributor-facing paths still skip install:

| Path | Lines | Gap |
|------|-------|-----|
| README build section | `README.md:176-182` | Documents the skip but does not add install to the happy path |
| Release checklist | `docs/RELEASING.md:82-85` | `ctest` only; contradicts CI |
| AUR PKGBUILD `check()` | `packaging/PKGBUILD:26-28` | `ctest` before `package()`'s `cmake --install` |

CI is honest (`/.github/workflows/ci.yml:32-44`). Local contributors and `makepkg` do not get the same gate.

### 3. Flatpak browser discovery is shipped on `main` but invisible in user/contributor docs

`CHANGELOG.md:12-19` (`[Unreleased]`) documents Flatpak profile discovery and launch wrapping. No mention in `README.md`, `CONTRIBUTING.md`, or `AGENTS.md`. A contributor adding a browser family or debugging discovery will not know to test under `~/.var/app/<app-id>/` or to read the Flatpak branches in `src/core/discovery.cpp`. This is the biggest functional doc hole since v0.2.0.

### 4. AGENTS.md JSON sample misstates `holdAutoOpen` default

`AGENTS.md:52` shows `"holdAutoOpen": true` in the sample config. Actual default is `false` (`src/core/config.cpp:261`, `docs/config.schema.json:67`). Agents hand-editing config from the sample will enable hold when they meant defaults.

### 5. `holdMs` clamp triple mismatch (low)

| Layer | Range | Location |
|-------|-------|----------|
| Schema | min 400, no max | `docs/config.schema.json:73` |
| Settings UI | 400-5000 | `src/qml/pages/PreferencesPage.qml:59-60` |
| Controller setter | 200-10000 | `src/app/Controller.cpp:282` |

Hand-edited values between 200-399 or above 5000 load but the UI cannot set them; above 5000 is silently clamped on save through the UI path only.

### 6. CI vs README build type mismatch (low)

CI configures **Debug** (`/.github/workflows/ci.yml:29`). README and RELEASING use **Release** (`README.md:174`, `docs/RELEASING.md:83`). Both work; Release is what packagers and end users expect. Not a blocker, but a green CI run is not the same profile strangers build locally.

### 7. `lane --rediscover` is heavier than the docs imply (low)

`main.cpp:99` describes reloading "in the running daemon." Implementation: `Controller::rediscover()` just calls `reload()` (`src/app/Controller.cpp:467-470`). With no daemon, `lane --rediscover` still constructs the full `Controller` (tray icon, D-Bus service) and enters `app.exec()` (`src/app/main.cpp:142-155`). Works, but it is not a lightweight one-shot CLI like `--list` or `--explain`.

### 8. Contribution scaffolding still minimal (low)

`.github/` contains only `workflows/` (no issue template, PR template, or CODEOWNERS). Fine for a solo project today; strangers get no PR shape beyond `CONTRIBUTING.md:46-48`.

## CI, release, and packaging

**CI (`/.github/workflows/ci.yml`)** runs on `ubuntu-latest` inside `archlinux:latest`, installs the full KDE/Qt dep set (including `appstream`), builds Debug, installs to `build/throwaway-install`, then runs `ctest` with `QT_QPA_PLATFORM=offscreen`. Latest `main` run: success (2026-09-17, `gh api`).

**Release (`/.github/workflows/release.yml`)** triggers on `v*` tags, extracts the matching `CHANGELOG.md` section via awk, and creates a GitHub Release with `softprops/action-gh-release@v2`. v0.2.0 release workflow: success (2026-09-15). `docs/RELEASING.md:126-136` accurately describes this (no binary artifacts; source tarballs attached by GitHub).

**PKGBUILD (`packaging/PKGBUILD`)** looks structurally sound: `pkgver=0.2.0`, tag tarball URL (`:16`), pinned `sha256sums` (`:17`), runtime `depends` match README minus build tools, `build()` uses Release + `/usr` prefix, `package()` runs `cmake --install` and installs LICENSE. `check()` gap noted above. Not published to AUR from this repo (no packaging automation found).

**RELEASING.md** three-file lockstep (`CMakeLists.txt`, `CHANGELOG.md`, metainfo) matches practice. Step 5 should add the same `cmake --install` CI uses before calling `ctest` honest.

## Schema vs code

Top-level config keys: **no drift**. All 22 keys in `src/core/types.h:138-161` are loaded/saved in `src/core/config.cpp:231-379` and present in `docs/config.schema.json`.

`customTargets` `kind` / `browserName`: aligned with AGENTS ignore-on-load note (`docs/config.schema.json:183-197`).

`rules.location` enum includes `title` and `process`; code cannot populate them on Wayland. Schema and AGENTS are honest; UI does not expose these fields.

## Considered and fine

- README `pacman` dependency list matches `CMakeLists.txt:31-58` and CI (`/.github/workflows/ci.yml:21-27`). `kcrash` and `kcolorscheme` present.
- CLI surface in README and AGENTS matches `src/app/main.cpp:90-108` for all documented flags.
- `lane --list`, `--explain`, `--config-path`, `--version` exit before starting the daemon (`src/app/main.cpp:111-140`). Good agent ergonomics.
- GitHub URLs, screenshot raw links, and release page resolve (HTTP 200).
- `docs/RELEASING.md` tag-triggered release flow matches `release.yml`; manual `gh release create` fallback is documented.
- PolyForm license called out honestly in RELEASING AppStream validator section (`docs/RELEASING.md:95-104`).
- CONTRIBUTING browser-family guide (`CONTRIBUTING.md:16-42`) is concrete and points at the right files.
- Test count grew to 11 C++ tests (`tests/CMakeLists.txt:10-20`); CI still passes.
- Repo rename completed; update checker test strings use `bitskc/lane` (`tests/test_version.cpp:88-101`).
- `gstack-full-analysis.md` is stale (still quotes round-1 devex verdicts) but is a synthesis artifact, not contributor-facing onboarding.

## Method

1. `gstack-skill-start --skill plan-devex-review` -> `SESSION_ID: 777869-1789660303-e2984661`, `TEL_START: 1789660303`.
2. Read grounding files and prior `docs/reviews/devex.md`.
3. Verified round-1 claims in source; checked URLs with `curl -sI`; CI/release status via `gh api`.
4. Write-only deliverable; no builds or Lane execution.
