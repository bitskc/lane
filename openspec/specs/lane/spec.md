# lane Specification

## Purpose
Lane is a Plasma-native default-browser proxy: it discovers browsers, profiles, PWAs, and containers, routes links through rules and remembered destinations, and shows a picker overlay or hold HUD before opening.

## Requirements

### Requirement: Discover browser profiles
Lane SHALL discover Gecko and Chromium profiles from XDG desktop files plus `profiles.ini` / `Local State`.

#### Scenario: Gecko profile discovery
- **WHEN** a Gecko browser (Firefox, Zen, LibreWolf, Floorp, Waterfox) is installed with a `profiles.ini`
- **THEN** Lane lists each profile as a destination

#### Scenario: Chromium profile discovery
- **WHEN** a Chromium browser (Brave, Chrome, Edge, Vivaldi, Opera) has profiles in `Local State`
- **THEN** Lane lists each profile as a destination

### Requirement: Discover PWAs
Lane SHALL discover firefoxpwa sites; the desktop `Name=` SHALL win over the manifest name.

#### Scenario: PWA name precedence
- **WHEN** a firefoxpwa site has both a desktop entry name and a manifest name
- **THEN** Lane uses the desktop `Name=`

### Requirement: Discover containers
Firefox/Zen containers (contextual identities) SHALL be destinations where `containers.json` exists and a protocol-handler extension is present.

#### Scenario: Container target
- **WHEN** a profile has containers and a protocol-handler extension
- **THEN** Lane lists each container as a destination and launches it via `ext+container:` argv

### Requirement: Route links
Lane SHALL route in order: explicit rules, path-scoped remembered destination, unique PWA scope, picker policy, default target.

#### Scenario: Rule wins
- **WHEN** a URL matches an explicit rule
- **THEN** Lane opens it in the rule target immediately, with no hold

#### Scenario: Path-scoped memory
- **WHEN** the user remembers a destination for `github.com/bitskc`
- **THEN** Lane opens `github.com/bitskc/*` there, and other `github.com` paths still ask

### Requirement: Hold HUD on silent opens
Silent opens (remembered, PWA, default) SHALL show a hold HUD for `holdMs` unless `holdAutoOpen` is off, rendered with Qt Quick Controls.

#### Scenario: Hold cancels
- **WHEN** a silent open is held and the user presses Esc or Space
- **THEN** Lane cancels the launch and shows the picker

#### Scenario: Hold disabled
- **WHEN** `holdAutoOpen` is off
- **THEN** Lane opens immediately with no hold HUD

### Requirement: Picker overlay
The picker SHALL be a Wayland layer-shell overlay sized to the active screen, with exclusive keyboard and a 440px card. Type SHALL filter; 1-8 SHALL pick when the filter is empty; Enter, Esc, Alt+A, and Alt+P SHALL confirm, cancel, remember, and open in a private window. The overlay SHALL render using Qt Quick Controls with no Kirigami runtime dependency.

#### Scenario: Number pick
- **WHEN** the filter is empty and the user presses 1 through 8
- **THEN** Lane opens the corresponding row

#### Scenario: Remember with Alt+A
- **WHEN** the user presses Alt+A
- **THEN** Lane toggles Always for the current path scope
- **AND** if Always is enabled when the user confirms a destination, Lane remembers that destination for the current path scope and opens it

#### Scenario: Private window with Alt+P
- **WHEN** the user highlights a browser destination and presses Alt+P
- **THEN** Lane resolves the destination's private counterpart and launches it immediately without saving to remembered destinations

#### Scenario: Wayland exclusive input
- **WHEN** the picker shows on a Wayland compositor with layer-shell
- **THEN** it takes exclusive keyboard focus and dismisses on Esc

### Requirement: Private profiles stay out of the picker
Private/incognito/Tor profiles SHALL stay available to rules but SHALL NOT clutter the picker.

#### Scenario: Incognito hidden
- **WHEN** a profile is private or incognito
- **THEN** it is not ranked in the picker but still matches explicit rules

