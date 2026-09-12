# OpenSpec Audit: `initial-tern`

Audited 2026-09-12. Repo at `fe77477` on branch `lane-screenshots`. Read-only audit of `openspec/changes/initial-tern/` against shipped source.

## Verdict

**STALE — spec contradicts shipped product and was never promoted to canonical spec.**

## Requirement coverage

| Requirement | Implemented? (evidence) | Covering task | Status |
|---|---|---|---|
| **Scope: discovery** | Yes — `src/core/discovery.cpp` | Core bundle (task 1) | [x] |
| **Scope: pipeline (O365, unshorten, substitutions)** | Yes — `src/core/pipeline.cpp`, `src/core/unshorten.cpp` | Core bundle (task 1) | [x] |
| **Scope: rules** | Yes — `src/core/matcher.cpp`, `src/core/router.cpp`, `src/app/RuleModel.cpp` | Core + settings tasks (1, 7) | [x] |
| **Scope: PWA auto-open** | Yes — `src/core/router.cpp:188-200` (`pwaShouldAutoOpen`) | Core bundle (task 1, implicit) | [x] |
| **Scope: picker** | Yes — `src/qml/Picker.qml`, `src/app/Controller.cpp` | App bundle (task 2) | [x] |
| **Scope: toast** | Yes — `src/app/Controller.cpp:671-717` (`KNotification`) | — | **UNCOVERED** |
| **Scope: settings** | Yes — `src/qml/pages/*.qml`, `src/app/Controller.cpp` | App + settings tasks (2, 7) | [x] |
| **Scope: CLI** | Yes — `src/app/main.cpp:90-138` | CLI task (8) | [x] |
| **Scope: autostart** | Yes — `src/app/Autostart.cpp:41-42` (`lane --daemon`) | App bundle (task 2) | [x] |
| **Scope: set-as-default** | Yes — `src/app/Controller.cpp:430-438`, `src/qml/pages/OverviewPage.qml:23-28` | Task 10 | [ ] (done, not checked) |
| **Scope: No Lua** | Honored — no Lua runtime in tree | — | N/A (negative) |
| **Scope: no containers** | **Violated** — containers shipped (`src/core/discovery.cpp:395-474`) | — | **CONTRADICTS** |
| **Scope: no PDF/mailto** | Honored — http/https only; UI text at `OverviewPage.qml:19` | — | N/A (negative) |
| **Req: Discover Gecko + Chromium profiles** | Yes — `src/core/discovery.cpp` (`geckoProfiles`, Chromium `Local State`) | Core (task 1) | [x] |
| **Req: firefoxpwa; desktop Name= wins** | Yes — discovery PWA path | PWA Name task (5) | [x] |
| **Req: Route rules → remembered → PWA → picker → default** | Yes — `src/core/router.cpp:121-222` | Core + settings (1, 7) | [x] (wording stale; see drift) |
| **Req: Picker is layer-shell overlay on active screen** | Yes — `Picker.qml:14-20` (`Screen.width`/`height`) | App (task 2) | [x] |
| **Req: incognito/Tor for rules, not picker clutter** | Yes — `router.cpp:95`, `PickerModel.cpp:51` | Hide incognito (task 6) | [x] |
| **Req: never steal default except settings button** | Yes — no auto xdg-mime; button only at `Controller.cpp:430-438` | Task 10 | [ ] (done, not checked) |
| **Req: `--list` / `--explain` without daemon** | Yes — `main.cpp:114-138` exits before `KDBusService` | CLI (task 8) | [x] |
| **Out of scope: Browser Tamer Lua** | Honored | — | OK |
| **Out of scope: Firefox Multi-Account Containers** | **Violated** — full container targets | — | **CONTRADICTS** |
| **Out of scope: PDF / mailto handlers** | Honored | — | OK |
| **Design: Qt 6 resident process, D-Bus `app.lane.Lane`** | Yes — `main.cpp:141`, `Controller.cpp` tray | App (task 2) | [x] |
| **Design: autostart `lane --daemon`** | Yes — `Autostart.cpp:41-42` | App (task 2) | [x] |
| **Design: decision order (5 steps)** | Yes — `router.cpp:138-217` matches order | Core (task 1) | [x] (step 2 wording stale) |
| **Design: pipeline unwrap/shorten/substitute** | Yes — `pipeline.cpp`, `Controller.cpp` launch path | Core (task 1) | [x] |
| **Design: layer-shell overlay** | Yes — `Picker.qml:17-20` | App (task 2) | [x] |
| **Design: exclusive keyboard** | Yes — `Picker.qml:18` | App (task 2) | [x] |
| **Design: blur** | **No** — no blur usage under `src/qml/` | — | **UNCOVERED / drift** |
| **Design: 520px card** | **No** — card is 440px (`Picker.qml:57`) | — | **UNCOVERED / drift** |
| **Design: type-to-filter, 1–9, Enter, Esc, Alt+A** | Yes — `Picker.qml:36-42`, `129-175` | App (task 2) | [x] |
| **Tests for router, discovery, pipeline** | Yes — `tests/test_router.cpp`, `test_discovery.cpp`, `test_pipeline.cpp` (+ others) | Tests (task 9) | [x] |

