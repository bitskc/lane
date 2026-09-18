# Developer experience review

Review date: 2026-09-18. Repo: `bitskc/lane` @ `1de0d59` (main). Method: read README, CONTRIBUTING, AGENTS.md, schema, CI/release workflows, PKGBUILD, CMakeLists, CHANGELOG; compare config/schema/code; run `cmake --build build` + `QT_QPA_PLATFORM=offscreen ctest --test-dir build` (12/12 pass); verify `appstreamtest` after prefix install. No Lane binary execution.

**Verdict: A cold Arch/CachyOS clone builds and tests cleanly; schema and packaging gates are in good shape. Remaining pain is doc drift on the hold-bar tutorial, zero non-Arch build guidance, and no guard against merge-conflict junk landing in committed files.**

## TTHW

| Step | Wall time | Notes |
|------|-----------|-------|
| `git clone` | not measured | One-time |
| README `pacman -S` on a clean Arch/CachyOS box | ~2-5 min | 20 packages, network-bound (carried from prior round) |
| `cmake` + `ninja` (deps present) | **~47 s** | Measured 2026-09-18 on Ryzen 5 5500U, incremental rebuild |
| `ctest` (12 tests, offscreen) | **~0.8 s** | All pass; includes `appstreamtest` when prefix install exists |
| `cmake --install` + `update-desktop-database` | not measured | README `README.md:190-192`; required for menu entry and honest AppStream gate |

**Cold stranger on Arch/CachyOS (clone + pacman + build + install + test): ~5-10 min.** **Warm repeat build with deps: under a minute.** README build block is accurate for Arch: one `pacman` line, Release build, `$HOME/.local` prefix, install before `ctest`, `update-desktop-database` call (`README.md:183-192`).

**openSUSE/Tumbleweed:** not documented in the repo. Package names (`kirigami-addons6-devel`, rootless podman workarounds, etc.) are tribal knowledge only.

## Round-3 fix verification