### Requirement: Default browser is opt-in
Lane SHALL NOT take the default browser except through the settings button.

#### Scenario: Settings button
- **WHEN** the user clicks "Use Lane as default browser"
- **THEN** Lane registers as the default for http and https only

### Requirement: CLI without daemon
`lane --list` and `lane --explain URL` SHALL work without the unique daemon.

#### Scenario: Explain
- **WHEN** the user runs `lane --explain https://example.com`
- **THEN** Lane prints the routing decision and exits

### Requirement: Rediscover and configure
`lane --rediscover` SHALL rescan targets in the running daemon; `lane --configure` SHALL open settings.

#### Scenario: Rediscover
- **WHEN** the user runs `lane --rediscover`
- **THEN** the running daemon reloads config and rescans targets

### Requirement: Update checker
Lane SHALL check GitHub releases for updates and SHALL show the result in settings.

#### Scenario: Update available
- **WHEN** a newer release exists
- **THEN** the Overview page shows the new version

### Requirement: Legacy config migration
Lane SHALL migrate `~/.config/tern` config to `~/.config/lane` on first run.

#### Scenario: First run after rename
- **WHEN** Lane starts and finds `~/.config/tern/config.json` but no `~/.config/lane`
- **THEN** Lane copies the config and uses it

### Requirement: Security
Lane SHALL open only http and https; custom handlers SHALL run as argv, never through a shell. Lane SHALL judge a URL by a strict parse that never repairs malformed input into a different origin, SHALL classify a host as private or local on its canonical form, SHALL read a link document only when it is a regular file, and SHALL expand a URL placeholder only where the placeholder is a whole token.

#### Scenario: Refuse file URL
- **WHEN** Lane receives a `file:` or `javascript:` URL
- **THEN** it refuses to open it

#### Scenario: Shell rejected
- **WHEN** a custom handler uses `bash -c` or similar
- **THEN** Lane rejects it

#### Scenario: Windows interpreter rejected by name
- **WHEN** a custom handler's executable is `powershell.exe`, `pwsh.exe`, `python.exe`, `wscript.exe`, `cscript.exe` or `mshta.exe`
- **THEN** Lane rejects it, and an executable such as `firefox.exe` is still accepted

#### Scenario: Malformed URL is not repaired into another origin
- **WHEN** Lane receives `https://example.com..` or `http://example.com:99999/` as a click
- **THEN** after normalization the URL is still unsafe, and Lane does not open a rewritten host such as `https`

#### Scenario: Well-formed URLs still open
- **WHEN** Lane receives a URL with a literal space, `%20`, a stray `%`, an IDN host, a unicode path, an IPv6 literal or a port
- **THEN** it normalizes to a fully encoded URL that is safe to open, and `%20` is not decoded to a space

#### Scenario: Control and line-separator characters are refused
- **WHEN** a click contains CR, LF, NUL, U+2028 or U+2029
- **THEN** normalization leaves it unencoded and it is unsafe, and the sanitized form is empty

#### Scenario: Private host with a trailing dot
- **WHEN** a redirect target host is `localhost.`, `127.0.0.1.`, `2130706433.` or `metadata.google.internal.`
- **THEN** Lane treats it as private and does not follow the unshortened link
- **AND** `1.1.1.1.` is still public

#### Scenario: Private host in an IPv6 transition address
- **WHEN** a redirect target host is a NAT64 address such as `[64:ff9b::7f00:1]` or a 6to4 address such as `[2002:7f00:1::]` that embeds a private or loopback IPv4
- **THEN** Lane treats it as private
- **AND** `[64:ff9b::101:101]` (public 1.1.1.1) is not treated as private

#### Scenario: Link document is a decoy
- **WHEN** a `.webloc` file has a `URL` key and a separate note string containing `URL=https://evil.example/`
- **THEN** Lane routes the `URL` key's value and not the note

