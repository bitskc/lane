# cross-platform Specification

## Purpose
Lane's full application builds and runs on a platform without KDE Frameworks, using a small platform backend interface for the OS-specific seams (single instance, default browser, autostart, tray, notifications, overlay windowing, activation token, source discovery).

## Requirements

### Requirement: KF6-free application build
Lane SHALL build its full Linux application using Qt6 plus LayerShellQt without any required KDE Frameworks library. CMake, Ninja and extra-cmake-modules remain build-time tools; extra-cmake-modules SHALL NOT impose the optional KF6WindowSystem API version floor. Optional KDE-adjacent features MAY link additional libraries when present but the build MUST NOT fail without them.

#### Scenario: Configure on a KF6-free host
- **WHEN** Lane is configured on a Linux host with Qt6 and LayerShellQt but no KF6 packages
- **THEN** configure and build complete successfully

#### Scenario: Optional activation-token dependency
- **WHEN** KF6WindowSystem is absent at configure time
- **THEN** Lane still builds and runs, degrading only the Wayland activation-token handoff to launched browsers

### Requirement: Platform backend seam
OS-specific behavior (desktop notifications, autostart, activation-token production, and in later changes: default-browser registration, discovery roots, source info) SHALL be supplied through one platform backend interface with exactly one implementation selected per operating system at compile time.

#### Scenario: Linux backend parity
- **WHEN** Lane runs on Linux
- **THEN** the Linux platform backend supplies the behavior previously provided by KF6 services, with no feature lost beyond the documented exceptions (activation token when KF6WindowSystem is absent; tray Passive status)

### Requirement: Single instance without D-Bus activation dependency
Exactly one Lane process SHALL own the user session's click routing at a time; a second invocation SHALL forward its request (URL to route, or `--settings`) to the running instance and exit. On Linux, D-Bus-activatable launch SHALL continue to work via the freedesktop Application interface on the documented object paths.

#### Scenario: URL handoff
- **WHEN** a Lane daemon is running and `lane <url>` is invoked
- **THEN** the running instance routes the URL and the new process exits without launching a second daemon

#### Scenario: Desktop-file D-Bus activation
- **WHEN** the desktop entry declares `DBusActivatable=true` and the desktop launches it while no instance runs
- **THEN** Lane starts, registers its D-Bus name and activation object paths, and the launcher's request is handled

#### Scenario: Stale socket recovery
- **WHEN** a previous Lane process died leaving its local socket file behind
- **THEN** the next invocation detects the dead listener, removes the stale socket, and becomes the primary instance instead of exiting

#### Scenario: Bus-name collision is fatal in daemon mode
- **WHEN** Lane starts as a daemon and the D-Bus name `app.lane.Lane` is already owned
- **THEN** it exits nonzero rather than running a second unrouted daemon (preserving systemd Type=dbus readiness semantics)

### Requirement: Desktop notifications with actions
Lane SHALL deliver its notifications through the platform-native mechanism and SHALL preserve actionable buttons where the platform supports them. Notifications SHALL carry the application desktop-entry hint so the desktop attributes them to Lane.

#### Scenario: Watchdog restore action
- **WHEN** the default-browser watchdog detects a takeover on Linux
- **THEN** the notification presents a "Restore Lane" action that re-registers Lane when clicked

#### Scenario: Platform without actions
- **WHEN** the platform notification mechanism has no action support or the notification service is absent
- **THEN** Lane degrades to a best-effort notice and the restore path remains reachable in the Settings Overview page

### Requirement: Crash log without KDE crash handler
On fatal termination Lane SHALL write a crash log (message log plus termination reason) under the platform application-data directory instead of showing a KDE crash dialog.

#### Scenario: Crash produces a log file
- **WHEN** Lane terminates via `std::terminate`, `qFatal`, or a fatal signal
- **THEN** a log file exists in the platform data directory with the terminating message and recent log lines

### Requirement: Activation token relay
When the platform supports activation tokens, a click received through any handoff transport (local socket, D-Bus Activate/Open/CommandLine) SHALL carry the caller's activation token through to the launched browser's environment.

#### Scenario: Token survives D-Bus activation
- **WHEN** a desktop launches Lane via `org.freedesktop.Application.Open` with an activation token in platform_data
- **THEN** the launched browser receives that token and the compositor may focus it

#### Scenario: Token survives local-socket handoff
- **WHEN** a second `lane <url>` invocation forwards to the running daemon with `XDG_ACTIVATION_TOKEN` set in its environment
- **THEN** the daemon exports that token to the launched browser
