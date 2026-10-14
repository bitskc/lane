## ADDED Requirements

### Requirement: macOS application build
On macOS, Lane SHALL build its full application as a relocatable `Lane.app` bundle with bundle identifier `app.lane.Lane`, using Qt 6, the same C++ core and the same `app.lane.ui` QML module as Linux. The macOS target SHALL NOT require D-Bus, LayerShellQt, extra-cmake-modules or any KDE library. `lane-core` SHALL remain pure Qt with no Objective-C++ sources. The Linux build and its behavior SHALL be unchanged.

#### Scenario: Configure and build on macOS
- **WHEN** Lane is configured on an Apple-Silicon Mac with Xcode, Qt 6.11 or newer, CMake and Ninja
- **THEN** the build produces `Lane.app` and all core and application tests build

#### Scenario: No D-Bus dependency on macOS
- **WHEN** `Lane.app` starts on macOS
- **THEN** it does not link or call D-Bus activation, and startup does not exit because no bus exists

#### Scenario: Linux is unchanged by the seam refactor
- **WHEN** the `Platform` interface gains the macOS-driven methods before any macOS file exists
- **THEN** the full Linux ctest suite passes and the Linux native smoke (launch, picker with nonzero hit areas, default-browser state, notification) matches the run recorded before the refactor

#### Scenario: Bundle has no host-specific library paths
- **WHEN** every Mach-O file inside `Lane.app` is inspected with `otool -L`
- **THEN** no dependency path begins with `/opt/homebrew` or `/usr/local`

### Requirement: Platform default-handler, overlay, activation and autostart seam
The platform backend SHALL expose default-handler query, default-handler registration with an asynchronous result, a flag stating whether handler changes are observable by file watch, overlay-window configuration, and instance-activation registration. `Controller` and `main` SHALL reach autostart, default-handler, overlay and activation behavior only through these methods and SHALL NOT spawn `xdg-mime`, include LayerShellQt, call D-Bus registration, or call the free autostart functions directly. Exactly one backend implementation SHALL be compiled per operating system. `DefaultBrowserWatcher` SHALL be constructed only when the backend reports file-watchable handler changes.

#### Scenario: Linux behavior preserved through the seam
- **WHEN** Lane on Linux queries or sets the default browser, registers D-Bus activation, toggles autostart, or opens the picker on Wayland
- **THEN** the result is identical to the behavior before the seam existed (same `xdg-mime` queries for `x-scheme-handler/http` and `/https`, same D-Bus names and fatal-on-collision behavior, same autostart file, same layer-shell configuration, same fallback to a normal window when layer-shell is unavailable)

#### Scenario: Controller has no OS-specific calls
- **WHEN** `Controller.cpp`, `Controller.h` and `main.cpp` are inspected
- **THEN** they contain no `LayerShellQt` include, no `xdg-mime` spawn, no direct D-Bus registration call, and no call to the free `setAutostart` or `autostartEnabled`

#### Scenario: One backend per OS
- **WHEN** the project is configured for Linux and for macOS
- **THEN** each configuration compiles exactly one `Platform` implementation file

#### Scenario: Watcher only where it can work
- **WHEN** the backend reports that handler changes are not file-watchable
- **THEN** no `DefaultBrowserWatcher` is constructed and takeover detection uses the backend's polling path

### Requirement: macOS URL delivery through LaunchServices
On macOS, Lane SHALL receive links that LaunchServices opens for Lane as `QFileOpenEvent`, SHALL accept only `http` and `https` URLs, and SHALL route them through the same `Controller::openUrl` path as command-line and socket URLs. The event filter SHALL be installed immediately after the application object is created. Events that arrive before `Controller` exists SHALL be held in arrival order in a bounded queue and delivered once it exists. The command-line invocation `lane <url>` SHALL keep forwarding to a running instance over the local socket.