#### Scenario: Link document is not a regular file
- **WHEN** a `.webloc` or `.url` path is a fifo or a symlink to `/dev/zero`
- **THEN** Lane ignores it without blocking

#### Scenario: Link document entities
- **WHEN** a `.webloc` URL contains `&amp;`
- **THEN** Lane routes the URL with `&`

#### Scenario: Placeholder inside a longer token
- **WHEN** a custom argument is `--user-data-dir=%user`
- **THEN** it is passed unchanged, while `%u` and `--url=%u` expand to the URL

### Requirement: Strip tracking parameters
When tracking parameter stripping is enabled (`stripTrackingParams: true`), Lane SHALL remove well-known surveillance and campaign query parameters from HTTP and HTTPS URLs before matching rules or routing to a destination. Non-tracking query parameters SHALL remain unmodified in their exact byte order and encoding. The rest of the URL (scheme, host, path, fragment) SHALL NOT be re-encoded or altered.

The stripping rule SHALL match:
1. Any query parameter whose percent-decoded key starts with `utm_` (case-insensitive).
2. Known global tracker keys (case-insensitive): `fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `mc_cid`, `mkt_tok`, `_ga`, `_gl`, `dclid`, `yclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`, `igshid`; plus any key with prefix `hsa_` (case-insensitive). `ref` is NOT a tracker and is never stripped.
3. Host-scoped tracker keys: `si` SHALL be stripped only when the host is `youtube.com`, `youtu.be`, `music.youtube.com`, or `open.spotify.com`.

#### Scenario: Campaign tracking parameters stripped
- **WHEN** Lane processes a URL containing `https://example.com/article?utm_source=twitter&id=42&fbclid=xyz`
- **AND** `stripTrackingParams` is `true`
- **THEN** Lane sets `matchUrl` to `https://example.com/article?id=42`; because the link is not a wrapped/shortened URL, `openUrl` equals that same clean `matchUrl`

#### Scenario: Stripping must not re-encode parameters or path
- **WHEN** Lane processes `https://bücher.de/café?token=YWJj==&utm_source=x#section?utm_source=y`
- **AND** `stripTrackingParams` is `true`
- **THEN** Lane produces `https://bücher.de/café?token=YWJj==#section?utm_source=y`
- **AND** the host is NOT converted to punycode, the path `/café` is NOT re-encoded, and `token=YWJj==` does NOT have `=` replaced by `%3D`

#### Scenario: Host-scoped tracker parameter (si)
- **WHEN** Lane processes `https://youtu.be/dQw4w9WgXcQ?si=abcdef123456&t=10`
- **THEN** Lane normalizes the URL to `https://youtu.be/dQw4w9WgXcQ?t=10`
- **AND WHEN** Lane processes `https://example.com/?si=search_idx&lang=en`
- **THEN** `si` is preserved as `https://example.com/?si=search_idx&lang=en`

#### Scenario: Case-insensitive and prefix utm stripping
- **WHEN** Lane processes `https://example.com/?UTM_Source=twitter&utm_id=GA123&q=query`
- **THEN** Lane normalizes the URL to `https://example.com/?q=query`

#### Scenario: Non-HTTP schemes ignored
- **WHEN** Lane processes a non-HTTP URL like `mailto:test@example.com?subject=Hello&utm_source=test`
- **THEN** the URL is left unmodified

#### Scenario: Safelinks (O365) wrapped links
- **WHEN** Lane processes an O365-wrapped link containing tracking parameters in the inner target
- **AND** `openUnwrapped` is `false`
- **THEN** trackers are stripped from `matchUrl` for rule evaluation, but `openUrl` remains the original O365 wrapper

---

### Requirement: Open in private window shortcut
While the picker overlay is displayed and a target is selected, pressing `Alt+P` SHALL resolve that target's exact private counterpart and launch it without saving to remembered destinations. If the target has no private counterpart, Lane SHALL fail closed.

