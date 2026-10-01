# Design: Browser Picker Supremacy

## Context

Lane is a resident Qt 6 / Kirigami Wayland link router running on Linux. Core link-processing runs through `pipeline.cpp` (unwrapping Outlook links and expanding shorteners) and `router.cpp` (evaluating explicit rules, path-scoped memory, and defaults). User settings are persisted atomically in `~/.config/lane/config.json`.

See `proposal.md` for motivation and high-level requirements.

## Goals / Non-Goals

**Goals:**
- Eliminate tracking telemetry in clicked URLs via deterministic in-memory parameter stripping.
- Provide seamless desktop context awareness using KDE Plasma Activities without IPC bottlenecks.
- Defend Lane's default browser registration against hostile package updates.
- Provide a zero-friction 1-key flick (`Alt+P`) to open any link in an isolated private window.
- Ensure the picker never dead-ends a user during typos or initial zero-target startup.

**Non-Goals:**
- Arbitrary JavaScript/Lua scripting runtimes (violates simple/predictable core ethos).
- Web browser extensions (unneeded maintenance burden across Chrome Web Store and AMO).
- Non-Plasma desktop activity synchronization (GNOME and wlroots lack standardized Activity concepts).

## Decisions

### 1. Clean Links: High-performance `QUrlQuery` parsing in `pipeline.cpp`
- **Decision**: Implement `cleanTrackingParams(const QUrl &url) -> QUrl` as an early step in `pipeline.cpp`, preceding unshorten and rule matching.
- **Rationale**: Tracking parameters (`utm_*`, `fbclid`, `gclid`, etc.) alter the query string without affecting the semantic target page. Stripping them early simplifies downstream regex rule authoring and remembered path lookups.
- **Keys stripped**: `utm_source`, `utm_medium`, `utm_campaign`, `utm_term`, `utm_content`, `fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `si`, `igshid`.
- **Implementation**: Uses `QUrlQuery` with a `static const QSet<QStringView>` allowlist of banned keys. Avoids full regex passes over entire URL strings.

### 2. KActivities: Signal-driven cached state, zero IPC on click path
- **Decision**: Connect `KActivities::Consumer::currentActivityChanged` to update an in-memory `QString m_currentActivityId` and `QString m_currentActivityName` in `Controller`.
- **Rationale**: Polling D-Bus on link-click introduces perceptible latency. Caching the active activity ID makes rule evaluation in `matcher.cpp` a simple string comparison taking <1 microsecond.
- **Fallback**: On non-Plasma desktops or minimal containers where KActivities daemon is not running, the consumer reports an empty/default ID, gracefully skipping activity-specific rules.

### 3. Default Browser Watchdog: Debounced file watching on `mimeapps.list`
- **Decision**: Monitor `~/.config/mimeapps.list` using `QFileSystemWatcher`, debounced with a 2-second single-shot timer.
- **Rationale**: Inotify triggers multiple times during package manager operations or browser profile writes. Debouncing ensures the watchdog checks only when the file write has settled.
- **Notification**: Employs `KNotification` with the configured `notifyrc` event, presenting an action button "Restore Lane" wired to `Controller::setDefaultBrowser()`.

### 4. Alt+P Private Diversion: Target model pairing and engine flag fallback
- **Decision**: Extend `TargetModel` with `getPrivateTarget(const QString &targetId) -> Target`.
- **Rationale**: For discovered targets with paired incognito rows (e.g. `zen-private`, `brave-incognito`), Lane switches the launch target ID directly. For generic/custom browsers, it appends the engine-specific flag (`--private-window` for Gecko, `--incognito` for Chromium).
- **Keybinding**: Layer-shell picker binds `Shortcut { sequence: "Alt+P" }` to invoke `controller.launchPrivate(pickerModel.selectedTargetId)`.

### 5. Schema & Configuration Persistence
- Add the following keys to `Config` (`src/core/types.h`) and `docs/config.schema.json`:
  - `stripTrackingParams`: `bool` (default: `true`)
  - `watchdogEnabled`: `bool` (default: `true`)
  - `activityDefaults`: `QHash<QString, QString>` (Activity ID -> Target ID)

## Risks / Trade-offs

- **Risk**: A website legitimately relies on `id` or query parameters that match tracking prefixes.
  - **Mitigation**: The banned list contains only explicit tracking-specific tokens (`utm_*`, `*clid`, `fbclid`). General query parameters (`q`, `id`, `page`, `ref`) are strictly preserved.
- **Risk**: Build failure on non-KDE environments without `kf6-kactivities-devel`.
  - **Mitigation**: Use `find_package(KF6Activities)` conditionally in CMake; compile out activity matching if the library is unavailable.
