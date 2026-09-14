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
The picker SHALL be a Wayland layer-shell overlay sized to the active screen, with exclusive keyboard and a 440px card. Type SHALL filter; 1-8 SHALL pick when the filter is empty; Enter, Esc, Alt+A SHALL confirm, cancel, and remember.

#### Scenario: Number pick
- **WHEN** the filter is empty and the user presses 1 through 8
- **THEN** Lane opens the corresponding row

#### Scenario: Remember with Alt+A
- **WHEN** the user presses Alt+A
- **THEN** Lane remembers the destination for the current path scope and opens it

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