#### Scenario: Alt+P pairs to exact profile counterpart
- **WHEN** the picker has `Firefox · Work` (`browser:firefox:work`) highlighted
- **AND** the host also has `Firefox · Personal` and `Firefox · Work (Private)`
- **AND** the user presses `Alt+P`
- **THEN** Lane launches `Firefox · Work (Private)` (`browser:firefox:work:private`)
- **AND** Lane does NOT launch `Personal (Private)` or Brave Tor

#### Scenario: Alt+P on container row maps to profile private window
- **WHEN** the picker has a container row highlighted (`browser:firefox:default:container:1`)
- **AND** the user presses `Alt+P`
- **THEN** Lane resolves the base profile ID and launches `browser:firefox:default:private`

#### Scenario: Alt+P on unsupported target fails closed
- **WHEN** the picker has a PWA, custom action, or generic target highlighted that lacks a private counterpart
- **AND** the user presses `Alt+P`
- **THEN** Lane does NOT launch a normal window
- **AND** the picker remains open and displays an inline notice "No private mode for <target>"

#### Scenario: Alt+P ephemeral privacy invariant
- **WHEN** a target is launched via `Alt+P`
- **THEN** Lane does NOT write to `config.remembered` regardless of `alwaysForHost` state
- **AND** the launch toast/notification does NOT disclose the host name in notification history

#### Scenario: Alt+P on a hidden target resolves its private counterpart anyway
- **WHEN** the highlighted target is marked `hidden` but still appears in the picker (hidden targets stay reachable via Alt+P)
- **AND** the user presses `Alt+P`
- **THEN** Lane resolves the private counterpart by exact id regardless of the counterpart's `hidden` flag, and launches it

#### Scenario: Alt+P fail-closed notice clears on selection or filter change
- **WHEN** the "No private mode for <target>" notice is shown
- **AND** the user moves the selection to a different target OR types in the filter box
- **THEN** the notice is cleared so a stale private-mode warning never sits under a different, private-capable target

### Requirement: Plasma Activity-scoped rules and defaults
Lane SHALL recognize the currently active KDE Plasma Activity. A rule MAY specify an `activity` filter matching either the Activity ID or Activity Name. When set, the rule SHALL only match when the specified Activity is active. In addition, Lane SHALL support per-Activity default destinations that override the global default when no explicit rule or path memory matches.

#### Scenario: Activity-specific rule match
- **WHEN** the current Plasma Activity is "Work"
- **AND** a rule specifies `activity: "Work"` and destination `Brave Work`
- **THEN** matching URLs route to `Brave Work`

#### Scenario: Activity rule mismatch
- **WHEN** the current Plasma Activity is "Personal"
- **AND** a rule specifies `activity: "Work"`
- **THEN** the rule does not match, and fallback evaluation continues

#### Scenario: Activity default destination fallback
- **WHEN** no rule or remembered path matches the incoming URL
- **AND** the current Activity has an assigned default destination in `activityDefaults`
- **THEN** Lane routes to the Activity's default destination instead of the global default

---

### Requirement: Default browser watchdog and restore notification
Lane SHALL periodically verify whether it remains the registered default handler for `x-scheme-handler/http` and `x-scheme-handler/https` in user associations. If an external process changes the association away from Lane, Lane SHALL issue a desktop notification identifying the new default browser and providing an action to restore Lane as the default browser.

#### Scenario: External takeover detection
- **WHEN** an external browser update sets `mimeapps.list` default for `x-scheme-handler/http` to another browser
- **AND** Lane's watchdog is enabled
- **THEN** Lane presents a notification with the message identifying the current default and an action button labeled "Restore Lane"

#### Scenario: User restores default via notification
- **WHEN** the user activates the "Restore Lane" action on the watchdog notification
- **THEN** Lane re-registers itself as the default handler for http and https

---

### Requirement: Empty picker recovery and guidance
When the picker overlay contains zero selectable destinations, it SHALL present explicit actionable feedback distinguishing between a search filter miss and an empty target discovery state.

