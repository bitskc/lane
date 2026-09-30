## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Strip tracking parameters
When tracking parameter stripping is enabled (`stripTrackingParams: true`), Lane SHALL remove well-known surveillance and campaign query parameters from HTTP and HTTPS URLs before matching rules or routing to a destination. Non-tracking query parameters SHALL remain unmodified in their exact byte order and encoding. The rest of the URL (scheme, host, path, fragment) SHALL NOT be re-encoded or altered.

The stripping rule SHALL match:
1. Any query parameter whose percent-decoded key starts with `utm_` (case-insensitive).
2. Known global tracker keys (case-insensitive): `fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `mc_cid`, `mkt_tok`, `_ga`, `_gl`, `dclid`, `yclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`.
3. Host-scoped tracker keys: `si` SHALL be stripped only when the host is `youtube.com`, `youtu.be`, `music.youtube.com`, or `open.spotify.com`.

#### Scenario: Campaign tracking parameters stripped
- **WHEN** Lane processes a URL containing `https://example.com/article?utm_source=twitter&id=42&fbclid=xyz`
- **AND** `stripTrackingParams` is `true`
- **THEN** Lane sets `matchUrl` and `openUrl` to `https://example.com/article?id=42`

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
