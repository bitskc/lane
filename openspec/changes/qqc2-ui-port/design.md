# Design: Qt Quick Controls UI Port

## Context

7 QML files today, measured Kirigami surface (grep at `7552c5f`):

| API | Sites | Replacement |
|---|---|---|
| `FormCard.*` delegates | ~80 across 4 pages | Local `ui/` delegate module |
| `Kirigami.Theme` | 63 | `palette` / `ApplicationWindow.palette`, `Material`/`Fusion` colors |
| `Kirigami.Units` | 22 | dp constants in `ui/Theme.qml` singleton |
| `Kirigami.Icon` | 10 | `IconImage` (themed on Linux via `QIcon::fromTheme`) |
| `Kirigami.ShadowedRectangle` | 2 | `Rectangle` + `MultiEffect` shadow |
| `Kirigami.Heading` / `MessageType` | 4 | `Label` + `ui/Theme` font roles |
| `ListItemDragHandle` | 1 | Hand-ported drag (~100 lines, see decision 3) |

`Controller` exposes everything QML needs already (targets/routers/hold properties) — zero C++ contract changes.

## Goals / Non-Goals

**Goals**: `ldd`/runtime requires no Kirigami modules; settings/picker/hold visually near-identical on Plasma; one QML tree for all OSes; drag reorder keeps the kind+incognito sibling invariant.

**Non-Goals**: no layout redesign or feature changes; no Windows/macOS-specific polish; no new animations beyond a shadow and the existing hold progress bar.

## Decisions

### 1. Local `ui/` module instead of inline rewrites

`src/qml/ui/` — a QML module (URI `app.lane.ui`) exporting FormCard-shaped components so call sites migrate with minimal churn:

- `FormCard.qml` — grouped container: `ColumnLayout` of delegates inside a rounded, subtly-outlined `Rectangle`, matching FormCard's card look.
- `FormHeader.qml` — section title row.
- `FormDelegateSeparator.qml` — inset divider between delegates.
- `SwitchRow`, `ComboRow`, `ButtonRow`, `TextRow`, `TextFieldRow`, `SpinBoxRow` — the six delegate shapes actually used (inventory above: 13 Button, 13 Switch, 7 Text, 6 Combo, 3 TextField, 1 SpinBox, 1 Abstract). Each takes `text`, `description`, and the control property (`checked`/`model`/`displayText`...) exactly like FormCard, so ~80 call sites become sed-scale edits.
- `Theme.qml` singleton — spacing/radius/color tokens replacing `Kirigami.Units` and theme accents: pulls from `ApplicationWindow.palette` so `qqc2-desktop-style` recolors it automatically.
- `ThemedShadow.qml`, `DragHandle.qml`.

Rationale over inline QQC2: FormCard's grouped-card look is the app's visual identity; rewriting each page freehand produces a generic settings window and 4 divergent implementations.

### 2. Style selection

`qtquickcontrols2.conf` (qrc): `Style=org.kde.desktop` when the style exists at runtime; fallback `Fusion`. Qt picks the conf at startup — detect `qqc2-desktop-style` presence at build time is unnecessary: set `Style=org.kde.desktop` + `FallbackStyle=Fusion`. On Windows/macOS same conf just falls through to Fusion/Fluent automatically. No per-OS QML.

### 3. DragHandle

QQC2 has no `ListItemDragHandle`. Port Kirigami's approach directly (its source is ~150 lines): a handle `MouseArea` inside the delegate; `drag.target` = delegate; on drag, compute target index from `ListView.indexAt`/y-position; on release call the existing `controller.moveTarget(id, index)` — which already enforces kind+incognito sibling rules in `moveIdAmongSiblings`. Model-side tests already cover the ordering rule; the QML side only needs to produce correct `from/to` indices. Auto-scroll: drive `listView.contentY` when the dragged item nears edges (Kirigami does this via `flickable` — same shape).

### 4. Picker/Hold overlays

`ShadowedRectangle` → `Rectangle { radius; color; border }` + `MultiEffect` (Qt 6.8+, in `QtQuick.Effects`) or plain `layer.enabled` shadow fallback. Exclusive keyboard, layer-shell scope, type-to-filter, and the `holdProgress` binding are windowing/C++ concerns — unchanged.

### 5. About page

`AboutPage.qml` lands in `bare-qt6-port` task 7 written FormCard-style; this port converts it like every other page (it's listed in the 7-file scope as the 8th file).

## Risks / Trade-offs

- **Visual regression on Plasma** → `qqc2-desktop-style` keeps it native; verify via screenshot diff in tests task (existing screenshot workflow). If the style renders poorly, Fusion still ships — flag for user review, don't block.
- **DragHandle correctness** → the model invariant is already tested; QML-side risk is index computation at section boundaries — write a manual-test checklist (drag across section edges, drag to top/bottom, drag while filtered).
- **`IconImage` theme support varies** (6.8+ supports themed icons on Linux) → fallback `Image` + `icon.name` URL provider; verify each icon renders in the settings screenshot pass.
- `MultiEffect` needs Qt 6.8+; Qt min is 6.6 → use `layer.effect`-free approach: static shadow via a second `Rectangle` offset+radius, zero dependency.

## Migration Plan

One branch. Order: `ui/` module skeleton + Theme → port Settings pages (biggest surface, easiest to verify side-by-side) → Picker.qml → Hold.qml → build system cleanup → screenshot verification. Rollback is a branch revert; QML is bundled in the binary, no schema or config risk.

## Open Questions

None blocking: style fallback and icon strategy decided above.
