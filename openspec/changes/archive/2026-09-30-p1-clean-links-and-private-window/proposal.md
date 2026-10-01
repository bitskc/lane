# Proposal: Phase 1 — Clean Links & Private Window Shortcut

## Why

Every day, links clicked from newsletters, Slack, social media (Twitter/X, YouTube, Reddit), and search engines arrive polluted with surveillance and campaign query parameters (`utm_*`, `fbclid`, `gclid`, `si`, `mc_eid`). These parameters inflate URL length, track user identity across domain boundaries, and interfere with Path-scoped rule matching (where `pathOf()` includes the raw query string). Meanwhile, users who want to open an untrusted, sensitive, or disposable link in a private/incognito window are currently forced to copy the link or author an explicit rule, because private profiles are intentionally hidden from the picker list to avoid clutter.

Phase 1 solves both daily frictions with zero external dependencies and zero impact on layer-shell overlay startup latency (<20ms).

## What Changes

- **Clean Links (Tracking Parameter Stripping)**:
  - Add `stripTrackingParams: bool` (default: `true`) to `Config`.
  - Add a raw-segment query parameter cleaner in `src/core/pipeline.cpp` using byte-level segment splicing rather than URL re-serialization. This strictly preserves base64 padding (`==`), semicolon separators, query parameter order, IRI paths, and IDN hostnames without re-encoding.
  - Case-insensitive matching on `utm_` prefixes and high-confidence unique tracking keys (`fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `mc_cid`, `mkt_tok`, `_ga`, `_gl`, `dclid`, `yclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`).
  - Host-scoped tracking parameters: `si` is stripped only when the host is `youtube.com`, `youtu.be`, `music.youtube.com`, or `open.spotify.com`.
  - Scheme guard: only cleans `http` and `https` URLs; non-HTTP schemes (e.g. `mailto:`) are passed through unmodified.
  - Safelinks (O365) invariant: trackers are stripped from `matchUrl` for rule evaluation, while `openUrl` remains the original wrapper unless `openUnwrapped` is enabled.
  - Add a toggle switch in `PreferencesPage.qml` under a new "Privacy & Filtering" section.
- **Dedicated "Open in Private Window" Shortcut (`Alt+P`)**:
  - Add `Shortcut { sequence: "Alt+P" }` in `Picker.qml`.
  - Implement pure helper `privateCounterpart(const Target &t, const QList<Target> &allTargets)` in `src/core/router.cpp`.
  - Sibling matching resolves strictly by exact profile ID: `id + ":private"` (Gecko) or `id + ":incognito"` (Chromium); container targets strip `:container:N` to resolve their parent profile's private window. This strictly prevents multi-profile cross-talk and avoids accidentally opening Brave Tor.
  - Fail closed: Targets without a private counterpart (PWAs, custom actions) do not launch normal windows; they display an inline notice "No private mode for <target>" and keep the picker open.
  - Ephemeral privacy: `Alt+P` launches bypass `alwaysForHost` persistence and suppress host names in notification history.
  - Add visual affordance `"alt+p private"` to the picker footer in lowercase monospace style.

## Capabilities

### Modified Capabilities
- `lane`:
  - `clean-links`: In-memory raw-segment query parameter stripping in `runPipeline`.
  - `picker-overlay`: Adds `Alt+P` keyboard shortcut to the layer-shell picker to route links into private browser instances.

## Distribution & Sequencing Gate

Per `docs/designs/gstack-full-analysis.md`, v0.3.0 is already released and deployed to the fleet (Cachy, Beelink, Garuda). The implementation of Phase 1 is gated on:
1. Publishing `lane` to the AUR (`yay -S lane`).
2. Refreshing README screenshots to display the current 0.3.0 UI.
3. Explicit user approval to proceed with Phase 1 C++ implementation.