**Uncovered requirement count: 3** (toast, blur, 520px card — no task lines; implemented or not, tasks never tracked them).

## Drift findings

### 1. Containers explicitly out of scope but shipped

- **Spec says:** `proposal.md` scope: "No Lua, **no containers**, no PDF/mailto." Out of scope lists "Firefox Multi-Account Containers."
- **Shipped:** Full container discovery and launch via `ext+container:` URLs. `CHANGELOG.md` documents "Firefox and Zen contextual identities (containers)."
- **Evidence:** `src/core/discovery.cpp:395-474` builds `Kind::Container` targets with `ext+container:name=...&url=...` args; `Picker.qml:262-269` renders container rows.

### 2. "Remembered host" vs path-scoped destination ladder

- **Spec says:** `proposal.md` requirement: "remembered **host**." `design.md` step 2: "Remembered **host** from Always for this site."
- **Shipped:** Path-scoped keys (`host`, `host/segment`, …). Picker exposes a destination ladder with comma/period navigation. "Always" defaults to path scope for tenant URLs.
- **Evidence:** `src/core/destination.cpp:124-138` (`destinationLadder`), `:185-214` (`suggestedLadderIndex`); `router.cpp:177-178` uses `destinationKeyMatchesBest`; `Picker.qml:165-174` ladder keys.

### 3. Picker card width 520px vs 440px

- **Spec says:** `design.md` overlay: "**520px** card."
- **Shipped:** Card width 440.
- **Evidence:** `src/qml/Picker.qml:57` — `width: 440`.

### 4. Blur specified, not implemented

- **Spec says:** `design.md`: "blur."
- **Shipped:** Dim tint + shadowed card; no blur effect in QML.
- **Evidence:** `rg blur src/qml/` returns nothing; `Picker.qml:44-48` uses rgba dim, `:55-66` uses `Kirigami.ShadowedRectangle`.

### 5. Default-browser task unchecked but feature works

- **Spec says:** `tasks.md` last item still `[ ]`.
- **Shipped:** Settings button calls `makeDefaultBrowser()`; status reads xdg-settings.
- **Evidence:** `OverviewPage.qml:23-28`; `Controller.cpp:159-165`, `:430-438`.

### 6. D-Bus name updated in product, spec updated on rename only

- **Spec says:** `design.md` correctly lists `app.lane.Lane` (post-rename edit).
- **Note:** Rename commit `c578cac` touched openspec the same day as container landing (`3601b1a`) but did not reconcile container scope.

## Task coverage gaps

Shipped features with **no line in `tasks.md`:**

| Feature | Evidence |
|---|---|
| Hold HUD + `holdMs` / `holdAutoOpen` prefs | `src/qml/Hold.qml`, `PreferencesPage.qml:50-64`, `Controller.cpp:799-837` |
| Path-scoped memory / destination ladder UI | `src/core/destination.cpp`, `Picker.qml:165-174`, `Controller.h:52-53` |
| Firefox/Zen containers | `src/core/discovery.cpp:368-474`, `CHANGELOG.md` |
| GitHub release update checker | `src/app/UpdateChecker.cpp`, `OverviewPage.qml:88-117` |
| Drag-reorder + rename targets | `Controller.cpp:537-557`, `TargetsPage.qml` |
| Version compare + release CI | `src/core/version.cpp`, `.github/workflows/release.yml`, `docs/RELEASING.md` |
| Legacy config migration (`~/.config/tern` → `lane`) | `src/core/config.cpp:169-199`, `main.cpp:60` |
| `--rediscover` CLI + settings button | `Controller.cpp:315-316`, `:425-427`, `OverviewPage.qml:48-50`, `AGENTS.md:25` |

