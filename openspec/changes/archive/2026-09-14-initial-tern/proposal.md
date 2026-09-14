# Lane initial product

Plasma-native default-browser proxy: discover browsers/profiles/PWAs/containers, route with rules, remember destinations, picker overlay.

## Why

KDE has one default browser. This machine has Zen profiles, Brave, Firefox, and 13 firefoxpwa apps. Browser Tamer is the strategic model; Velja is the feel.

## What Changes

Ships Lane v0.1.0: a resident Qt 6 daemon that becomes the default browser, discovers browser profiles, PWAs, and Firefox/Zen containers, routes links through rules and path-scoped remembered destinations, and shows a layer-shell picker or a short hold HUD before silent opens. Settings, CLI, autostart, update checker, legacy config migration, and release machinery are included.

## Scope

v1: discovery, pipeline (O365, unshorten, substitutions), rules, PWA auto-open, containers, picker, hold HUD, toast, settings, CLI, autostart, set-as-default, update checker, release machinery. No Lua, no PDF/mailto.

## Requirements

- Discover Gecko and Chromium profiles from XDG desktop files plus `profiles.ini` / `Local State`
- Discover firefoxpwa sites; desktop `Name=` wins over manifest name
- Discover Firefox/Zen containers as targets where `containers.json` exists and a protocol-handler extension is present
- Route: rules, path-scoped remembered destination, unique PWA scope, picker policy, default
- Remember destinations by path, not just host; the picker offers a destination ladder to narrow or widen scope
- Show a hold HUD before silent opens (remembered, PWA, default) unless `holdAutoOpen` is off; rules open immediately
- Picker is a Wayland layer-shell overlay sized to the active screen
- Private/incognito/Tor stay available to rules but do not clutter the picker
- Never steal the default browser except via the settings button
- `lane --list` and `lane --explain URL` work without the unique daemon
- `lane --rediscover` rescans targets in the running daemon; `lane --configure` opens settings
- Drag-reorder and rename targets in settings
- Check for updates against GitHub releases; version compare and release CI in repo
- Migrate `~/.config/tern` config to `~/.config/lane` on first run

## Out of scope

- Browser Tamer Lua scripts
- PDF / mailto handlers

## Capabilities

### New Capabilities

- `lane`: Lane v0.1.0 link routing: discovery, routing, picker, hold HUD, CLI, settings
