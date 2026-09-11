# Changelog

All notable changes to Tern are documented here. The format follows
[Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/), and Tern
uses [Semantic Versioning](https://semver.org/): while the major
version is 0, minor releases may still contain breaking changes.

## [Unreleased]

### Added

- Firefox and Zen contextual identities (containers) show up as
  destinations, one row per container per profile, e.g.
  "Zen · Default · Work". Launch wraps the http(s) URL in an
  `ext+container:name=...&url=...` argument passed to the browser;
  Tern itself still only ever opens `http`/`https` links, and does not
  register `ext+container` as anything it handles. Containers only
  show up for profiles where Tern can tell a protocol-handler
  extension is installed (Open URL in Container, Default Container
  Handler, or similar); without one, the browser has nothing to act
  on an `ext+container` link with.

### Changed

- Picker shows eight rows instead of six.
- Overlay tints the desktop instead of dimming it out. (Picker/hold no longer blur the whole screen.)

### Fixed

- Picker and hold overlays failed to load on current Kirigami (6.28),
  because they set `borderColor`/`borderWidth` directly instead of the
  grouped `border.color`/`border.width` properties. This made link
  clicks bounce in the taskbar and then do nothing. Fixed in
  `Picker.qml` and `Hold.qml`.
- Firefox-family browsers (Firefox, Zen, LibreWolf, Floorp, Waterfox)
  were launched with `-P <internal name>`, and on Zen that internal
  name is often `Default Profile` or `Default (release)`, which don't
  round-trip cleanly as a launch argument. Tern now launches with
  `--profile <folder>`, using the profile's real directory instead.
  Discovery also picks whichever config folder actually holds a
  browser's `profiles.ini` (instead of guessing based on folder
  order), skips profiles whose folder is gone or was never opened,
  and no longer lists a browser twice when two `.desktop` files point
  at the same install. Zen's install-default profile is now labeled
  "Default" instead of its raw internal name.

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
- Rules engine: match by URL, window title, or source process, with
  optional regex, scoped to any/domain/path. First match wins.
- Discovery for Gecko profiles (Firefox, Zen, LibreWolf, Floorp,
  Waterfox), Chromium-family profiles (Brave, Chrome, Edge, Vivaldi,
  Opera), and `firefoxpwa` sites.
- Outlook safe-link unwrapping and optional link unshortening.
- Agent-friendly `~/.config/tern/config.json` with a published JSON
  schema (`docs/config.schema.json`), plus `tern --list`,
  `tern --explain URL`, and `tern --config-path` for inspecting
  config without the GUI. See `AGENTS.md`.
- `KStatusNotifierItem` tray icon with Settings and Rediscover actions.
- systemd user unit for autostart, installed to the systemd user unit
  search path.

### Changed

- Desktop entry, D-Bus service, and autostart files now use absolute
  paths to the installed binary, so Plasma's app menu and D-Bus
  activation find `tern` even when `~/.local/bin` is not on `PATH`.
- Tern claims a real D-Bus name, `app.tern.Tern`, instead of a
  placeholder, so the app menu can start it and duplicate launches
  hand off to the running instance.
- Project license switched to the PolyForm Noncommercial License 1.0.0.
  Personal and hobby use is free; commercial use needs a separate
  license. See `COMMERCIAL.md`.

### Security

- Tern only ever opens `http` and `https` URLs. It rejects `file`,
  `javascript`, `data`, and URLs with embedded credentials. Custom
  handlers run as argv, never through a shell.

[Unreleased]: https://github.com/bitskc/tern/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bitskc/tern/releases/tag/v0.1.0
