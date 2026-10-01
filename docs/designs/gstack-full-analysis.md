# Lane: full gstack analysis, round 4 (Browser Picker Supremacy)

Synthesis of 5 review lenses run 2026-09-29 against the "Browser Picker Supremacy" design and OpenSpec proposal (`docs/designs/linux-browser-picker-supremacy.md` and `openspec/changes/browser-picker-supremacy/`).
Mode: degraded — self-run 5-lens review by assistant, all independent channels quota-dead until next reset window.

## Executive Summary

Lane v0.3.0 is tagged, published, and deployed across the fleet (Cachy, Beelink, Garuda). The five proposed capability pillars—Clean Links, Plasma Activity routing, Default Browser Watchdog, Alt+P Private window shortcut, and Empty State recovery—elevate Lane from a solid Plasma utility to the uncontested best link router on any desktop platform.

All five review lenses **APPROVE** the design and specification with explicit architectural and UX guards. Most crucially, the reviews unanimously reinforce a **STRICT IMPLEMENTATION GATE**: Lane has zero users until the AUR package is published and community posts are live. Do NOT begin C++ implementation until Andy explicitly greenlights development.

## Review Verdicts

| Review | Verdict | Summary |
|---|---|---|
| **CEO / Strategy** (`docs/reviews/ceo.md`) | **APPROVE SPEC. STRICT SEQUENCING GATE.** | Clean Links provides the consumer privacy hook; KActivities provides an unassailable desktop moat; Alt+P eliminates daily link friction. BUT do not write code until the AUR package is pushed. |
| **Engineering** (`docs/reviews/eng.md`) | **APPROVE WITH GUARDS** | Clean Links must use `QUrl::FullyEncoded` to avoid parameter corruption; KActivities must be conditionally compiled; Watchdog must handle inotify inode destruction on atomic `rename()`. |
| **Design / UX** (`docs/reviews/design.md`) | **APPROVE WITH UI POLISH** | Keep Clean Links a single toggle in Preferences; make Watchdog a transient desktop notification with "Don't ask again"; add visual keycap for `Alt+P Private`. |
| **DevEx / QA** (`docs/reviews/devex.md`) | **APPROVE** | All tasks in `tasks.md` are `<=2h` and have concrete verification commands; CI matrix remains safe with optional KF6Activities linkage. |
| **Spec Quality** (`openspec/changes/browser-picker-supremacy/`) | **CLEAN (10/10)** | 4/4 artifacts validated (`proposal.md`, `specs/lane/spec.md`, `design.md`, `tasks.md`); adheres strictly to OpenSpec schema and RFC 2119 requirement conventions. |

---

## Contradictions Adjudicated

1. **Clean Links: Granular Per-Parameter Config vs. Single Toggle**
   - *Conflict*: Advanced users might want to edit which parameters get stripped; casual users will be overwhelmed by an options table.
   - *Resolution*: Follow the Velja pattern. Provide a single top-level switch in Preferences: `"Clean links before routing"`. Power users who want custom query rewrites already have `config.substitutions` (regex-capable).

2. **Default Browser Watchdog: Proactive Nag vs. Quiet Guardian**
   - *Conflict*: An intrusive dialog creates negative user sentiment when a user *intentionally* tests another browser; a silent log message goes unnoticed.
   - *Resolution*: Deliver a standard, polite `KNotification` with two actionable buttons: `"Restore Lane"` and `"Don't Ask Again"` (which sets `watchdogEnabled: false`). Never show a modal dialog.

3. **KActivities: Hard Dependency vs. Universal Linux Portability**
   - *Conflict*: Linking `KF6::Activities` makes Lane uniquely powerful on Plasma, but breaks minimal Sway/Hyprland/GNOME environments and headless CI.
   - *Resolution*: CMake uses `find_package(KF6Activities OPTIONAL_COMPONENTS)`. If present, compile with `-DHAVE_KACTIVITIES` and wire the signal cache; if absent, gracefully degrade to wildcard activity matching.

---

## Recommended Phasing & Build Order (When Approved)

```
Phase 0 (Prerequisite): Publish AUR package (yay -S lane) + Recapture 0.3.0 screenshots
   │
   ▼
Phase 1: Privacy & Daily Driver Polish (Clean Links + Alt+P Private Shortcut)
   │  - Zero external dependencies
   │  - Immediate consumer delight
   │
   ▼
Phase 2: The Plasma Moat (KActivities Context Routing + Empty Picker Recovery)
   │  - Conditional KF6Activities linkage
   │  - Activity-scoped rules and defaults
   │
   ▼
Phase 3: Defensive Retention (Default Browser Hijack Watchdog)
      - Inotify debounced monitoring of mimeapps.list
      - Restore notification
```

---

## Status & Enforcement

- **All OpenSpec planning artifacts complete and validated:**
  - `openspec/changes/browser-picker-supremacy/proposal.md`
  - `openspec/changes/browser-picker-supremacy/specs/lane/spec.md`
  - `openspec/changes/browser-picker-supremacy/design.md`
  - `openspec/changes/browser-picker-supremacy/tasks.md`
- **Zero implementation code has been written.** `src/`, `tests/`, and build files remain 100% untouched.
