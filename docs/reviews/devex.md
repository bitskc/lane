# Developer experience review

Review date: 2026-09-11. Repo: `bitskc/tern` v0.1.0. Method: follow docs literally, scratch build at `/tmp/tern-devex-build` (no `cmake --install`), read source for drift checks.

## TTHW

Measured on an already-cloned tree with Arch/CachyOS deps installed (did not re-run `pacman`; simulates a box where the README dependency step already succeeded).

| Step | Wall time | Notes |
|------|-----------|-------|
| `cmake -S . -B /tmp/tern-devex-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local` | **3.8 s** | 18 CMake QML plugin warnings (Kirigami/LayerShellQt/qqc2 "link target does not exist"); 4x "Could NOT find WrapVulkanHeaders" (harmless). Configure succeeds. |
| `cmake --build /tmp/tern-devex-build` | **37.0 s** | 84 Ninja steps on Ryzen 5 5500U. No compiler warnings in build log. |
| `ctest --test-dir /tmp/tern-devex-build --output-on-failure` | **0.8 s** | 9/9 pass (8 unit tests + ECM `appstreamtest`). |
| **Build + test subtotal (deps present)** | **~42 s** | |

Not measured here: `git clone` (~5 s on this network), README `pacman -S` on a fresh box (typically **2-5 min**), `cmake --install` + `update-desktop-database` (README steps 164-166; skipped per review constraints).

**Dependency list vs `CMakeLists.txt`:** README `pacman` line is missing two packages that `find_package` requires:

| Package | Required by |
|---------|-------------|
| `kcrash` | `KF6::Crash` in `CMakeLists.txt:49`, `KCrash::initialize()` in `src/app/main.cpp:66` |
| `kcolorscheme` | `KF6::ColorScheme` in `CMakeLists.txt:48`, linked in `src/CMakeLists.txt:69` |

Every package listed in README **is** needed. Nothing listed is unused. CI (`.github/workflows/ci.yml:25-26`) already installs `kcrash` and `kcolorscheme`; README does not.

Following README literally on a fresh Arch box **without** those two packages fails at `cmake` configure with missing `KF6Crash` / `KF6ColorScheme`.

## Doc errors

