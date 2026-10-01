# Proposal: Browser Picker Supremacy (5 High-Leverage Pillars)

## Why

Lane v0.3.0 established best-in-class Wayland layer-shell performance, path-scoped destination memory, and native container discovery. However, users still endure tracking parameter pollution (`utm_*`, `fbclid`), context-blind link routing when switching Plasma Activities (Work vs Personal), silent hijacking of the default browser association by external browser updates, and clumsy workarounds when attempting to open disposable links in private windows.

Addressing these five seams elevates Lane into the undisputed best, simplest, and most powerful link router on Linux, matching and exceeding commercial macOS alternatives like Velja and Choosy.

## What Changes

- **Clean Links (Tracking Parameter Stripping)**: Automatic, opt-in stripping of surveillance query keys (`utm_*`, `fbclid`, `gclid`, `si`, `mc_eid`, etc.) in the URL normalization pipeline prior to route evaluation.
- **Plasma Activity-Aware Routing**: Integration with KDE's `KActivities` service to allow scoping rules and setting default fallback browsers per desktop Activity (e.g. Work vs Personal).
- **Default Browser Watchdog**: Active notification and 1-click restore mechanism when external package upgrades or apps overwrite `x-scheme-handler/http` in `mimeapps.list`.
- **Picker Private Window Hotkey (`Alt+P`)**: Direct shortcut in the layer-shell picker to divert the active URL into the highlighted target's private/incognito profile without manual rule authoring.
- **Picker Empty State & Filter Recovery**: Explicit visual guidance, escape hatch, and settings CTA when search filters miss or when zero browser targets are discovered.

## Capabilities

### Modified Capabilities
- `lane`:
  - `clean-links`: Adds tracking parameter stripping to the URL pipeline.
  - `activity-routing`: Adds Plasma Activity scope to rules and default browser selection.
  - `browser-watchdog`: Adds background verification and restore notifications for default browser associations.
  - `picker-private-hotkey`: Adds `Alt+P` private window trigger to the picker overlay.
  - `picker-empty-recovery`: Adds filter clearance and settings guidance when no picker targets match.

## Impact

- Zero breaking changes to existing `config.json` schemas; new keys are additive with sensible defaults.
- Zero latency impact on picker overlay appearance (<20ms).
- Graceful degradation on non-Plasma Wayland compositors (Sway, Hyprland, GNOME) where KActivities is absent.