#### Scenario: Search filter produces no matches
- **WHEN** the user types a search query that matches zero destinations
- **THEN** the picker displays "No matching destinations — press Esc or Backspace to clear"
- **AND** pressing `Esc` clears the search text instead of dismissing the picker

#### Scenario: Zero discovered browsers on system
- **WHEN** target discovery finds zero valid browser targets on the host
- **THEN** the picker displays "No browsers discovered — press Alt+S for Settings or Alt+R to Rediscover"
- **AND** pressing `Alt+S` opens the Settings window

### Requirement: System tray presence
Lane SHALL show a tray icon on platforms with a status-icon mechanism; activating it SHALL open the Settings window, and its context menu SHALL offer Settings and Rediscover browsers. On Linux the tray SHALL NOT offer Quit (the resident daemon owns link routing). On macOS the status-item menu SHALL additionally offer Quit Lane, because LaunchServices relaunches Lane on the next link click while Lane is the registered handler and a status item without Quit is not discoverable there.

#### Scenario: Tray activation
- **WHEN** the user clicks Lane's tray icon on Linux
- **THEN** the Settings window opens or raises

#### Scenario: Status-item activation on macOS
- **WHEN** the user clicks Lane's status item on macOS
- **THEN** the status-item menu opens, and choosing Settings opens or raises the Settings window

#### Scenario: Tray menu
- **WHEN** the user opens the tray context menu on Linux
- **THEN** it offers "Settings" and "Rediscover browsers" and no Quit entry

#### Scenario: Status-item menu on macOS
- **WHEN** the user opens the status-item menu on macOS
- **THEN** it offers "Settings", "Rediscover browsers" and "Quit Lane"

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

### Requirement: Settings window
The settings window SHALL present the same page model (Overview, Browsers & apps, Rules, Preferences) implemented with Qt Quick Controls and grouped card delegates inside scrollable pages, without Kirigami or kirigami-addons at runtime. Its section navigation SHALL be reachable and operable by keyboard alone, and its lists SHALL keep their rows alive across unrelated settings changes.

#### Scenario: Drag reorder persists
- **WHEN** the user drags a browser row to a new position in Browsers & apps while not filtering
- **THEN** `targetOrder` updates in config.json and the picker order follows

#### Scenario: Private windows are not orderable
- **WHEN** the Private windows section is shown in Browsers & apps
- **THEN** its rows show no drag handle and cannot be reordered

#### Scenario: Keyboard section navigation
- **WHEN** the user presses Tab until the section list has focus and then presses Down and Enter
- **THEN** the next section becomes current and its page is shown

#### Scenario: Section list exposes its state
- **WHEN** a screen reader inspects the section list
- **THEN** each entry has a name and a selected state, and the current entry is focusable

#### Scenario: Divider is visible
- **WHEN** the Settings window is shown
- **THEN** the divider between the section list and the page is one pixel wide

#### Scenario: Lists survive a settings change
- **WHEN** the user hides, renames or moves a target while another row is mid-drag-settle
- **THEN** unaffected rows are not destroyed and recreated, and the settle animation completes

#### Scenario: Two renames in one tick
- **WHEN** two rename edits finish in the same millisecond
- **THEN** both renames persist

### Requirement: Platform styling without KDE QML runtime
The UI SHALL use the Fusion Qt Quick Controls style for its widgets and SHALL take its colors, fonts, and icons from the platform theme; it SHALL NOT require any KDE QML module at runtime.

#### Scenario: Plasma appearance
- **WHEN** Lane runs on Plasma with the KDE platformtheme plugin
- **THEN** controls render in Fusion with the user's Plasma colors and fonts

#### Scenario: No KDE modules loaded
- **WHEN** Lane runs on any host without Kirigami installed
- **THEN** the UI renders fully usable with zero `org.kde` imports resolved at runtime

### Requirement: Single in-flight click
Lane SHALL resolve one click at a time. While a link is being resolved, Lane SHALL queue later opens, SHALL ignore pointer and keyboard input, and SHALL defer `--rediscover` and `--settings` until that click is applied. A D-Bus `Open` carrying several URIs SHALL give each URI the activation token it arrived with. When Lane is quitting, it SHALL NOT route, launch or drain queued links.