| File | Wrong line (quoted) | Correct replacement |
|------|---------------------|---------------------|
| `README.md` | `sudo pacman -S --needed cmake extra-cmake-modules ninja qt6-base qt6-declarative qt6-svg \` / `  kirigami kirigami-addons ki18n kcoreaddons kconfig kdbusaddons knotifications \` / `  kwindowsystem kiconthemes kstatusnotifieritem layer-shell-qt qqc2-desktop-style` (lines 158-160) | Append `kcrash kcolorscheme` to the `pacman` line (same place CI installs them). |
| `README.md` | `Press 1 through 9 to pick a target by number.` (line 79) | `Press 1 through 8 to pick a target by number.` (`src/qml/Picker.qml:22` sets `maxRows: 8`; keys 1-9 are bound but only eight rows render). |
| `README.md` | CLI block (lines 146-152) lists `--daemon`, `--pick`, `--explain`, `--list`, `--config-path` only | Add `-v, --version`, `--settings`, and note `-p` is a short form of `--pick`. Optionally document `--rediscover` (parsed in `Controller::handleArgs`, `src/app/Controller.cpp:232-233`). |
| `AGENTS.md` | CLI block (lines 186-188) documents only `--config-path`, `--list`, `--explain` | Add the flags `main.cpp` actually registers: `--daemon`, `-p`/`--pick`, `--settings`, `-v`/`--version`. Mention `--rediscover` and the undocumented alias `--configure` (`src/app/Controller.cpp:230`). |
| `AGENTS.md` | `Each has id, name, exec, args, icon, and kind` / `(app, action, browser, or pwa)` (lines 133-135) | `Each has id, name, exec, args, and icon. All custom targets load as kind Custom; the kind field in JSON is written on save but ignored on load (src/core/config.cpp:113).` Or drop `kind` from the doc until the loader honors it. |
| `AGENTS.md` | `The inline shape below covers the same fields for quick reference.` (line 17) | Either add `kind` to `docs/config.schema.json` or soften to "covers top-level fields; see schema for nested shapes." |
| `docs/RELEASING.md` | Step 5 (lines 77-80): `cmake --build build` / `ctest --test-dir build` only | After bumping `project(tern VERSION ...)` in step 2, reconfigure so `tern_version.h` regenerates: `cmake -S . -B build -G Ninja` then build and test. Version comes from configure time (`docs/RELEASING.md:7-8`). |

**Verified non-errors (user suspicions closed):**

- `AGENTS.md` sample JSON **does** include `hiddenTargetIds` (line 49). Matches `Config` in `src/core/types.h:153` and the live `~/.config/tern/config.json`.
- Documented CLI flags `--daemon`, `--pick`, `--explain`, `--list`, `--config-path`, `--settings` all exist in `src/app/main.cpp:93-104`. `--version` comes from `parser.addVersionOption()` (line 92).

## Schema drift

Top-level `Config` fields: **no drift**. All 22 keys in `src/core/types.h:139-161` appear in `docs/config.schema.json` and vice versa.

Nested drift on `customTargets` items:

| Direction | Field | Detail |
|-----------|-------|--------|
| Code writes, schema missing | `kind` | `targetToJson()` sets it (`src/core/config.cpp:101`); schema `customTargets.items.properties` has no `kind`. |
| Code writes, schema missing | `browserName` | `targetToJson()` sets it (`src/core/config.cpp:97`); not in schema. |
| AGENTS documents, loader ignores | `kind` | `targetFromJson()` hardcodes `Kind::Custom` (`src/core/config.cpp:113`); enum values in AGENTS.md are not enforced. |

`rules`, `substitutions`, `targetOrder`, `targetAliases`, and `hiddenTargetIds` match code and AGENTS.

## CI

**What it catches**

- Full configure + build on `archlinux:latest` with the complete dependency set (including `kcrash`, `kcolorscheme`).
- All 8 C++ unit tests under `tests/` plus ECM `appstreamtest` (validates metainfo against install manifest).
- `QT_QPA_PLATFORM=offscreen` avoids needing a real Wayland session.

**What it misses**

- **QML runtime.** No test loads `Picker.qml`, `Hold.qml`, or `Settings.qml`. The Kirigami 6.28 `borderColor`/`borderWidth` regression called out in `CHANGELOG.md` would not fail CI.
- **Install smoke test.** CI never runs `cmake --install`; it relies on build-tree artifacts and `install_manifest.txt`.
- **Release workflow.** `release.yml` is tag-triggered only; not exercised on PRs.
- **README dependency set.** CI installs more packages than README lists; a contributor following README on a clean machine hits configure failure before CI would help them.

**Run history** (`gh run list --repo bitskc/tern`, 2026-09-11):

| Result | Workflow | When |
|--------|----------|------|
| success | CI | 2026-09-11 05:32 UTC (`Use offscreen Qt platform in CI tests`) |
| failure | CI | earlier same day (offscreen fix landing) |
| success | Release | 2026-09-11 05:26 UTC (`v0.1.0` tag) |

Changelog extraction in `release.yml` awk matches Keep a Changelog `## [0.1.0] - YYYY-MM-DD` headers; verified locally against `CHANGELOG.md`.

## Adding a browser family

No CONTRIBUTING section points here. A stranger adding, say, "Arc" would need to reverse-engineer `src/core/discovery.cpp`.

**Trace**

1. **`fingerprint()`** (`src/core/discovery.cpp:169-281`): pattern-match on desktop entry exec/name/id blob. Order matters (e.g. `zen` before `firefox`). Add a new `if (blob.contains(...))` branch returning `Engine::Gecko` or `Engine::Chromium` with brand string and config data directory(ies).
2. **Profile walk** (automatic once fingerprint is right):
   - Gecko: `geckoProfiles()` reads `profiles.ini`, skips junk/missing dirs, emits private windows, calls `geckoContainers()` for container targets.
   - Chromium: `chromiumProfiles()` reads `Local State` profile cache; Brave gets a Tor pseudo-profile.
   - Unknown engine: `genericBrowser()` single default target.