#### Scenario: Cold start via a clicked link
- **WHEN** Lane is not running and the system opens `https://example.com/a` with Lane as the target application
- **THEN** Lane starts, and the picker (or the matching rule's browser) handles exactly that URL once

#### Scenario: Running instance receives a link
- **WHEN** Lane is running and the system opens a second URL for Lane
- **THEN** the running instance routes it and no second daemon starts

#### Scenario: Events before readiness are not lost or reordered
- **WHEN** three URLs arrive as `QFileOpenEvent` before `Controller` exists
- **THEN** after it exists they are routed in the order received, subject to the existing burst policy in `Controller`

#### Scenario: Buffer overflow is bounded and logged
- **WHEN** more than eight URLs arrive before `Controller` exists
- **THEN** the oldest are dropped and a log line records each drop

#### Scenario: Non-web scheme is rejected
- **WHEN** a `QFileOpenEvent` carries `file:///etc/passwd` or `mailto:a@b.c`
- **THEN** nothing is routed or launched and a log line records the rejection

#### Scenario: Command-line forward
- **WHEN** a Lane daemon is running and `Lane.app/Contents/MacOS/lane <url>` is invoked
- **THEN** the daemon routes the URL, the new process exits zero only after the daemon acknowledges, and it exits nonzero on an ambiguous acknowledgment without replaying the URL

### Requirement: macOS single instance socket
On macOS the single-instance socket SHALL be a per-user local socket named `lane-<uid>.sock` inside the per-user temporary directory the OS provides for the current user. If the full path would exceed 103 bytes, Lane SHALL exit nonzero with a message naming the path and its length and SHALL NOT fall back to a shared location. Stale-socket recovery and the forward protocol SHALL match Linux.

#### Scenario: Normal path length
- **WHEN** the per-user temporary directory plus `lane-<uid>.sock` is 103 bytes or fewer
- **THEN** Lane listens on that path and a second invocation forwards to it

#### Scenario: Overlong path fails fast
- **WHEN** the resulting socket path is longer than 103 bytes
- **THEN** Lane exits nonzero with a message naming the path and length and creates no socket elsewhere

#### Scenario: Stale socket after kill
- **WHEN** a previous Lane process was killed with SIGKILL leaving its socket file
- **THEN** the next launch detects the dead listener, removes the stale file, and becomes the primary instance

### Requirement: macOS default handler registration and takeover detection
Lane SHALL become the macOS handler for `http` and `https` only when the user presses the in-app default-browser button. Registration SHALL use the asynchronous system API, SHALL report success only after re-querying both schemes and finding Lane's bundle identifier at the running bundle's resolved path, and SHALL treat a dismissed or declined system consent prompt as a distinct, non-error `Declined` state. Lane SHALL NOT change the handler from startup, from tests, or from a shell. A handler that has Lane's bundle identifier at a different resolved path SHALL be reported as another copy of Lane, with its path, and not as Lane.

#### Scenario: User accepts the consent prompt
- **WHEN** the user presses the button and approves the macOS prompt
- **THEN** after the re-query shows Lane at the running bundle's path for both `http` and `https`, Overview shows Lane as the default browser

#### Scenario: User dismisses the consent prompt
- **WHEN** the user presses the button and dismisses the macOS prompt
- **THEN** Overview shows that the handler is unchanged and names the current handler, and no success or error toast is shown

#### Scenario: Only one scheme changes
- **WHEN** the re-query shows Lane for `http` but not `https`
- **THEN** Lane reports a partial state, does not claim success, and offers the button again

#### Scenario: Takeover while watchdog is on
- **WHEN** `watchdogEnabled` is true, Lane was the handler, and another app takes `http` or `https`
- **THEN** within 60 seconds Lane raises the takeover alert with a Restore action, and Restore re-registers only when clicked

#### Scenario: No polling when watchdog is off
- **WHEN** `watchdogEnabled` is false
- **THEN** no handler-query timer runs

#### Scenario: Another router was the handler first
- **WHEN** Lane has never been the handler and a different app (for example another link router) is
- **THEN** no takeover alert is raised, and Overview names that app before the user presses the button

#### Scenario: Same bundle id at another path
- **WHEN** an application with bundle id `app.lane.Lane` at a path different from the running bundle is the handler
- **THEN** Overview shows another copy of Lane with its path and does not show Registered

### Requirement: macOS desktop notifications
Lane SHALL deliver notifications through `UNUserNotificationCenter` and SHALL preserve the Restore action. Authorization SHALL be requested the first time a notification is needed, not at launch. Action callbacks SHALL be keyed by unique notification identifier in a bounded map. If authorization is denied, the notification SHALL be dropped with a log line and Settings Overview SHALL show a banner that explains notifications are off and opens the system notification settings.

#### Scenario: Authorized takeover notification
- **WHEN** authorization is granted and a takeover is detected
- **THEN** a notification attributed to Lane appears with a Restore action that re-registers Lane when clicked

#### Scenario: Denied authorization
- **WHEN** the user has denied notifications for Lane
- **THEN** no notification is posted, a log line records the drop, and Overview shows the "Notifications are off" banner with an action that opens system settings

#### Scenario: Action map is bounded
- **WHEN** more notifications are posted than the map limit without any being clicked
- **THEN** the oldest action entries are evicted

#### Scenario: Private launch suppresses the host notification
- **WHEN** a private or incognito launch would show a host-name notification
- **THEN** it is suppressed exactly as on Linux

### Requirement: macOS login item
Lane SHALL manage start-at-login through `SMAppService.mainAppService` reached through the platform seam, and the Settings toggle SHALL reflect the real service status after every attempt. A status of requires-approval SHALL read as off with a message pointing to System Settings Login Items. An invalid-signature failure SHALL read as a build problem, not as success.

#### Scenario: Enable login item
- **WHEN** the user turns on start at login and the system accepts the registration
- **THEN** the toggle shows on and the service status is Enabled

#### Scenario: Approval required
- **WHEN** registration succeeds but the status is requires-approval
- **THEN** the toggle shows off and a message explains how to approve Lane in System Settings

#### Scenario: Signature rejected
- **WHEN** registration fails with an invalid-signature error
- **THEN** the toggle shows off and the message says the build is not signed for login items

#### Scenario: Disable login item
- **WHEN** the user turns the toggle off
- **THEN** the service is unregistered and the toggle shows off after the status re-read

### Requirement: macOS overlay window
On macOS the picker and Hold windows SHALL be borderless, appear above normal windows and on the active Space including over full-screen apps where the OS allows it, take keyboard focus after Lane activates itself, and dismiss on focus loss when `closeOnFocusLoss` applies. Lane SHALL NOT request the Accessibility or Input Monitoring permission, create event taps, or claim an exclusive input grab. `requestActivationToken` SHALL return empty once and SHALL NOT set XDG environment variables.

#### Scenario: Picker takes keyboard focus
- **WHEN** a link opens the picker while another app is frontmost
- **THEN** the picker receives key events for digits, arrows, Return and Escape without the user clicking it first

#### Scenario: Full-screen app underneath
- **WHEN** another app is full screen on the active Space and a link opens the picker
- **THEN** the picker is visible over it, or, if the OS refuses, Lane shows it as a normal top-most window on the active Space and logs the fallback

#### Scenario: No special permission requested
- **WHEN** the built `Info.plist` and entitlements are read and a full picker session runs on the verification machine
- **THEN** they contain no Accessibility or Input Monitoring usage keys and macOS shows no such prompt

#### Scenario: Hit areas on Retina
- **WHEN** the picker is shown at device pixel ratio 2
- **THEN** every row, the copy button, the email button and the footer buttons have nonzero hit rectangles inside the window

#### Scenario: No XDG token variables
- **WHEN** a browser is launched from the picker on macOS
- **THEN** the child environment contains no `XDG_ACTIVATION_TOKEN` or `DESKTOP_STARTUP_ID` set by Lane

### Requirement: macOS application discovery
On macOS Lane SHALL discover browsers from `.app` bundles in `/Applications` and `~/Applications`, offering a bundle only if its `Info.plist` declares `http` or `https` in `CFBundleURLTypes`. Target identity SHALL be `browser:macapp:<bundle-id>:<profile-key>` with the existing `:private`, `:incognito`, `:tor` and `:container:<n>` suffixes unchanged. Lane's own bundle SHALL never be a target. Bundles SHALL be deduplicated by bundle identifier (shorter path, then lexicographic) and by resolved real path. A bundle whose `CFBundleExecutable` is not a bare file name, or whose resolved executable is not a regular executable file inside the bundle's `Contents/MacOS`, SHALL be skipped with a log line. Bundle fields SHALL exist on `Target` at runtime and SHALL NOT be written to `config.json`. Gecko, Chromium, container and PWA profile parsing SHALL be shared with Linux through a per-OS profile-root table; the Linux table SHALL be unchanged.

#### Scenario: Installed browsers are found
- **WHEN** Brave, Brave Beta, Edge, Zen, Floorp and Safari are installed
- **THEN** each appears once with its bundle identifier in the target id

#### Scenario: App without URL types is excluded
- **WHEN** a bundle declares no `http` and no `https` URL type
- **THEN** it is not offered as a browser

#### Scenario: Same bundle id at two paths
- **WHEN** two bundles share one bundle identifier
- **THEN** one target is offered, from the shorter path

#### Scenario: Case-insensitive profile directories
- **WHEN** both `Zen` and `zen` resolve to the same directory
- **THEN** each profile is listed once

#### Scenario: Rules survive a move
- **WHEN** a fixture bundle is moved to a different folder and discovery runs again
- **THEN** rules and remembered choices that reference its bundle identifier still resolve

#### Scenario: Lane never routes to itself
- **WHEN** discovery runs and Lane is the registered handler
- **THEN** no target resolves to Lane's own bundle

#### Scenario: Executable escapes the bundle
- **WHEN** a bundle's `CFBundleExecutable` is `../../../../usr/bin/curl` or names a symlink pointing outside `Contents/MacOS`
- **THEN** the bundle is skipped and a log line names the reason

#### Scenario: Binary property list
- **WHEN** a bundle's `Info.plist` is a binary property list
- **THEN** it is read after conversion and the bundle is discovered like an XML one

#### Scenario: Bundle fields stay out of config
- **WHEN** configuration is saved after discovery on macOS
- **THEN** the written `config.json` contains no bundle path or bundle id fields beyond the target id

#### Scenario: Gecko PWA and container discovery
- **WHEN** fixture profile directories for a Gecko browser contain `containers.json` and a firefoxpwa profile
- **THEN** container and PWA targets are produced with the same identity suffixes as Linux

#### Scenario: Linux discovery unchanged
- **WHEN** the Linux discovery tests run against their existing fixtures
- **THEN** results are identical to before this change

### Requirement: macOS browser launch
On macOS Lane SHALL launch Gecko and Chromium browsers by executing the bundle's `Contents/MacOS/<CFBundleExecutable>` with the same argument construction as Linux, through the existing launch guards (sanitized `http(s)` URL only, executable resolution, interpreter-chain block, detached start). Safari SHALL be one normal target opened with `/usr/bin/open -b com.apple.Safari <url>` and no invented profile or private routes. The `action:email` target SHALL open `/usr/bin/open` with a `mailto:` URL whose `body` carries the fully percent-encoded sanitized URL. These `open` invocations SHALL be built by the launcher from the target's kind and bundle id with a fixed executable path and SHALL NOT take their command text from target `exec` or configuration. On macOS only, a user `custom:` target SHALL fail closed when its command, or the final target of its symlink chain, is `open`, `osascript`, `swift`, `script`, `tclsh`, `wish`, `shortcuts`, `lldb` or `automator`, or when it resolves to a directory or ends in `.app`. The Linux guard list SHALL be unchanged.

#### Scenario: Chromium profile launch
- **WHEN** a link routes to a Brave profile target
- **THEN** the Brave executable inside the bundle starts with that profile directory argument and the sanitized URL

#### Scenario: Private launch flags
- **WHEN** a link routes to a private target for Gecko, Chromium or Edge
- **THEN** the browser receives its private flag (`--private-window` for Gecko, `--incognito` for Chromium, the Edge flag confirmed by the launch smoke) and the URL

#### Scenario: Safari
- **WHEN** a link routes to Safari
- **THEN** `/usr/bin/open -b com.apple.Safari <url>` runs with no profile or private argument

#### Scenario: Email action
- **WHEN** the user triggers the email action on a URL
- **THEN** `/usr/bin/open` runs with `mailto:?body=<percent-encoded URL>`, and `xdg-email` is not used on macOS

#### Scenario: Email header injection attempt
- **WHEN** the URL is `https://x.test/?a=1&to=evil@x.test&cc=a@b.c&subject=hi`
- **THEN** the mailto URL contains exactly one `body` parameter whose value is that whole URL percent-encoded, and no `to`, `cc` or `subject` parameter

#### Scenario: Custom target naming open fails closed
- **WHEN** a user `custom:` target's command is `open`, `osascript`, a symlink or copy-by-name resolving to either, a directory, or a path ending in `.app`
- **THEN** the launch is refused with the existing blocked-command error and nothing starts

#### Scenario: Linux guard list unchanged
- **WHEN** a Linux `custom:` target's command is named `open`
- **THEN** its behavior is the same as before this change

#### Scenario: Non-web URL is refused
- **WHEN** a launch request carries a URL that is not `http` or `https`
- **THEN** nothing is launched

#### Scenario: Container target on macOS
- **WHEN** a container target is launched on macOS and the `ext+container` handler does not take effect
- **THEN** container targets are hidden on macOS with a logged reason instead of being offered

### Requirement: macOS packaging and local signing
The macOS bundle SHALL declare `CFBundleURLTypes` for `http` and `https` only, `LSUIElement` true, a minimum system version of 13.0, and a version string taken from the project version. Nested code SHALL be signed before the bundle, and the signature of each nested item and of the bundle SHALL be verified individually. For the owner-machine delivery scope the signature MAY be ad-hoc or the local Apple Development identity; Developer ID signing, notarization, Intel and universal builds are out of scope for this capability. The bundle SHALL install to `~/Applications/Lane.app` without elevated privileges and SHALL NOT carry the quarantine attribute after install.

#### Scenario: Declared URL schemes
- **WHEN** the built `Info.plist` is read
- **THEN** it declares exactly the `http` and `https` schemes and no `mailto`, `file` or document types

#### Scenario: Signature verifies
- **WHEN** `codesign --verify --strict` runs on every nested Mach-O and then on the installed bundle
- **THEN** each exits zero

#### Scenario: Install without sudo
- **WHEN** the install script runs as the normal user
- **THEN** `Lane.app` is present in `~/Applications`, no privileged command was used, and `xattr -p com.apple.quarantine` reports the attribute absent

### Requirement: macOS native verification gate
A macOS change to the application SHALL NOT be reported done on `ctest` alone. The gate SHALL run the installed `Lane.app` on real hardware and observe: LaunchServices delivery of a real URL to a cold and a running instance, a picker screenshot with nonzero hit rectangles, a login-item status round trip, a notification authorization outcome, the typed-key check, and the default-handler button outcome. Steps that need a second display are recorded as not exercised when none is attached. The gate SHALL deliver test URLs by naming Lane (`open -b app.lane.Lane <url>`) and SHALL NOT change the user's default handlers; the handler changes only when the owner presses the in-app button. Only the scripted part of the gate SHALL be re-run unattended.

#### Scenario: End-to-end link delivery
- **WHEN** `open -b app.lane.Lane https://example.com` runs against the installed bundle with Lane not running, then again while it runs
- **THEN** both clicks reach the picker (or the matching rule) exactly once and the evidence includes a screenshot and the hit-rectangle assertion

#### Scenario: Handlers untouched by the gate
- **WHEN** the gate completes without the owner pressing the default-browser button
- **THEN** the system http and https handler is the same application as before the gate

#### Scenario: Second display not attached
- **WHEN** the verification machine has one display
- **THEN** the report lists the second-display scenarios as not exercised
