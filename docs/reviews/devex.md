# Developer experience review

Review date: 2026-09-12. Repo: `bitskc/tern` (product name Lane) v0.1.0. Method: follow `README.md` literally on a scratch build at `/tmp/lane-devex-build` (no `cmake --install`), read source for drift checks, run safe CLI flags only.

## TTHW

Measured on a cloned tree with all README-listed Arch packages already installed (`pacman -Q` checked each; none missing). Did not re-run `pacman -S`.

| Step | Wall time | Notes |
|------|-----------|-------|
| `cmake -S . -B /tmp/lane-devex-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local` | **4.2 s** | Configure succeeds. **25** CMake QML-plugin warnings ("link target does not exist"); **4x** `Could NOT find WrapVulkanHeaders` (harmless on this box). |
| `cmake --build /tmp/lane-devex-build` | **46 s** | 91 Ninja steps, Ryzen 5 5500U. No compiler warnings in build log. |
| `QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/lane-devex-build --output-on-failure` | **0.55 s** | 10/10 reported pass (9 C++ tests + `appstreamtest`). |
| **Build + test subtotal (deps present)** | **~51 s** | |

Not measured here: `git clone`, README `pacman -S` on a fresh box (typically **2-5 min**), `cmake --install` + `update-desktop-database` (README steps 177-179; skipped per review constraints).

**`appstreamtest` on the README path is a false green.** Without `cmake --install`, the test logs `Not installed yet, skipping` and still passes (`LastTest.log` in `/tmp/lane-devex-build`). CI avoids this by installing to `build/throwaway-install` before `ctest` (`.github/workflows/ci.yml:32-42`). A contributor who follows README literally never validates metainfo.

Scratch-built `lane --version` prints `Lane 0.1.0` (matches `CMakeLists.txt:2`).

## Dependency check

| Source | Verdict |
|--------|---------|
| `README.md:170-173` `pacman` line | **Complete.** Includes `kcrash` and `kcolorscheme` (fixed since 2026-09-11 review). Matches `CMakeLists.txt:43-54` (`KF6::Crash`, `KF6::ColorScheme`). |
| `CMakeLists.txt` `find_package` / `ecm_find_qmlmodule` | All satisfied by README list on this machine. Nothing listed in README is unused. |
| `packaging/PKGBUILD:8-14` runtime `depends` | Matches README minus build-only tools (`cmake`, `ninja`, `extra-cmake-modules`). Includes `kcrash` / `kcolorscheme`. |
| Installed on review host (`pacman -Q`) | **20/20** README packages present. |

**Verdict:** README dependency list is accurate today. Last round's missing `kcrash` / `kcolorscheme` finding is **fixed**.

## Doc errors

### Last-round spot-check (2026-09-11 `devex.md`)

| Prior finding | Status |
|---------------|--------|
| README `pacman` missing `kcrash`, `kcolorscheme` | **Fixed** (`README.md:172`). |
| README picker "1 through 9" | **Fixed** (`README.md:79` says 1-8; `src/qml/Picker.qml:22` `maxRows: 8`). |
| README CLI block incomplete | **Partially fixed.** Block now lists `--version`, `--settings`, `--configure`, `--rediscover` (`README.md:151-164`), but two of those flags do not work from the shell (see below). |
| `AGENTS.md` CLI block incomplete | **Partially fixed** (`AGENTS.md:198-209`). Same broken flags. |
| `AGENTS.md` sample missing `hiddenTargetIds` | **Fixed** (`AGENTS.md:56`). |
| `AGENTS.md` `kind` field wrong | **Fixed** (`AGENTS.md:142-148` documents write-on-save, ignore-on-load accurately). |
| `docs/RELEASING.md` step 5 missing reconfigure | **Fixed** (`docs/RELEASING.md:77-86`). |
| `docs/config.schema.json` missing `kind`, `browserName` | **Fixed** (`docs/config.schema.json:183-197`). |
| `CLAUDE.md` "restart daemon" vs `AGENTS.md` `--rediscover` | **Aligned in prose** (`CLAUDE.md:22`, `AGENTS.md:20-26`). Both recommend `lane --rediscover`. The flag itself is broken at the CLI (see below), so the alignment is misleading. |

### New / remaining errors

