# lane delta: bare Qt6 port

## ADDED Requirements

### Requirement: System tray presence
Lane SHALL show a tray icon on platforms with a status-icon mechanism; activating it SHALL open the Settings window, and its context menu SHALL offer Settings and Rediscover browsers. The tray SHALL NOT offer Quit (the resident daemon owns link routing).

#### Scenario: Tray activation
- **WHEN** the user clicks Lane's tray icon
- **THEN** the Settings window opens or raises

#### Scenario: Tray menu
- **WHEN** the user opens the tray context menu
- **THEN** it offers "Settings" and "Rediscover browsers" and no Quit entry

### Requirement: Application metadata
Lane SHALL set its application name, version, organization, and desktop-file identity directly on the application object without KDE metadata classes. The displayed version SHALL equal the CMake project version and the Overview page SHALL continue to show the PolyForm Noncommercial license notice.

#### Scenario: Version string
- **WHEN** the user runs `lane --version` or reads the Overview page
- **THEN** the displayed version equals the CMake project version

### Requirement: Crash log
Lane SHALL NOT depend on KDE's crash dialog. On fatal failure it SHALL write a crash log containing the terminating reason and recent log output to a file in the application data directory.

#### Scenario: Fatal log
- **WHEN** Lane aborts or hits an unhandled exception
- **THEN** a timestamped crash log exists under the application data directory
