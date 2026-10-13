# lane delta: Qt Quick Controls UI port

## MODIFIED Requirements

### Requirement: Picker overlay
The picker SHALL be a Wayland layer-shell overlay sized to the active screen, with exclusive keyboard and a 440px card. Type SHALL filter; 1-8 SHALL pick when the filter is empty; Enter, Esc, Alt+A, and Alt+P SHALL confirm, cancel, remember, and open in a private window. The overlay SHALL render using Qt Quick Controls with no Kirigami runtime dependency.

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

## ADDED Requirements

### Requirement: Settings window
The settings window SHALL present the same page model (Overview, Browsers & apps, Rules, Preferences) implemented with Qt Quick Controls and grouped card delegates inside scrollable pages, without Kirigami or kirigami-addons at runtime.

#### Scenario: Drag reorder persists
- **WHEN** the user drags a browser row to a new position in Browsers & apps while not filtering
- **THEN** `targetOrder` updates in config.json and the picker order follows

#### Scenario: Private windows are not orderable
- **WHEN** the Private windows section is shown in Browsers & apps
- **THEN** its rows show no drag handle and cannot be reordered

### Requirement: Platform styling without KDE QML runtime
The UI SHALL use the Fusion Qt Quick Controls style for its widgets and SHALL take its colors, fonts, and icons from the platform theme; it SHALL NOT require any KDE QML module at runtime.

#### Scenario: Plasma appearance
- **WHEN** Lane runs on Plasma with the KDE platformtheme plugin
- **THEN** controls render in Fusion with the user's Plasma colors and fonts

#### Scenario: No KDE modules loaded
- **WHEN** Lane runs on any host without Kirigami installed
- **THEN** the UI renders fully usable with zero `org.kde` imports resolved at runtime
