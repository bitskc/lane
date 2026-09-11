# Changelog

All notable changes to Tern are documented here. The format follows
[Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/), and Tern
uses [Semantic Versioning](https://semver.org/): while the major
version is 0, minor releases may still contain breaking changes.

## [Unreleased]

Nothing yet.

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
  `tern --explain <url>`, and `tern --config-path` for inspecting
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

### Fixed

- Picker overlay window now finds the launcher binary correctly from
  Plasma's application menu (was previously relying on a relative
  path that only worked from a shell with the right `PATH`).

### Security

- Tern only ever opens `http` and `https` URLs. It rejects `file`,
  `javascript`, `data`, and URLs with embedded credentials. Custom
  handlers run as argv, never through a shell.

[Unreleased]: https://github.com/bitskc/tern/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bitskc/tern/releases/tag/v0.1.0