| File | Wrong line (quoted) | Correct replacement |
|------|---------------------|---------------------|
| `README.md` | `4. Run `lane --rediscover` so the running daemon reloads the file.` (`README.md:128`) and CLI block `lane --rediscover` (`README.md:164`) | Register `--rediscover` (and `--configure`) in `QCommandLineParser` in `src/app/main.cpp:90-105`, or change docs to the working path: restart via `pkill -f "lane --daemon"; lane --daemon &`, or D-Bus activation. Today `lane --rediscover` exits 1: `Lane: Unknown option 'rediscover'.` Handler exists only in `Controller::handleArgs` (`src/app/Controller.cpp:315-316`), which runs after `parser.process()` and never sees unknown flags. |
| `README.md` | `lane --configure` (`README.md:163`) | Same as above. `lane --configure` exits 1: `Unknown option 'configure'.` Alias is parsed only inside `Controller::handleArgs` (`Controller.cpp:313`). |
| `CHANGELOG.md` | `CLI docs in README and AGENTS.md list every option the binary accepts, including ... --rediscover.` (`CHANGELOG.md:56-58`) | False claim until `main.cpp` registers those options. `lane --help` (`main.cpp:90-105`) omits `--rediscover` and `--configure`. |
| `README.md` | `[GitHub Releases](https://github.com/bitskc/lane/releases)` (`README.md:25`) | `https://github.com/bitskc/tern/releases` until the GitHub repo is renamed. `bitskc/lane` returns HTTP 404 (verified 2026-09-12). `git remote` is still `bitskc/tern`. |
| `CHANGELOG.md` | `[Unreleased]: https://github.com/bitskc/lane/compare/...` and `[0.1.0]: https://github.com/bitskc/lane/releases/tag/v0.1.0` (`CHANGELOG.md:242-243`) | Use `bitskc/tern` URLs until rename. |
| `data/app.lane.Lane.metainfo.xml` | `<url type="homepage">https://github.com/bitskc/lane</url>` (`metainfo.xml:8-9`) | `https://github.com/bitskc/tern` (or rename the GitHub repo). |
| `packaging/PKGBUILD` | `source=("$pkgname-$pkgver.tar.gz::https://github.com/bitskc/lane/archive/...")` and `sha256sums=('SKIP')` (`PKGBUILD:16-24`) | Point at `bitskc/tern` tarball URL and pin a real checksum, or finish the GitHub rename and regenerate. Comment still says rename is pending. |
| `openspec/changes/initial-tern/proposal.md` | `No Lua, no containers, no PDF/mailto.` (`proposal.md:11`) and out-of-scope `Firefox Multi-Account Containers` (`proposal.md:26`) | Containers shipped (`CHANGELOG.md:12-21`, `AGENTS.md:151-163`). Mark spec stale or update scope so a new contributor is not told containers do not exist. |
| `openspec/changes/initial-tern/design.md` | `1-9 when the filter is empty` (`design.md:19`) | `1-8` to match `src/qml/Picker.qml:22` (`maxRows: 8`). |
| `CONTRIBUTING.md` | Build block (`CONTRIBUTING.md:7-11`) has only `cmake` commands | Add "install deps from README" or repeat the `pacman` one-liner. A stranger who reads CONTRIBUTING first hits configure failure on a clean box. |

**Verified non-errors**

- `lane --version`, `--help`, `--list`, `--explain`, `--config-path` work without the daemon (tested; `--list` printed 44 targets, `--explain` printed routing for `https://github.com/bitskc/lane`).
- `CLAUDE.md` and `AGENTS.md` agree on config reload semantics (both mention `--rediscover`); the problem is implementation, not doc contradiction.

## Schema drift

Top-level `Config` fields: **no drift**. All 22 keys in `src/core/types.h:138-161` appear in `docs/config.schema.json` and `loadConfig` / `saveConfig` (`src/core/config.cpp:222-352`).

`customTargets` items: **aligned** with code and AGENTS after last round. Schema documents `browserName` and `kind` with the ignore-on-load note (`docs/config.schema.json:183-197`; loader hardcodes `Kind::Custom` at `config.cpp:118`).

Minor type bound drift:

| Field | Schema | Code / UI |
|-------|--------|-----------|
| `holdMs` | `minimum: 0` (`docs/config.schema.json:73`) | Settings UI clamps **400-5000** (`src/qml/pages/PreferencesPage.qml:59-60`). Hand-edited `100` loads but UI will not expose it. Consider `minimum: 400` in schema or document UI-only clamp. |

`rules`, `substitutions`, `targetOrder`, `targetAliases`, `hiddenTargetIds`, `remembered` match code and AGENTS.

## CI/release

**CI (`.github/workflows/ci.yml`)**

- Runs on `ubuntu-latest` with `archlinux:latest` container. Good match for README's Arch/CachyOS audience; not a generic Ubuntu build.
- Installs full dep set including `kcrash`, `kcolorscheme`, `appstream` (README omits `appstream`; only needed for optional local `appstreamcli validate`, not configure).
- Builds **Debug** (`ci.yml:29`) while README says **Release** (`README.md:174`). Both work; Release is what packagers and end users expect.
- Runs `cmake --install build --prefix build/throwaway-install` before tests so `appstreamtest` actually checks metainfo. README workflow skips this.
- `QT_QPA_PLATFORM=offscreen` set. Sensible for headless CI.
- Does **not** load QML modules (`Picker.qml`, `Hold.qml`). Kirigami API drift (the `borderColor` regression in `CHANGELOG.md:68-69`) still would not fail CI.
- Latest runs on `bitskc/tern` (2026-09-11): CI **success** on merge of rename/review-fixes PR; prior offscreen fix landed same day.

**Release (`.github/workflows/release.yml`)**

