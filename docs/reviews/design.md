# Lane Design & UX Review, round 4 (Browser Picker Supremacy)

Scope: Interaction design, keyboard ergonomics, accessibility, and visual affordances across `docs/designs/linux-browser-picker-supremacy.md`.
Mode: degraded — self-run 5-lens review by assistant, all independent channels quota-dead until next reset window.

## Verdict

**APPROVE WITH UI POLISH REQUIREMENTS.**

The proposed additions preserve Lane's core design ethos: minimal visual noise, instant keyboard response, and no modal nag screens. Five specific UX requirements must guide implementation:

## UX Findings & Interaction Polish

### 1. Picker Footer Affordance (`Alt+P`)
- **Current Footer**: `1-8 Pick • Alt+A Always • ,/. Scope • Esc Cancel`
- **Updated Footer**: `1-8 Pick • Alt+P Private • Alt+A Always • ,/. Scope • Esc Cancel`
- **Ergonomics**: `Alt+P` is a standard mnemonic for "Private" across modern browsers. It does not conflict with digit selection (1-8), scope navigation (`,`, `.`), or rule creation (`Alt+A`).
- **Feedback**: When `Alt+P` is pressed, the picker window should close immediately, matching the instant dispatch of `Enter` or digit selection.

### 2. Clean Links: Single-switch simplicity in Preferences
- **Guideline**: Do NOT expose a multi-row table of regexes or query parameters in the Preferences UI. That belongs in advanced config JSON.
- **FormCard Layout**:
  - Title: `Clean links before routing`
  - Description: `Strips tracking parameters (utm_*, fbclid, gclid, etc.) from opened links.`
  - Type: Standard Kirigami `FormSwitchDelegate`
  - Default: `true` (ON). Privacy should be safe by default.

### 3. Watchdog Notification: Polite, Actionable, Non-Intrusive
- **Tone**: Defensive, not needy.
- **Copy**:
  - Title: `Default browser changed`
  - Body: `Another application set %1 as the default browser.`
  - Action 1: `Restore Lane` (Calls `setDefaultBrowser()`)
  - Action 2: `Ignore` / `Don't ask again` (Sets `watchdogEnabled: false`)
- **Behavior**: Standard transient desktop notification via `KNotification`. Never display a modal dialog. If the user ignores it, it fades silently into the Plasma notification history.

### 4. Empty Picker State: Two-Phase Recovery
- **State A (Search Filter Miss)**:
  - Header: `No matching destinations`
  - Hint: `Press Esc to clear filter`
  - Behavior: If `filterText.length > 0`, pressing `Esc` clears `filterText` without dismissing the picker. Pressing `Esc` again dismisses the picker. This prevents accidental overlay dismissal when mistyping a search query.
- **State B (Zero Discovered Browsers)**:
  - Header: `No browsers found`
  - Hint: `Press Alt+S for Settings or Alt+R to Rediscover`
  - Shortcuts: `Alt+S` invokes `controller.openSettings()`; `Alt+R` invokes `controller.rediscover()`.

### 5. Screen Reader Accessibility (Orca)
- All new footer elements and empty-state placeholders must declare explicit `Accessible.name` and `Accessible.description` properties.