#### Scenario: Input during unshorten
- **WHEN** a shortened link is being resolved and the user presses Return on a still-visible picker
- **THEN** the previous picker does not launch and the new click is the one routed

#### Scenario: Quit during unshorten
- **WHEN** Lane is asked to quit while it is resolving a link
- **THEN** it does not open a window or launch a browser for that link

#### Scenario: SIGTERM
- **WHEN** the daemon receives SIGTERM
- **THEN** it exits through the normal shutdown path and runs the controller destructor

#### Scenario: Command during a click
- **WHEN** `lane --rediscover` reaches the daemon while a click is being resolved
- **THEN** discovery runs after that click is applied, not in the middle of routing it

#### Scenario: Several URIs in one Open
- **WHEN** a D-Bus `Open` carries three URIs and an activation token
- **THEN** each URI is opened with that token

#### Scenario: Queue overflow is visible
- **WHEN** more than four links arrive during one resolution
- **THEN** the oldest is dropped and a warning names it

### Requirement: Plist scan budget
Lane SHALL parse a macOS `Info.plist` within a fixed budget and SHALL drop a bundle that exceeds it instead of using a partial result. A binary plist converted for parsing SHALL be capped.

#### Scenario: Deeply nested plist
- **WHEN** an `Info.plist` nests dictionaries more than 32 levels deep
- **THEN** its bundle is not discovered, and a sibling bundle with a normal plist is

#### Scenario: Very wide plist
- **WHEN** an `Info.plist` has more than 20,000 elements
- **THEN** its bundle is not discovered

### Requirement: Default-handler state is derived from both schemes
Lane SHALL report that it handles only part of the links when exactly one of http and https resolves to Lane. It SHALL report it as the default only when both do, and SHALL report an unknown state when a query fails, never a partial one.

#### Scenario: Another browser owns both
- **WHEN** http and https both resolve to `firefox.desktop`
- **THEN** the state is unknown and the current handler is `firefox.desktop`

#### Scenario: Lane owns one
- **WHEN** http resolves to Lane and https resolves to `firefox.desktop`
- **THEN** the state is partial

#### Scenario: Query failure
- **WHEN** the handler query for either scheme fails
- **THEN** the state is not partial and Lane is not reported as the default

### Requirement: First-run onboarding opens Settings once
Lane SHALL open Settings for first-run onboarding at most once. An explicit Settings open SHALL consume the first-run state. On macOS, `--rediscover` and `--pick` SHALL NOT trigger onboarding.

#### Scenario: Settings opened explicitly on first run
- **WHEN** the user opens Settings before the first link launch completes
- **THEN** the later launch does not open Settings again

### Requirement: QML load failure is recoverable
When a Lane window's QML fails to load, Lane SHALL discard the failed engine so the next request to show that window retries.

#### Scenario: Retry after failure
- **WHEN** the picker QML fails to load once and is then fixed
- **THEN** the next picker request loads it without restarting Lane

### Requirement: Cross-thread discovery state
State read by Qt's image-loader thread SHALL be published as a complete value and SHALL NOT be mutated in place. Icons that call into the platform file-icon provider SHALL load on the GUI thread.

#### Scenario: Rediscover during icon load
- **WHEN** Rediscover runs while bundle icons are loading
- **THEN** no icon request observes a partially built path table

### Requirement: macOS key hints match the real shortcuts
On macOS the picker footer, empty-state hints and the accessible names and descriptions of the picker controls SHALL show the macOS key names for the shortcuts that actually fire, taken from one per-platform table that also drives the shortcuts, so hint and shortcut cannot drift. Linux strings SHALL be unchanged. Escape SHALL still clear a non-empty field before cancelling, and digit keys, Return and arrows SHALL behave as on Linux.

