## MODIFIED Requirements

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

## ADDED Requirements

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
