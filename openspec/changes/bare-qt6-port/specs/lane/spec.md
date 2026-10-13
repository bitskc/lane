# lane delta: bare Qt6 port

## MODIFIED Requirements

### Requirement: Single instance and activation
Lane SHALL run as a single per-user instance. Additional invocations SHALL hand their arguments (URL to route, or `--settings`) to the running instance and exit. The mechanism SHALL NOT require a desktop-bus name owned before the argument handoff completes; on Linux, `DBusActivatable=true` launch SHALL keep working through the freedesktop Application interface.

#### Scenario: URL handoff
- **WHEN** a Lane daemon is running and a second `lane <url>` starts
- **THEN** the running instance receives and routes the URL and the second process exits promptly

#### Scenario: Cold start via desktop activation
- **WHEN** no Lane instance runs and the desktop entry is activated with a URL
- **THEN** Lane starts, claims its activation endpoint, and routes the URL

### Requirement: System tray presence
Lane SHALL show a passive tray icon on platforms with a status-icon mechanism; activating it SHALL open the Settings window.

#### Scenario: Tray activation
- **WHEN** the user clicks Lane's tray icon
- **THEN** the Settings window opens or raises

### Requirement: Notifications with actions
Lane SHALL post transient desktop notifications for browser-takeover warnings, save failures, launch failures, and silent-open confirmations. On platforms with notification actions, the takeover notification SHALL include a "Restore Lane" action.

#### Scenario: Action notification on Linux
- **WHEN** an external app claims the http(s) handler on Linux
- **THEN** the notification names the new default and offers a restore action that re-registers Lane when activated

#### Scenario: Platform without actions
- **WHEN** the platform notification mechanism has no action support
- **THEN** Lane still delivers the notification text and surfaces the restore path inside the Settings window

### Requirement: Crash reporting
Lane SHALL NOT depend on KDE's crash dialog. On fatal failure it SHALL write a crash log containing the terminating reason and recent log output to a file in the application data directory.

#### Scenario: Fatal log
- **WHEN** Lane aborts or hits an unhandled exception
- **THEN** a timestamped crash log exists under the application data directory

### Requirement: Application metadata and About display
Lane SHALL set its application name, version, organization, and license identity without KDE metadata classes. The About presentation SHALL display the PolyForm Noncommercial license text or a short notice naming the license and where the full text lives.

#### Scenario: Version string
- **WHEN** the user runs `lane --version` or opens About
- **THEN** the displayed version equals the CMake project version

#### Scenario: License notice
- **WHEN** the About presentation is shown
- **THEN** it names PolyForm Noncommercial 1.0.0 and COMMERCIAL.md rather than a bare "Custom" label
