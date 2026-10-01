## ADDED Requirements

### Requirement: Strip tracking parameters
When tracking parameter stripping is enabled, Lane SHALL remove well-known surveillance and campaign query parameters (`utm_source`, `utm_medium`, `utm_campaign`, `utm_term`, `utm_content`, `fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `si`, `igshid`) from HTTP and HTTPS URLs before matching rules or routing to a destination. Non-tracking query parameters SHALL remain unmodified in their original order.

#### Scenario: Stripping campaign tracking parameters
- **WHEN** Lane processes a URL containing `https://example.com/article?utm_source=twitter&id=42&fbclid=xyz`
- **AND** tracking parameter stripping is enabled
- **THEN** Lane normalizes the URL to `https://example.com/article?id=42` before route evaluation

#### Scenario: Preservation of tracking parameters when disabled
- **WHEN** Lane processes a URL containing `https://example.com/?utm_source=newsletter`
- **AND** tracking parameter stripping is disabled in configuration
- **THEN** the URL is routed without parameter modification

---

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

### Requirement: Open in private window shortcut
While the picker overlay is displayed, pressing `Alt+P` SHALL immediately divert the active URL to the selected destination's private or incognito profile, bypassing normal non-private profile launching.

#### Scenario: Private window launch from picker
- **WHEN** the picker displays with a browser profile selected (e.g., Zen Default)
- **AND** the user presses `Alt+P`
- **THEN** Lane closes the picker and launches the private counterpart (e.g., Zen Private Window) with the active URL

#### Scenario: Alt+P on custom target without explicit private counterpart
- **WHEN** the selected target is a generic browser executable without an auto-discovered private counterpart
- **AND** the user presses `Alt+P`
- **THEN** Lane appends the standard private flag (`--incognito` for Chromium, `--private-window` for Gecko) to the launch arguments

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
