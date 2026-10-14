# Design: Qt Quick Controls UI Port

## Context

7 QML files today. Measured Kirigami surface (grep at `7552c5f`, corrected):

| API | Sites | Replacement |
|---|---|---|
| `FormCard.*` delegates | ~80 across 4 pages | Local `ui/` delegate module |
| `FormCardPage` | 4 (root of every settings page) | `ui/ScrollableFormPage` |
| `Kirigami.Theme` | 63 | attached `palette` + `ui.Theme` semantic colors |
| `Kirigami.Units` | 22 | `ui.Theme` metrics (font-relative where needed) |
| `Kirigami.Icon` | 10 | `image://laneicon/` provider |
| `Kirigami.ShadowedRectangle` | 2 | `Rectangle` + `MultiEffect` |
| `Kirigami.Heading`/`MessageType` | 0 (inventory corrected) | n/a |
| `ListItemDragHandle` | 1 | `ui/DragHandle` |

`Settings.qml` is **already** a plain QQC2 `ApplicationWindow` + `RowLayout` sidebar + `StackLayout` — only its Kirigami colors/icons are replaced; the shell is not rewritten. `Controller` exposes everything QML needs; the only new C++ is a `QQuickImageProvider` registered in `ensureEngine`.

Style verdict (verified by probe + package contents): `qqc2-desktop-style` imports `org.kde.kirigami` internally → cannot be used; `FallbackStyle` does not rescue a missing primary style (Qt probe: UI fails to instantiate); `main.cpp:69` `QQuickStyle::setStyle("org.kde.desktop")` overrides any conf → deleted. **Fusion everywhere**, colors from the platform theme (KDE's platformtheme plugin on Plasma is a QPT plugin, not a QML runtime — does not violate the no-KDE-QML requirement).

## Goals / Non-Goals

**Goals**: zero `org.kde` QML imports resolved at runtime; settings/picker/hold visually near-identical on Plasma via Fusion + platformtheme palette; one QML tree for all OSes; full keyboard-model parity.

**Non-Goals**: no layout redesign or feature changes; no private-window reordering (they are not orderable by design — spec corrected); no About page; no new C++ beyond the icon provider.

## Decisions

### 1. `ui/` module: separate target, page + delegates + primitives

`src/qml/ui/` → second `qt_add_qml_module` on a `laneui` STATIC target, URI `app.lane.ui`, linked into `lane` (one call = one URI; no hand-written qmldir — the tool generates it).

- `ScrollableFormPage.qml` — FormCardPage replacement: `Flickable` + centered `ColumnLayout` capped at card width, `ensureVisible` on active-focus changes (matches FormCardPage semantics so tall pages like Preferences still scroll).
- `FormCard`, `FormHeader`, `FormDelegateSeparator` — grouped-card look.
- `FormSwitchDelegate`, `FormComboBoxDelegate`, `FormButtonDelegate`, `FormTextDelegate`, `FormTextFieldDelegate`, `FormSpinBoxDelegate` — FormCard-compatible property names so ~80 call sites are mechanical.
- `Theme.qml` singleton — metrics + semantic colors ONLY (a singleton has no window/palette; colors at call sites use the attached `palette` property). Semantic colors `positiveTextColor`/`negativeTextColor`/`neutralTextColor` mapped to palette-aware constants (QPalette has no semantic roles). Font-relative metrics where the original is `gridUnit`-based so Plasma font scaling still works.
- `ThemedShadow.qml` — `Rectangle` + `QtQuick.Effects.MultiEffect` shadow (available Qt 6.5, under our 6.6 floor; picker uses shadow.size 32, yOffset 10, alpha .42 — must reproduce to avoid a flatter look).
- `DragHandle.qml` — MouseArea-driven reorder honoring the existing invariants: **disabled while filtering** (collapsed height-0 rows make index math wrong), **capture dragId once** before the first model move, auto-scroll at edges; release → `controller.moveTarget(id, index)` (sibling rules live in `moveIdAmongSiblings`, already tested).

### 2. Icons: `image://laneicon/` C++ provider

`QQuickImageProvider` resolves `image://laneicon/<name>` using `QIcon::fromTheme`, falling back to `application-x-executable`/`image-missing`. QML supplies physical-pixel `sourceSize` including DPR; the provider renders that size once. Each engine registers the provider. Private Qt Quick Controls implementation types and recoloring abstractions are not needed.

### 3. Style: Fusion conf, kill setStyle

- Delete `QQuickStyle::setStyle("org.kde.desktop")` (main.cpp:69).
- Ship `:/qtquickcontrols2.conf` in qrc with `Style=Fusion`. (Not `org.kde.desktop` — it imports Kirigami.)
- On Plasma, KDE's platformtheme Qt plugin supplies palette/fonts — native-ish look with zero KDE QML imports. Verified acceptable visually via screenshot pass.

### 4. Keyboard-model parity checklist (the part with no test coverage)

Preserve digits 1-8 in the focused filter field, comma/period scope narrowing, Esc-clear-then-cancel, Alt+A toggling Always (persisted when a destination is confirmed), Alt+P private launch, Alt+E email, Ctrl+C copy and Alt+S/Alt+R zero-target recovery. Keep maxRows 8. The actual-app probe exercises filter cancellation and Enter/hold behavior on a private Wayland compositor; registered control tests cover drag and preference events.

## Risks / Trade-offs

- **Fusion≠Breeze look** — accepted; screenshot pass compares against `docs/screenshots/*.png`; user review gate.
- **DragHandle reimplementation** — invariants pinned in design + manual checklist; model-side already tested.
- Icon scaling applies DPR exactly once through QML sourceSize; theme lookup and fallback were verified on Qt 6.11.2.
- Semantic colors are approximations (Breeze green/red/orange) — flag to user if off-brand; no functional risk.

## Migration Plan

One branch after bare-qt6-port merges. Order: `ui` module → settings pages → picker → hold → build cleanup → screenshot verify. Rollback: revert + rebuild + reinstall + daemon restart.

## Open Questions

None blocking.
