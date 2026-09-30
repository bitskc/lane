# Technical Design: Phase 1 — Clean Links & Private Window Shortcut

## Context

Lane runs a link-processing pipeline in `src/core/pipeline.cpp` (`runPipeline`) and presents a Wayland layer-shell picker in `src/qml/Picker.qml` managed by `src/app/Controller.cpp`. Target selection and ranking are implemented in `src/core/router.cpp`.

See `proposal.md` for motivation and scope boundaries.

## Goals / Non-Goals

**Goals:**
- Strip tracking telemetry without re-encoding, corrupting, or modifying any other part of the URL.
- Retain exact byte-for-byte query parameters, parameter order, base64 padding (`==`), and `;` separators.
- Provide immediate 1-key dispatch to the exact matching private profile using `Alt+P`.
- Fail closed if a highlighted target does not support private browsing (never accidentally launch normal history).
- Ensure private launches leave zero trace in `config.remembered` or desktop notification logs.

**Non-Goals:**
- In-depth content filtering, DOM inspection, or adblocking.
- Arbitrary regex substitution UI (power users already have `config.substitutions`).
- Supporting non-standard browser CLI flags for unknown custom script targets.
- Private window diversion from the hold HUD or silent rule opens (Alt+P is picker-only).

## Decisions

### 1. Raw-Segment Query Splicing in `src/core/pipeline.cpp`
- **Why NOT `QUrlQuery`**: Re-serializing through `QUrlQuery` encodes `=` inside values (corrupting base64 tokens like `token=YWJj==` into `token=YWJj%3D%3D`) and re-encodes IRI paths (`/café` -> `/caf%C3%A9`) and IDN hosts into punycode *only when a tracker is stripped*, breaking downstream regex and path-scope rules.
- **Algorithm**:
  1. Scheme guard: Process only `http` and `https` URLs. Return all others untouched.
  2. Locate query: Find `?` and optional `#`. If no `?` exists, return immediately.
  3. Extract raw query string slice between `?` and `#`.
  4. Split raw query on `&`.
  5. For each `key=value` segment (or lone `key`):
     - Extract `key`, percent-decode it, and check case-insensitively:
       - Starts with `utm_`? Drop.
       - Matches banned key set (`fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `mc_cid`, `mkt_tok`, `_ga`, `_gl`, `dclid`, `yclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`)? Drop.
       - Equals `si`? If host is `youtube.com`, `youtu.be`, `music.youtube.com`, or `open.spotify.com`, drop; otherwise keep.
     - Surviving segments are retained **byte-for-byte** without re-encoding.
  6. Reconstruct URL:
     - If surviving segments is empty: remove `?` entirely.
     - If segments survive: join with `&` and place between prefix and `#fragment`.

### 2. Pure Sibling Resolver: `privateCounterpart()` in `src/core/router.cpp`
- **Signature**:
  ```cpp
  const Target *privateCounterpart(const Target &target, const QList<Target> &allTargets);
  ```
- **Resolution Rules**:
  1. If `target.incognito == true`: target is already private; return `&target`.
  2. If `target.kind == Kind::Container` (e.g. `...:container:1`): strip the `:container:N` suffix to obtain base profile ID, then search for `baseId + ":private"`.
  3. Gecko targets: look up `target.id + ":private"` in `allTargets`.
  4. Chromium targets: look up `target.id + ":incognito"` in `allTargets`.
  5. Ignore `target.hidden` on the candidate: explicit `Alt+P` keystroke overrides row visibility.
  6. If no exact match is found: return `nullptr` (fail closed). Never guess flags or launch a different profile.
- **Testability**: Pure function in `src/core/router.cpp` tested comprehensively in `tests/test_router.cpp` (Gecko, Chromium, multi-profile isolation, Brave Tor avoidance, container mapping, and fail-closed nulls).

### 3. Alt+P Picker Wiring & Fail-Closed UX
- In `src/app/Controller.h` / `Controller.cpp`:
  ```cpp
  Q_INVOKABLE void pickPrivate(int row);
  ```
- Implementation:
  - If `row < 0 || row >= m_pickerModel->count()`, return.
  - Resolve target `t = m_pickerModel->targetAt(row)`.
  - Call `const Target *priv = privateCounterpart(t, m_targets)`.
  - If `!priv`:
    - Set `m_pickerNotice = "No private mode for " + t.displayName()`.
    - Do NOT launch normal window; keep picker open.
    - Return.
  - If `priv`:
    - Request activation token from still-visible `m_pickerWindow`.
    - Call `requestActivationAndLaunch(*priv, "picker-private", window, m_click)`.
    - Dismiss picker (`hidePicker()`) releasing layer-shell exclusive grab.
    - Bypasses `alwaysForHost` persistence entirely.
    - In `toast()` and notifications: omit `click.host` when reason is `"picker-private"` to prevent notification history leaks.

### 4. Configuration & Schema Plumbing
- `src/core/types.h`: Add `bool stripTrackingParams = true;` to `struct Config`.
- `src/core/config.cpp`:
  - Add `"stripTrackingParams"` to `knownKeys` set (prevents spurious unknown-key warnings).
  - Serialize and deserialize `stripTrackingParams` in JSON.
- `docs/config.schema.json`: Document `stripTrackingParams`.
- `AGENTS.md`: Update sample config shape.
- `src/app/Controller.h`: Add `Q_PROPERTY(bool stripTrackingParams READ stripTrackingParams WRITE setStripTrackingParams NOTIFY stripTrackingParamsChanged)`.
- `src/qml/pages/PreferencesPage.qml`: Add Kirigami `FormSwitchDelegate` for "Clean links before routing".
- `src/app/main.cpp`: In `--explain`, print `[Cleaned tracking parameters]` and list removed keys if URL was modified.
