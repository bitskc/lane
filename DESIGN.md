# DESIGN.md

## Product

Tern is a default-browser proxy. The product is what happens in the 200ms after a click: either nothing visible (correct app already opened) or a quiet overlay that can be dismissed from the keyboard.

## Feel (Mac bar)

Choosy / Velja / Opener, not a settings dialog:

- Resident process. Cold-start Qt is too slow for a picker.
- Layer-shell overlay, exclusive keyboard, blur, Plasma accent.
- Compact card (~440px). Six visible rows, then scroll. Footer never clips.
- Checkbox for “Always for this site”, not a switch.
- One decision per unknown site, then memory.
- PWAs rank above browser windows. GitHub BITS is an app, not a tab.
- Settings is a persistent two-pane window. No hamburger.

## Security

- Open only `http` and `https`. `file:`, `javascript:`, `data:`, credentials-in-URL are refused.
- Display and clipboard never show userinfo.
- Unshorten: known hosts only, HEAD, no cookies, no private/link-local/metadata destinations.
- Outlook unwrap refuses nested non-http(s) URLs.
- Custom handlers are argv (`QProcess::splitCommand`), never a shell. Interpreters (`bash -c`, `python -c`, …) are rejected.
- Regex rules are length-capped and anchored.
- Default browser is opt-in, http/https only. mailto and PDF are not stolen.
- “Always for this site” is not stored for private/local hosts.

## Decision order

1. Explicit rules (first match) — open immediately, no hold
2. Remembered destination key (path-scoped, longest match wins)
3. Unique PWA scope, if Prefer PWAs is on (origin-wide PWA scopes do not auto-open tenant paths)
4. Picker policy (`no-rule` default)
5. Default target

Rules beat convenience. A work GitHub rule will beat the GitHub PWA.

### Hold on silent opens

Remembered, PWA, and default opens are *silent convenience* opens. When
`holdAutoOpen` is on (default), Tern shows a compact hold HUD for `holdMs`
(default 1600 ms) before launching. Enter launches now; Esc or Space cancels
the hold and shows the picker. A new URL arriving during the hold cancels
the previous hold. Rule matches are never held — the user wrote the rule.

## Non-goals (v1)

- Lua
- Firefox containers
- PDF / mailto handlers
