# Changelog

All notable changes to Lane are documented here. The format follows
[Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/), and Lane
uses [Semantic Versioning](https://semver.org/): while the major
version is 0, minor releases may still contain breaking changes.

## [Unreleased]

### Added

- Browsers installed as Flatpaks (Zen, Firefox, LibreWolf, Floorp,
  Waterfox, Brave, Chrome, Chromium, Edge, Vivaldi, Opera, Thorium) are
  discovered correctly. Lane now finds their real profile store under
  `~/.var/app/<app-id>/...` instead of falling back to a fake default
  profile with no containers, and launches them with the Flatpak
  wrapper's own `run --branch=... --command=... <app-id>` arguments
  kept in front of its own `--profile`/`--new-tab` flags, instead of
  handing those flags to `flatpak` itself.

- `lane --list` includes a fourth `kind` column (`browser`, `container`,
  `pwa`, `action`, `app`) for agent inspection.

### Fixed

- A second link clicked while Lane was still asking the compositor for
  an activation token could open in the right browser but with the
  wrong URL (the newer click's). The URL is now captured when the
  routing decision is made and carried through the async launch, so
  target and URL can no longer be mixed between clicks.
- A queued click that decided to launch or hold left the picker
  showing the previous click's rows; picking one would then open the
  new URL in a stale destination. The picker now closes whenever a
  click's decision is not to pick.
- Removed the `recentTargetIds` and `toastMs` config keys. Both were
  written and loaded but never read, and `recentTargetIds` meant every
  link open rewrote the config file.
- `lane` invoked over D-Bus no longer guesses at argv[0] by matching
  the name "lane"; the program name is always stripped, so a renamed
  binary can no longer be mistaken for a URL.
- A burst of links arriving while Lane was busy expanding a short URL
  queued without bound and could open a train of stale windows later.
  The queue now keeps only the four most recent clicks.
- The hold duration range is now 400-5000 ms in all three places that
  define it: the config schema, the Preferences spinbox, and the
  settings setter (previously 400+, 400-5000, and 200-10000).
- Picker section order is fixed (Web apps, Containers, Browsers, …) so
  Firefox/Zen containers are not buried below every browser profile when
  `targetOrder` lists Brave/Firefox/Edge first.
- Opening Settings rescans installed browsers so the daemon is not stuck
  on a pre-upgrade or pre-install target list until Rediscover is clicked.

- Native browser desktop entries no longer leak their own Exec= flags
  into launch arguments. An entry like `Exec=/usr/bin/firefox
  --new-window %u` used to put `--new-window` in front of Lane's
  `--profile` flag, so the browser opened the profile directory as a URL.
  Only Flatpak entries keep their `flatpak run ... <app-id>` prefix now.
- Desktop entries whose Exec= line starts with `env VAR=...` (for example
  `Exec=env MOZ_X11=1 firefox %u`) are unwrapped to the real program at
  discovery instead of producing a target that silently failed every
  launch. Entries that unwrap to nothing are dropped.
- A hand-edited custom target can no longer reach a shell through
  `flatpak run --command=sh <app-id>`: flatpak launches now require the
  `run` subcommand and a real app id, reject a blocked interpreter in
  `--command`, and apply the same checks when exec is a symlink to
  flatpak under another name.
- Chromium profiles still listed in Local State after their directory was
  deleted no longer appear as dead targets.
- Quoted arguments in Exec= lines (like `--command="zen browser"`) are
  kept as a single argument instead of being split apart.
- Picker and hold overlay no longer require the optional layer-shell QML
  module; they failed to load entirely on systems without it.
- Default target buttons on the Browsers & apps page now show which
  destination is currently selected.
- Picker filter with no matches shows a "No matching destinations"
  message instead of an empty row, and Enter no longer tries to pick
  from an empty list.
- Picker card height counts only section headers visible in the eight-row
  viewport, not every section in the filtered model.
- Drag reorder on Browsers & apps clears its pending move id when a drop
  is cancelled, so the next drag does not move the wrong target.
- Browsers & apps search shows "No matches" when the filter hides every
  row. Section collapse controls, picker section headers, and destination
  ladder buttons now have accessibility names. Rename fields show a hover
  underline so they read as editable.

## [0.2.0] - 2026-09-12

### Added

- Firefox and Zen contextual identities (containers) show up as
  destinations, one row per container per profile, e.g.
  "Zen · Default · Work". Launch wraps the http(s) URL in an
  `ext+container:name=...&url=...` argument passed to the browser;
  Lane itself still only ever opens `http`/`https` links, and does not
  register `ext+container` as anything it handles. Containers only
  show up for profiles where Lane can tell a protocol-handler
  extension is installed (Open URL in Container, Default Container
  Handler, or similar); without one, the browser has nothing to act
  on an `ext+container` link with.
- Picker rows are grouped under section headers (Browsers, Containers,
  Web apps, Actions). Container rows show their container color.
  Unmapped colors fall back to a neutral dot instead of black.
- Browsers & apps settings page has a search field and collapsible
  sections with counts in each header. Private windows start collapsed.
- Rules page warns when a rule's destination no longer exists, and
  offers a button to clear remembered destinations pointing at targets
  that are gone. Dead remembered entries are also pruned when config
  reloads.
- Hold duration is adjustable in Preferences (0.4 to 5 seconds), not
  just on or off.
- Picker and hold overlays expose accessibility roles and names, so a
  screen reader announces each destination and its number shortcut.
- Settings has a new Information section at the bottom of Overview,
  showing the running version, the license (PolyForm Noncommercial
  1.0.0, with a link to the full text), and a link to the project
  page on GitHub.
- "Check for updates" button in that same section asks GitHub for the
  latest release and tells you whether you're current, whether a
  newer version is out (with a link to it), or that the check failed.
  It only runs when you press the button. Lane never checks in the
  background or on startup, and it never downloads or installs
  anything itself; at most it offers to open the release page in your
  browser.
- Test coverage for the update checker's response handling and for the
  unshorten gates. Each update-check failure cause is proven to map to
  its own message, and the unshorten safety checks are exercised.

### Changed

- Picker shows eight rows instead of six.
- Overlay tints the desktop instead of dimming it out. (Picker/hold
  no longer blur the whole screen.)
- README and PKGBUILD build dependency lists now include
  `kcolorscheme` and `kcrash`, which a clean configure actually needs.
- PKGBUILD fetches the GitHub release tarball with a real checksum,
  installs the license file, and runs tests with
  `QT_QPA_PLATFORM=offscreen`.
- CLI docs in README and AGENTS.md list every option the binary
  accepts, including `--version`, `--settings`, `-p`, and
  `--rediscover`.
- `docs/config.schema.json` documents `kind` and `browserName` on
  `customTargets` entries (written on save; `kind` is ignored on load).
- Copy link moved out of the picker's destination list and into a
  small control in the footer, next to the `esc` hint. It no longer
  consumes a row or a number shortcut; the underlying action and its
  Ctrl+C binding are unchanged.
- The hold bar is now opt-in. `holdAutoOpen` defaults to off; turn it
  on in Preferences if you want the pause before silent opens. The
  `holdMs` schema minimum is 400 ms to match the Preferences slider.
- The Always checkbox now names the target it pins to. The picker
  footer shows `,` and `.` hints for the destination ladder, and the
  settings sidebar navigation has accessibility labels.

### Fixed

- Picker and hold overlays failed to load on current Kirigami (6.28),
  because they set `borderColor`/`borderWidth` directly instead of the
  grouped `border.color`/`border.width` properties. This made link
  clicks bounce in the taskbar and then do nothing. Fixed in
  `Picker.qml` and `Hold.qml`.
- Firefox-family browsers (Firefox, Zen, LibreWolf, Floorp, Waterfox)
  were launched with `-P <internal name>`, and on Zen that internal
  name is often `Default Profile` or `Default (release)`, which don't
  round-trip cleanly as a launch argument. Lane now launches with
  `--profile <folder>`, using the profile's real directory instead.
  Discovery also picks whichever config folder actually holds a
  browser's `profiles.ini` (instead of guessing based on folder
  order), skips profiles whose folder is gone or was never opened,
  and no longer lists a browser twice when two `.desktop` files point
  at the same install. Zen's install-default profile is now labeled
  "Default" instead of its raw internal name.
- The picker bound a number shortcut for a ninth row it never drew,
  so pressing 9 could open a destination that was not visible. Number
  shortcuts now match the eight visible rows.
- A failed launch (missing browser, bad custom command) did nothing
  and looked like Lane was broken. Lane now shows a notification
  naming the destination.
- Notifications never appeared. None of the `KNotification` call sites
  set a component name, so Plasma looked for `Lane.notifyrc` instead of
  the installed `app.lane.Lane.notifyrc` and dropped every toast. This
  affected the existing "Opened in" notice as well as the new
  launch-failure notice.
- Two conflicting rules combined with "never show the picker" honored
  neither rule and fell through to the default target. First match now
  wins, matching the documented decision order.
- Blocked links (`javascript:`, `file:`, credentials in the URL) used
  to show a normal picker whose rows could not open anyway. Lane now
  explains that it refused the link.
- A rejected custom app command showed nothing and the row never
  appeared. The error now shows inline in the add form.
- Link unshortening followed only one redirect, so a chained shortener
  left rules and the picker looking at an intermediate domain. Lane
  now follows up to four hops and stops on a loop.
- Picker number shortcuts (1 through the number of visible rows) and
  the `,`/`.` destination-ladder keys did nothing: the filter field
  held keyboard focus and swallowed the plain keypress before the
  `Shortcut` bound to it ever fired, so pressing a digit just typed
  into the filter. Ctrl+C had the same problem. These are now handled
  directly on the filter field instead of via `Shortcut`, so they work
  while the field has focus (as it always does) and the badges shown
  in the picker are honest about what pressing a key does. Return,
  Enter, Escape, Up, Down, and Alt+A were unaffected (the filter field
  does not claim those keys) and are unchanged.
- Opening a destination from the picker, the hold bar, or a silent
  rule/remembered open launched the target but never gave it focus,
  because Lane launched with no XDG activation token and the picker
  and hold overlays hold exclusive keyboard input at the moment of
  launch, which Wayland's focus-stealing prevention correctly refused
  to hand to an unauthorized process. Lane now requests a fresh
  activation token from the overlay that was on screen (or reuses
  whatever token this click's `openUrl()` call itself arrived with, for
  a silent open with no overlay involved) and hands it to the launched
  process via `XDG_ACTIVATION_TOKEN`, dismissing the overlay only after
  the token request is issued. A launch still proceeds immediately if
  no token can be obtained; it is just not raised.
- "Check for updates" always showed the same "Could not check for
  updates. Try again later." for every failure, whether the cause was
  no network connection, a rate limit, a 404, or an unparseable
  response, so a real problem (the GitHub repo slug changed ahead of
  the actual rename, and currently 404s) looked identical to being
  offline. Each cause now has its own message: DNS/connection/timeout
  failures say so, a rate limit names roughly when it resets, a 404
  names the repo slug and says releases were not found there (which
  covers both a missing repo and one with no releases yet, since
  GitHub's API answers both the same way), and a malformed 200
  response is called out as a bug rather than a network problem. A
  last-checked time is now shown next to the result, so pressing the
  button twice is visibly different from doing nothing.
- The picker window's height budgeted space for exactly two section
  headers regardless of how many were actually shown. With three or
  more kinds of destination present (containers, web apps, and custom
  apps, say), rows that should have been visible within the first few
  slots, including ones with a number-key shortcut, could be pushed
  below the scrollable fold. The picker now sizes itself from the
  real number of sections being shown.
- Dragging a row to reorder browsers or apps while a search filter was
  active could save an order different from what was visually
  dragged, because a filtered-out row keeps its slot in the underlying
  list even though it is drawn at zero height. Drag-reorder is now
  disabled while a filter is active; clearing the search box restores
  it.
- The picker footer put the Always checkbox, the destination ladder,
  and the key hints on one row, so a long target name pushed the
  footer past the card edge. The footer is now two rows and nothing
  clips (user-reported).
- A second click while a first link was still being unshortened could
  overwrite the in-flight click and cancel its hold. The in-flight
  click is now protected from re-entrancy.
- Lane segfaulted on X11 and non-wlroots compositors because the
  layer-shell surface could be null. A null check now skips the
  overlay instead of crashing.
- `lane --rediscover` and `lane --configure` were documented but the
  binary rejected them as Unknown option. Both are registered and
  work.
- Renaming, reordering, or hiding a target in settings triggered a
  full browser re-scan, which could reshuffle rows and lose the
  user's place. Cosmetic mutations now save without rediscovering.
- Remembered destinations pointing at targets that no longer exist
  were kept forever and could never match. They are now pruned when
  the config loads.
- Rules that match on window title or source process can never match
  on Wayland, because Lane cannot see the caller's identity there.
  They now warn once instead of silently never firing.
- A PWA scope like `/bits` matched any path that merely started with
  those characters, so `/bitskc` was wrongly treated as in scope.
  Scope matching now compares path segments.
- Unknown top-level keys in `config.json` were silently ignored, which
  hid typos and stale keys. They now log a warning naming the key.
- The settings page spawned `xdg-settings` on every read to check
  whether Lane is the default browser. The result is now cached and
  refreshed only when it can change, and refreshed every time Settings
  is opened, so an external change (e.g. via System Settings) is picked
  up without a restart.
- Every row on the Browsers & apps page (browsers, containers, web
  apps, custom apps, private windows) showed a drag handle, a toggle,
  and a "Default" button, but no name or icon. Each `ListView`'s
  `delegate` was a `Loader` whose `sourceComponent` pointed at a
  `Component` declared elsewhere in the file; objects a `Loader`
  creates from `sourceComponent` do not inherit the row's `model`/
  `index` context the way a `Component` used directly as `delegate`
  does, so every `model.name`/`model.iconName`/`model.hidden` binding
  inside the loaded delegate silently resolved to nothing. Each
  draggable section's delegate is now a plain `Item` wrapper (assigned
  directly as `ListView.delegate`, no `Loader`) holding the row's
  `ItemDelegate` as an ordinary child: plain children inherit `model`/
  `index` normally, and `Kirigami.ListItemDragHandle.listItem` still
  points at the inner `ItemDelegate`, not the wrapper, so the wrapper
  keeps the row's layout slot in the `ListView` while the delegate
  handle reparents the `ItemDelegate` during a drag, matching
  `ListItemDragHandle`'s documented contract. The private-windows
  section has no drag handle and keeps its `ItemDelegate` as the
  direct delegate.

### Security

- Custom targets loaded from `config.json` now go through the same
  shell and interpreter blocklist the settings window enforces.
  `launchTarget()` refuses a blocked interpreter, and `targetFromJson()`
  drops a bad entry at load time with a warning naming the id, keeping
  the rest of the config. Before this, a hand-edited or agent-edited
  `customTargets` entry pointing at `bash` or `python3` would run on
  the next matching click with no check.
- The blocklist above checked only the exec path's own basename, which
  two bypasses got past: an absolute exec that was itself a symlink to
  a blocked interpreter under an unrelated name, and a wrapper like
  `env` re-execing a blocked interpreter through its own args rather
  than being one itself. `isBlockedInterpreterChain()` now walks the
  full symlink chain from exec to whatever it actually resolves to,
  checking every hop's basename, and the blocklist itself now also
  covers re-exec wrappers (`env`, `xargs`, `sudo`, `pkexec`, `ssh`,
  `find`, and others that can run a different program than the one
  named in exec). This remains a blocklist, not a privilege boundary:
  editing `config.json` already requires write access to the user's
  home directory, so it is defense in depth against a hand-edited or
  agent-edited config, not protection against a locally compromised
  account.
- `Controller::persist()` (every settings, rule, target-order, and
  remembered-destination change) used to discard `saveConfig()`'s
  result. A failed atomic write (disk full, permissions, read-only
  `~/.config/lane`) was a silent no-op: the change looked saved and
  was gone on the next restart. Lane now shows a notification when a
  save fails.
- Config writes are atomic (`QSaveFile`, temp file plus rename). A
  crash or power loss mid-write can no longer truncate `config.json`.
- A config file that does not parse is moved aside to
  `config.json.corrupt-<timestamp>` with a warning instead of being
  silently replaced by defaults. Rules and remembered destinations are
  never discarded without a copy.

## [0.1.0] - 2026-09-11

First public release.

### Added

- Picker overlay: pick a target by number, filter by typing, or press
  Alt+A to remember a destination.
- Hold bar: a short pause (about 1.6 seconds) before a silent open, so
  you can stop it with Esc or Space. Explicit rules skip the hold.
- Two-pane settings window with Overview, Browsers & apps, Rules, and
  Preferences pages.
- Drag-reorder and rename for browsers and apps on the Browsers & apps
  page. Private/incognito windows are excluded from the drag order for
  their parent browser.
- Path-scoped memory for remembered destinations. `github.com/bitskc`
  can go somewhere different from `github.com`. Comma narrows the
  remembered path, period widens it.
- "Always for" now defaults to the path, not the whole host.
- Rules engine: match by URL with optional regex, scoped to
  any/domain/path. First match wins. Rule objects also accept
  `"title"` and `"process"` locations for compatibility, but those
  cannot match on Wayland because caller identity is not available.
- Discovery for Gecko profiles (Firefox, Zen, LibreWolf, Floorp,
  Waterfox), Chromium-family profiles (Brave, Chrome, Edge, Vivaldi,
  Opera), and `firefoxpwa` sites.
- Outlook safe-link unwrapping and optional link unshortening.
- Agent-friendly `~/.config/lane/config.json` with a published JSON
  schema (`docs/config.schema.json`), plus `lane --list`,
  `lane --explain URL`, and `lane --config-path` for inspecting
  config without the GUI. See `AGENTS.md`.
- `KStatusNotifierItem` tray icon with Settings and Rediscover actions.
- systemd user unit for autostart, installed to the systemd user unit
  search path.

### Changed

- Desktop entry, D-Bus service, and autostart files now use absolute
  paths to the installed binary, so Plasma's app menu and D-Bus
  activation find `lane` even when `~/.local/bin` is not on `PATH`.
- Lane claims a real D-Bus name, `app.lane.Lane`, instead of a
  placeholder, so the app menu can start it and duplicate launches
  hand off to the running instance.
- Project license switched to the PolyForm Noncommercial License 1.0.0.
  Personal and hobby use is free; commercial use needs a separate
  license. See `COMMERCIAL.md`.

### Security

- Lane only ever opens `http` and `https` URLs. It rejects `file`,
  `javascript`, `data`, and URLs with embedded credentials. Custom
  handlers run as argv, never through a shell.

[Unreleased]: https://github.com/bitskc/lane/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/bitskc/lane/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/bitskc/lane/releases/tag/v0.1.0