| # | Check | Status | Evidence |
|---|-------|--------|----------|
| 1 | README dependency list matches `CMakeLists.txt` `find_package` calls (`kcrash`, `kcolorscheme` present) | **Held** | README `pacman` line: `kcolorscheme kcrash` at `README.md:186`. CMake: `KF6` components `ColorScheme`, `Crash` at `CMakeLists.txt:51-54`. CI installs the same set at `.github/workflows/ci.yml:24-27`. |
| 2 | `docs/config.schema.json` matches `src/core/config.cpp` fields (`recentTargetIds`/`toastMs` gone; `holdMs` 400-5000) | **Held** | Known keys in `config.cpp:231-238` align with schema properties (22 keys, Python diff shows zero drift). No `recentTargetIds` or `toastMs` in schema, `config.cpp`, or `types.h`. `holdMs` schema: `minimum: 400`, `maximum: 5000` at `docs/config.schema.json:65-70`. Controller setter matches: `Controller.cpp:280-284`. UI spinbox `PreferencesPage.qml:59-60`. |
| 3 | `AGENTS.md` JSON example matches schema + `config.cpp` (`hiddenTargetIds` present; stale keys gone) | **Held** | Sample at `AGENTS.md:47-68`: `hiddenTargetIds: []` line 61; `holdAutoOpen: false` line 58; no `recentTargetIds` or `toastMs`. `--list` documents fourth `kind` column at `AGENTS.md:157-158`, `214`. |
| 4 | PKGBUILD: `appstream` in `makedepends`; `check()` installs to throwaway prefix before `ctest`; `depends` complete (`layer-shell-qt`, no `layer-shell-qt6-imports`) | **Held** | `packaging/PKGBUILD:15` (`appstream` in `makedepends`). `check()` at `:27-28` runs `cmake --install build --prefix build/throwaway-install` then `ctest`. `depends` includes `layer-shell-qt` at `:13`; no `layer-shell-qt6-imports`. |
| 5 | CI: Release build type; `ctest` after install (AppStream needs installed metainfo) | **Held** | `.github/workflows/ci.yml:29` (`CMAKE_BUILD_TYPE=Release`). Install step `:32-42` (real `--prefix`, not DESTDIR). Test step `:43-44`. Latest `main` CI run: success (2026-09-18, PR #39 merge). |
| 6 | `CHANGELOG.md`: no conflict-marker junk; `0.1.0` + `0.2.0` intact; footer links correct; `[Unreleased]` matches merged PRs | **Held** | No `<<<`, `>>>`, `=======`, or `conflict://` in `CHANGELOG.md`. `## [0.2.0]` at `:95`, `## [0.1.0]` at `:339`. Footer links at `:392-394` point at `v0.2.0...HEAD`, `v0.1.0...v0.2.0`, `v0.1.0` tag. `[Unreleased]` covers Flatpak discovery, `--list` kind column, launch-race fixes, dead-key removal, hold range, picker order, and wave-3 fixes matching merged PRs #11, #15-19, #31-32, #39 since `v0.2.0`. |

**Regressions from round-2 devex:** none on the six checks above. Round-2 gaps that are now fixed: README includes `cmake --install` before `ctest` (`README.md:190-191`); PKGBUILD `check()` installs before `ctest`; `holdMs` clamp unified; AGENTS sample `holdAutoOpen` is `false`; CI uses Release (round-2 doc incorrectly said Debug).

## Findings (ranked by newcomer impact)

### 1. README tutorial still teaches hold-bar as default-on

"What it does" correctly says the hold bar is optional (`README.md:49-51`). The "Stop a silent open" tutorial does not: it opens with "When Lane opens a link... it shows a hold bar" (`README.md:94-96`) and step 5 says "Turn the hold off" (`README.md:103-104`), implying it is on out of the box. Fresh installs have `holdAutoOpen: false` (`src/core/config.cpp:259`, `src/core/types.h:148`). A stranger following the tutorial will look for a bar that never appears unless they enable "Pause before opening" in Preferences.

### 2. No openSUSE / non-Arch build path

README title is "Build (CachyOS / Arch)" (`README.md:176`). CONTRIBUTING defers entirely to the README `pacman` line (`CONTRIBUTING.md:7-8`). Zero mentions of `zypper`, Tumbleweed package names (`kirigami-addons6-devel`, etc.), podman-based builds, or SELinux bind-mount labels anywhere in tracked docs. Beelink-style builders must reverse-engineer deps from `CMakeLists.txt` and Arch package names.

### 3. No guard against merge-conflict tool output in committed files

Git history shows at least two cleanup commits for CHANGELOG contamination (`bef9c23`, `3fde33f`; round-1/2 reviews cite a third). Active hooks are Git samples only (`.git/hooks/` has no installed `pre-commit`). CI runs build+test only; no step greps for `<<<<`, `>>>>`, `=======`, `conflict://`, or `⚠` conflict markers. The class will recur whenever parallel agents touch `CHANGELOG.md`.

### 4. CONTRIBUTING build block is a weaker gate than README and CI

CONTRIBUTING tells contributors to run `ctest` without `cmake --install` (`CONTRIBUTING.md:11-14`). README and CI install first. After a bare build, `appstreamtest` logs "Not installed yet, skipping" and passes (`README.md:195-197`). First-time contributors following CONTRIBUTING alone get a false-green AppStream check.

### 5. CONTRIBUTING discovery guide omits Flatpak fixture paths

README documents Flatpak browser discovery (`README.md:33-34`). AGENTS.md documents Flatpak profile locations and stale-target behavior (`AGENTS.md:164-169`). CONTRIBUTING's browser-family section (`CONTRIBUTING.md:16-42`) only describes native `profiles.ini` / `Local State` walks. A contributor adding or debugging Flatpak discovery will not know to put fixtures under `~/.var/app/<app-id>/` or to test `flatpak run` arg wrapping in `discovery.cpp`.

### 6. `lane --list` format is documented and matches code (low, verified)

`main.cpp:116-130` prints `id`, display name, exec, and `kindName(t.kind)` as four tab-separated columns. Kinds: `browser`, `container`, `pwa`, `action`, `app` (`src/core/types.h:161-175`). AGENTS.md matches (`AGENTS.md:157-158`, `214`). CHANGELOG `[Unreleased]` notes the column (`CHANGELOG.md:21-22`).

### 7. Build-type split between CONTRIBUTING and everything else (low)

CONTRIBUTING uses `CMAKE_BUILD_TYPE=Debug` (`CONTRIBUTING.md:11`). README, PKGBUILD, CI, and RELEASING use Release. Harmless for day-to-day hacking, but a contributor comparing local behavior to CI is not building the same profile.

### 8. `docs/RELEASING.md` matches the actual release workflow (verified, fine)

Three-file lockstep (`CMakeLists.txt`, `CHANGELOG.md`, metainfo) at `docs/RELEASING.md:25-39`. Step 5 install-then-test at `:82-87` matches CI. Tag push triggers `release.yml`, which awk-extracts the matching changelog section (`docs/RELEASING.md:131-137`, `.github/workflows/release.yml:14-52`). Manual `gh release create` fallback documented at `docs/RELEASING.md:146-149`.

## Considered and fine

- README `pacman` dependency list matches `CMakeLists.txt:31-58` and CI; `kcrash` and `kcolorscheme` present.
- Schema, `config.cpp`, `types.h`, and AGENTS sample: no key drift; dead keys removed.
- PKGBUILD structurally sound: pinned `sha256sums`, license install, offscreen `check()`, complete runtime `depends`.
- CLI surface in README and AGENTS matches `src/app/main.cpp:90-108`.
- `lane --list`, `--explain`, `--config-path`, `--version` exit before daemon startup (`main.cpp:111-140`).
- `DESIGN.md:44-46` correctly documents hold as opt-in (off by default).
- RELEASING install-before-ctest guidance is correct and aligned with CI.
- Test suite: 12 C++ tests (`tests/CMakeLists.txt:10-20`); all pass offscreen on this machine.
- PolyForm license called out honestly in RELEASING AppStream validator section (`docs/RELEASING.md:100-106`).
- CONTRIBUTING browser-family guide (`CONTRIBUTING.md:16-42`) is concrete for native browsers; points at the right files.
- `lane --rediscover` heaviness is documented in README (`README.md:140-144`) and AGENTS.md (`AGENTS.md:28-32`).

## Method

1. `gstack-skill-start --skill plan-devex-review` -> `SESSION_ID: 777869-1789769928-30047468`, `TEL_START: 1789769928`.
2. Read grounding files per review scope.
3. Built and ran `ctest` (12/12 pass); verified `appstreamtest` after prefix install.
4. Write-only deliverable: this file. No commits.
