# cross-platform Specification

## Purpose
Lane's full application builds and runs on a platform without KDE Frameworks, using a small platform backend interface for the OS-specific seams (single instance, default browser, autostart, tray, notifications, overlay windowing, source discovery).

## ADDED Requirements


### Requirement: KF6-free application build
Lane SHALL build its full application from source using Qt6 plus LayerShellQt only; no KDE Frameworks development package SHALL be required at configure or compile time.

#### Scenario: Configure on a KF6-free host
- **WHEN** Lane is configured on a Linux host with Qt6 and LayerShellQt but no KF6 packages
- **THEN** configure and build complete successfully

### Requirement: Platform backend seam
OS-specific behavior (single instance, default-browser registration and detection, autostart, tray icon, desktop notifications, overlay window hints, click-source info, discovery roots) SHALL be supplied through one platform backend interface with exactly one implementation selected per operating system at compile time.

#### Scenario: Linux backend parity
- **WHEN** Lane runs on Linux
- **THEN** the Linux platform backend supplies the behavior previously provided by KF6 services, with no feature lost

### Requirement: Single instance without D-Bus activation dependency
Exactly one Lane process SHALL own the user session's click routing at a time; a second invocation SHALL forward its URL (or `--settings` request) to the running instance and exit. On Linux, D-Bus-activatable launch SHALL continue to work.

#### Scenario: Second invocation forwards
- **WHEN** a Lane daemon is running and `lane <url>` is invoked
- **THEN** the running instance routes the URL and the new process exits without launching a second daemon

#### Scenario: Desktop-file D-Bus activation
- **WHEN** the desktop entry declares `DBusActivatable=true` and the desktop launches it while no instance runs
- **THEN** Lane starts, registers its D-Bus name, and the launcher's activation request is handled

### Requirement: Desktop notifications with actions
Lane SHALL deliver its notifications through the platform-native mechanism and SHALL preserve actionable buttons where the platform supports them.

#### Scenario: Watchdog restore action
- **WHEN** the default-browser watchdog detects a takeover on Linux
- **THEN** the notification presents a "Restore Lane" action that re-registers Lane when clicked

### Requirement: Crash log without KDE crash handler
On fatal termination Lane SHALL write a crash log (message log plus termination reason) under the platform application-data directory instead of showing a KDE crash dialog.

#### Scenario: Crash produces a log file
- **WHEN** Lane terminates via `std::terminate` or a fatal `qFatal`
- **THEN** a log file exists in the platform data directory with the terminating message and recent log lines