3. **Desktop scan** (`scanDesktopFiles`, lines 62-127): any `.desktop` with `WebBrowser` category or `x-scheme-handler/http` MIME is a candidate. `skipDesktopId()` filters noise.
4. **Dedup** (`discoverTargets`, lines 788-809): groups by exec + brand + dataDir; picks best desktop file via `isBetterDesktopApp()`.
5. **Tests**: add fixture tree under `tests/fixtures/` (desktop entry + profile data), extend `tests/test_discovery.cpp`. No template for a new family.
6. **Docs**: update README "What it does" browser list (line 31-32). Metainfo description (line 22-23) if user-facing.
7. **Launcher**: only if the browser needs nonstandard argv; default placeholders are `$url` / `$urlEncoded` in `src/core/launcher.cpp`.

**What contributors get wrong**

- Wrong config dir path (Gecko vs Chromium layout).
- Inserting fingerprint checks in the wrong order (substring collisions: `firefox` vs `firefoxpwa`).
- Forgetting `prefs.js` must exist or profile is skipped (line 548).
- Expecting containers without a protocol-handler extension in `extensions.json`.
- No mention that **Thorium** is already in `fingerprint()` (line 275) but not documented anywhere.

## Agent experience

Read `~/.config/tern/config.json` (not modified). Shape matches AGENTS.md top-level keys. Settings UI and file agree on field names.

**Workflow test:** copy config to `/tmp/tern-config-copy.json`, add a dummy rule in the copy, reason about reload:

- `tern --list` and `tern --explain URL` spawn a **new process** that reloads config each run (`main.cpp:34-36`, `113-115`). Good for read-only inspection without touching the daemon.
- Live routing uses the resident daemon's in-memory config. AGENTS restart recipe (`pkill -f "tern --daemon"; tern --daemon &`) is correct and necessary for JSON edits to affect default-browser routing.
- Caveat: `pkill -f "tern --daemon"` is broad; on a multi-session machine it kills every matching daemon. Usable for a single-user Plasma box, fragile elsewhere.
- Settings window persists via D-Bus to the running daemon (`Controller::persist()`); agents editing JSON directly bypass that path and must restart.

**Gap:** AGENTS does not say that `--explain`/`--list` reflect on-disk config immediately while the daemon may still run stale rules until restart.

## Release process

`docs/RELEASING.md` is mostly executable by a non-Andy release manager with repo write access and `gh` auth. Gaps:

1. **Reconfigure after version bump** (see Doc errors). Skipping it ships a binary whose `tern --version` still shows the old number until someone re-runs `cmake`.
2. **Assumes `build/` already exists** from a prior configure (step 5 never runs initial `cmake -S . -B build`).
3. **No branch policy.** Steps push `HEAD` and tag; doc does not say "release from `main`" or require a PR.
4. **No post-tag verification.** Nothing says to wait for CI on the release commit or confirm the GitHub Release body matches changelog.
5. **Manual fallback `sed` in step 10** uses a different extractor than `release.yml` awk; fine as emergency docs, but the two parsers behave differently on edge-case headers.
6. **AppStream vs changelog**: three-file lockstep is clear; good.

## Ranked fixes

1. Add `kcrash kcolorscheme` to README `pacman` line so a literal README build succeeds on a fresh Arch box.
2. Document full CLI surface in README and AGENTS (`--version`, `--settings`, `-p`, `--rediscover`, `--configure`).
3. Fix RELEASING step 5 to re-run `cmake` after the version bump.
4. Align `customTargets` schema with what `saveConfig` writes (`kind`, `browserName`) or stop writing/claiming fields the loader ignores.
5. Add a CONTRIBUTING subsection "Adding a browser family" pointing at `fingerprint()` and the fixture pattern in `tests/test_discovery.cpp`.
6. Add a headless QML smoke test (load Picker/Hold modules under `QT_QPA_PLATFORM=offscreen`) so Kirigami API drift fails CI.
7. Change README picker tutorial from "1 through 9" to "1 through 8" to match `Picker.qml`.
