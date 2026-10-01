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
Silent opens (remembered, PWA, default) SHALL show a hold HUD for `holdMs` unless `holdAutoOpen` is off.

#### Scenario: Hold cancels
- **WHEN** a silent open is held and the user presses Esc or Space
- **THEN** Lane cancels the launch and shows the picker

#### Scenario: Hold disabled
- **WHEN** `holdAutoOpen` is off
- **THEN** Lane opens immediately with no hold HUD

### Requirement: Picker overlay
The picker SHALL be a Wayland layer-shell overlay sized to the active screen, with exclusive keyboard and a 440px card. Type SHALL filter; 1-8 SHALL pick when the filter is empty; Enter, Esc, Alt+A, and Alt+P SHALL confirm, cancel, remember, and open in a private window.

#### Scenario: Number pick
- **WHEN** the filter is empty and the user presses 1 through 8
- **THEN** Lane opens the corresponding row

#### Scenario: Remember with Alt+A
- **WHEN** the user presses Alt+A
- **THEN** Lane remembers the destination for the current path scope and opens it

#### Scenario: Private window with Alt+P
- **WHEN** the user highlights a browser destination and presses Alt+P
- **THEN** Lane resolves the destination's private counterpart and launches it immediately without saving to remembered destinations

---

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
Lane SHALL open only http and https; custom handlers SHALL run as argv, never through a shell.

#### Scenario: Refuse file URL
- **WHEN** Lane receives a `file:` or `javascript:` URL
- **THEN** it refuses to open it

#### Scenario: Shell rejected
- **WHEN** a custom handler uses `bash -c` or similar
- **THEN** Lane rejects it

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
