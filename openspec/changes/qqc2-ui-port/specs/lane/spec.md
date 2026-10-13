# lane delta: Qt Quick Controls UI port

## MODIFIED Requirements

### Requirement: Picker overlay
The picker SHALL be a screen-edge overlay sized to the active screen, with exclusive keyboard where the platform windowing system permits and a 440px card. Type SHALL filter; 1-8 SHALL pick when the filter is empty; Enter, Esc, Alt+A, and Alt+P SHALL confirm, cancel, remember, and open in a private window. The overlay SHALL render using Qt Quick Controls with no Kirigami runtime dependency.

#### Scenario: Number pick
- **WHEN** the filter is empty and the user presses 1 through 8
- **THEN** Lane opens the corresponding row

#### Scenario: Remember with Alt+A
- **WHEN** the user presses Alt+A
- **THEN** Lane remembers the destination for the current path scope and opens it

#### Scenario: Private window with Alt+P
- **WHEN** the user highlights a browser destination and presses Alt+P
- **THEN** Lane resolves the destination's private counterpart and launches it immediately without saving to remembered destinations

#### Scenario: Wayland exclusive input
- **WHEN** the picker shows on a Wayland compositor with layer-shell
- **THEN** it takes exclusive keyboard focus and dismisses on Esc

### Requirement: Hold HUD on silent opens
Silent opens (remembered, PWA, default) SHALL show a hold HUD for `holdMs` unless `holdAutoOpen` is off, rendered with Qt Quick Controls.

#### Scenario: Hold cancels
- **WHEN** a silent open is held and the user presses Esc or Space
- **THEN** Lane cancels the launch and shows the picker

#### Scenario: Hold disabled
- **WHEN** `holdAutoOpen` is off
- **THEN** Lane opens immediately with no hold HUD

### Requirement: Settings window
The settings window SHALL present the same two-pane page model (Overview, Browsers & apps, Rules, Preferences, About) implemented with Qt Quick Controls and grouped card delegates, without Kirigami or kirigami-addons at runtime.

#### Scenario: Drag reorder persists
- **WHEN** the user drags a browser row to a new position in Browsers & apps
- **THEN** `targetOrder` updates in config.json and the picker order follows

#### Scenario: Private windows stay siblings
- **WHEN** the user drags a row in the Private windows section
- **THEN** only incognito targets reorder; normal browser order is untouched

### Requirement: Native desktop styling
The UI SHALL use the platform's native Qt Quick Controls style: `qqc2-desktop-style` where present (Plasma), with a universal Fusion fallback, and SHALL NOT require a KDE QML runtime.

#### Scenario: Plasma appearance
- **WHEN** Lane runs on Plasma with `qqc2-desktop-style` installed
- **THEN** controls use the Plasma color scheme and widget styling

#### Scenario: Style absent
- **WHEN** `qqc2-desktop-style` is absent
- **THEN** the UI falls back to Fusion and remains fully usable