- Tag-triggered only (`v*`). Does not build binaries; creates GitHub Release with notes from `CHANGELOG.md`.
- Awk extractor verified locally against `CHANGELOG.md` for `0.1.0`: pulls the right section body (matches `release.yml:19-32` logic).
- Manual fallback `sed` in `docs/RELEASING.md:142-143` uses a different parser than `release.yml`; fine for emergencies, easy to diverge on edge-case headers.
- `docs/RELEASING.md` three-file lockstep (`CMakeLists.txt`, `CHANGELOG.md`, `metainfo`) is accurate and executable. Step 5 now correctly requires reconfigure after version bump.
- Release workflow runs on `ubuntu-latest` **without** a container and does not run tests. Doc says build/test happen in CI on `main` (`RELEASING.md:134-136`). Tag pusher must trust CI was green on the release commit; doc does not say "wait for CI on the tag."

**Repo rename gap:** User-facing URLs (`README.md`, `CHANGELOG.md`, `metainfo.xml`, `PKGBUILD`, `test_version.cpp:88-101`) say `bitskc/lane`. Git remote and releases live at `bitskc/tern`. Broken links and `PKGBUILD` `SKIP` checksum until rename or URL rollback.

## Contribution path

- `CONTRIBUTING.md` exists with build commands, browser-family guide (`CONTRIBUTING.md:13-39`), changelog rule, and license note. **Improvement since last review.**
- No `.github/ISSUE_TEMPLATE/`, no PR template, no `CODEOWNERS`.
- First PR shape: focused diff, `## [Unreleased]` bullet in `CHANGELOG.md`, no version bump in feature PRs (`CONTRIBUTING.md:43-48`), follow `docs/RELEASING.md` only on release commits.
- `docs/RELEASING.md:47` references `CONTRIBUTING.md` correctly.

## Verdict

**Shippable for a solo Arch/KDE developer who already has deps**, but not doc-honest for three workflows the project advertises: (1) `lane --rediscover` after hand-editing config, (2) GitHub links in README/changelog/metainfo, (3) README-only build+test as a full validation gate (`appstreamtest` skips).

**Score (getting started, Arch clone):** 7/10 with deps installed (fast ~51 s build); 5/10 on a clean box if CONTRIBUTING is the entry (no `pacman` line); 4/10 if the contributor trusts `lane --rediscover` or `bitskc/lane` URLs.

### Ranked fix list

1. Register `--rediscover` and `--configure` in `src/app/main.cpp` `QCommandLineParser` (or remove them from README/AGENTS/CHANGELOG and document restart-only reload). This is the biggest doc/code lie left.
2. Fix GitHub URLs to `bitskc/tern` everywhere, or complete the GitHub repo rename to `bitskc/lane` and regenerate `PKGBUILD` checksum.
3. Add `cmake --install` (or document that `appstreamtest` needs it) to README build section so local `ctest` matches CI honesty.
4. Update `openspec/changes/initial-tern/` (containers in scope, picker keys 1-8) so OpenSpec does not mislead new contributors.
5. Point `CONTRIBUTING.md` build section at README `pacman` deps (or inline the list).
6. Align `holdMs` schema `minimum` with `PreferencesPage.qml` (400) or document hand-edit vs UI range.
7. Add a headless QML smoke test under `QT_QPA_PLATFORM=offscreen` so Kirigami API drift fails CI.
8. Add GitHub issue/PR templates when external contributors show up.

## Considered and fine

- README `pacman` dependency list matches `CMakeLists.txt` and CI after `kcrash` / `kcolorscheme` addition.
- `AGENTS.md` JSON sample, `hiddenTargetIds`, and `customTargets` `kind` behavior match code.
- `docs/RELEASING.md` reconfigure-after-bump step is correct.
- `docs/config.schema.json` top-level and `customTargets` fields match `config.cpp` load/save.
- CI Arch container choice fits the stated audience better than plain Ubuntu.
- `lane --list` / `--explain` / `--config-path` / `--version` work without daemon; good agent ergonomics for read-only inspection.
- `CONTRIBUTING.md` "Adding a browser family" section closes last round's gap.
- `CLAUDE.md` and `AGENTS.md` reload guidance is consistent (implementation gap is separate).
- Release changelog extraction awk matches Keep a Changelog `## [0.1.0]` headers.
- PolyForm license called out honestly in `RELEASING.md` AppStream validator section.

## Method

1. `gstack-skill-start --skill plan-devex-review` -> `SESSION_ID: 3239336-1789189764-00808fad`, `TEL_START: 1789189764`.
2. Read `~/.claude/skills/gstack/plan-devex-review/SKILL.md`; adapted shipped-repo review (no AskUserQuestion; write-only deliverable).
3. Scratch build per `README.md` at `/tmp/lane-devex-build` (Release, no install). Timed configure/build/test. `pacman -Q` for each README package (read-only).
4. Ran `lane --version`, `--help`, `--list`, `--explain`, `--config-path`, and probed `--rediscover` / `--configure` (failed as documented). Did not run `--daemon`, `--pick`, `--settings`, `lane <url>`, or touch `~/.config/lane/`.
5. Compared `docs/config.schema.json`, `src/core/config.cpp`, CI/release workflows, `docs/RELEASING.md`, `CONTRIBUTING.md`, `openspec/changes/initial-tern/`, and prior `docs/reviews/devex.md` findings.
6. `gstack-skill-end` with outcome success.
