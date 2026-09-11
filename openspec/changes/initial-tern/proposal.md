# Lane initial product

Plasma-native default-browser proxy: discover browsers/profiles/PWAs, route with rules, remember hosts, picker overlay.

## Why

KDE has one default browser. This machine has Zen profiles, Brave, Firefox, and 13 firefoxpwa apps. Browser Tamer is the strategic model; Velja is the feel.

## Scope

v1: discovery, pipeline (O365, unshorten, substitutions), rules, PWA auto-open, picker, toast, settings, CLI, autostart, set-as-default. No Lua, no containers, no PDF/mailto.

## Requirements

- Discover Gecko and Chromium profiles from XDG desktop files plus `profiles.ini` / `Local State`
- Discover firefoxpwa sites; desktop `Name=` wins over manifest name
- Route: rules, remembered host, unique PWA scope, picker policy, default
- Picker is a Wayland layer-shell overlay sized to the active screen
- Private/incognito/Tor stay available to rules but do not clutter the picker
- Never steal the default browser except via the settings button
- `lane --list` and `lane --explain URL` work without the unique daemon

## Out of scope

- Browser Tamer Lua scripts
- Firefox Multi-Account Containers
- PDF / mailto handlers