#### Scenario: Copy shortcut
- **WHEN** the picker has focus on macOS and the user presses Command+C
- **THEN** the current URL is copied and the footer and accessible description name Command+C, not Control+C

#### Scenario: Letter shortcuts with a focused text field
- **WHEN** the filter field has focus and the user presses each macOS letter shortcut shown in the footer
- **THEN** its action fires and no stray character appears in the field

#### Scenario: Accessible names follow the table
- **WHEN** an accessibility client reads the email or private-window control on macOS
- **THEN** the shortcut it announces is the same one shown in the footer

#### Scenario: Escape ladder
- **WHEN** the filter field is non-empty and the user presses Escape twice
- **THEN** the first press clears the field and the second cancels the picker

#### Scenario: Linux hints unchanged
- **WHEN** the picker runs on Linux
- **THEN** it shows the existing `alt+...` and `^C` strings

### Requirement: macOS Dock presence, quit and Settings window
On macOS Lane SHALL run as an accessory app with no Dock icon. The status-item Quit Lane entry and the standard Quit shortcut in the Settings, picker and Hold windows SHALL share one quit path: if a pick is pending in the picker or Hold, the request SHALL cancel that pick and Lane SHALL keep running; otherwise Lane SHALL quit. When no `config.json` exists at launch, Lane SHALL open the Settings Overview, except that a first launch caused by a link SHALL route that link first and open Settings only after the pick or launch completes. Closing the Settings window SHALL hide it and leave routing running. The status-item icon SHALL be a template image that follows light and dark appearance.

#### Scenario: Quit with nothing pending
- **WHEN** the user chooses Quit Lane from the status item and no pick is pending
- **THEN** Lane quits

#### Scenario: Quit with a pending pick
- **WHEN** a URL is pending in the picker or Hold and a quit is requested by shortcut or menu
- **THEN** that pick is cancelled and Lane keeps running

#### Scenario: Relaunch after quit
- **WHEN** Lane is the registered handler, has quit, and the user clicks a link
- **THEN** the system starts Lane and the link is routed once

#### Scenario: First launch
- **WHEN** Lane starts on macOS with no `config.json` and no link to route
- **THEN** the Settings Overview opens

#### Scenario: First launch caused by a link
- **WHEN** Lane starts on macOS with no `config.json` because the user clicked a link
- **THEN** the link is routed first and the Settings Overview opens only after that pick or launch completes

#### Scenario: Close Settings
- **WHEN** the user closes the Settings window
- **THEN** the window hides, Lane keeps running, and choosing Settings from the status item shows it again

#### Scenario: No Dock icon
- **WHEN** Lane is running
- **THEN** no Dock icon is shown

### Requirement: macOS default-handler states in Settings
The Overview page on macOS SHALL show the current http and https handler by application name, SHALL show distinct states for Registering, Registered, Declined, Partial and another copy of Lane, and SHALL never display Registered until both schemes re-query to the running Lane. When notifications are denied, Overview SHALL show the notifications-off banner with a button that opens system notification settings.

#### Scenario: State shown while the system prompt is open
- **WHEN** the user pressed the default-browser button and the system prompt has not been answered
- **THEN** Overview shows Registering and the button is disabled

#### Scenario: Current handler named
- **WHEN** another application is the handler
- **THEN** Overview names that application before any button press

### Requirement: macOS appearance and displays
On macOS the picker, Hold and Settings SHALL follow the system light or dark appearance live, SHALL disable Hold ring and dim animations when the system reduced-motion preference is on, and SHALL open the picker on the display containing the pointer even when the frontmost app is on another display.

#### Scenario: Appearance changes while open
- **WHEN** the system switches from light to dark while the picker is open
- **THEN** the picker recolors without restarting

#### Scenario: Reduced motion
- **WHEN** reduce motion is enabled in system settings
- **THEN** the Hold ring and the dim fade do not animate

#### Scenario: Pointer on the second display
- **WHERE** a second display is attached and the pointer is on it when a link opens the picker
- **THEN** the picker appears on that display