## Structural findings

### OpenSpec CLI

- **Present:** `/home/andy/.bun/bin/openspec` version **1.10.0**
- **`openspec list`:** reports `initial-tern` at **7/8 tasks** (matches one unchecked task).
- **`openspec validate initial-tern`:** **FAIL**
  - `Change must have at least one delta. No deltas found.`
  - Expects `specs/<capability>/spec.md` with `## ADDED/MODIFIED/...` headers and `#### Scenario:` blocks, or `skip_specs: true` in `.openspec.yaml`.
- **`openspec show initial-tern --json --deltas-only`:** also errors — `Change must have a What Changes section`.

### Missing canonical spec

- `openspec/specs/` **does not exist**.
- Change was **never archived** (still under `openspec/changes/initial-tern/`).
- No `specs/` delta directory on the change itself.
- **Effect:** There is **no canonical OpenSpec** for Lane. The only written spec is this active change folder, and it is stale relative to `main`.

### Git history vs feature landing

| When | Commit | What |
|---|---|---|
| 2026-09-10 22:13 | `d8b4b0c` | Openspec + initial product land together |
| 2026-09-10 22:43+ | `272845c`, `1ce41e5` | Path-scoped memory, hold HUD — **same night, after spec** |
| 2026-09-10 23:43 | `20967b0` | Drag-reorder + rename |
| 2026-09-11 00:19+ | `49e0d87`, `83ba59a` | CI, releases, changelog |
| 2026-09-11 08:33 | `3601b1a` | **Containers ship** |
| 2026-09-11 13:32 | `c578cac` | **Last openspec edit** (Tern→Lane rename only) |

Openspec has not been updated since the rename. Most v0.1.0 feature work landed **after** the initial spec write and **was never backported** into proposal/design/tasks.

## Recommended repair

**Recommendation: update in place, then promote — do not archive-and-rewrite from scratch.**

Reasoning:

1. The change id `initial-tern` is already referenced by tooling (`openspec list`) and git history. A clean rewrite would orphan that id without adding correctness.
2. The shipped product is coherent; the gap is documentation and OpenSpec structure, not unknown requirements.
3. Archive-and-rewrite only makes sense if you want a new change id (e.g. `lane-v0.1-shipped`) and to retire `initial-tern` as a historical mistake. That is extra process for little gain here.

**Concrete steps:**

1. **Edit `proposal.md` in place**
   - Move containers from out-of-scope into scope (or a "Shipped in 0.1.0" subsection).
   - Replace "remembered host" with "path-scoped remembered destination."
   - Add scope bullets for hold HUD, update checker, `--rediscover`, migration.

2. **Edit `design.md` in place**
   - Step 2: path-scoped ladder, not host-only.
   - Overlay: card width **440px** (or change product to 520px — pick one source of truth).
   - Drop blur or implement it; do not leave a lie in spec.

3. **Expand `tasks.md`**
   - Add tasks for the eight gap features above.
   - Mark task 10 `[x]` (default browser button verified).
   - Check off anything else already shipped.

4. **Add OpenSpec structure**
   - Create `openspec/changes/initial-tern/specs/lane/spec.md` with ADDED requirements + Scenario blocks, **or** add `.openspec.yaml` with `skip_specs: true` if you only want markdown tracking for now.
   - Add `proposal.md` **What Changes** section (required by CLI).

5. **After docs match `main`**
   - Run `openspec validate initial-tern` until clean.
   - **Archive** the change so requirements promote into `openspec/specs/` — that gives Lane its first canonical spec.

Do **not** leave `initial-tern` forever unarchived: as long as it sits in `changes/` with no `specs/` promotion, every future OpenSpec change starts from zero canonical baseline.

## Method

1. Read `openspec/changes/initial-tern/{proposal,design,tasks}.md` in full.
2. Ran `which openspec`, `openspec --version`, `openspec list`, `openspec validate initial-tern`, `openspec show initial-tern --json --deltas-only`.
3. Mapped each proposal/design bullet to `src/` implementation via `rg` and targeted file reads.
4. Verified seeded drift claims (containers, path memory, picker width, default-browser task, hold HUD, etc.) against line-level evidence.
5. Compared `git log -- openspec/` timestamps to feature commits on `main`.
6. Wrote this file only; no repo mutations elsewhere.
