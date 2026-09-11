# Tern

Tern is a link router for KDE Plasma. It becomes your default browser,
then sends each click to the right browser profile, web app, or custom
handler.

If you run Zen for personal stuff, Firefox for work, and a GitHub PWA
for repos, Tern sorts that out without you thinking about it.

It is a Qt 6 + Kirigami app that stays running in the background, so the
picker shows up instantly on Wayland. Same idea as Browser Tamer, Choosy,
or Velja, but native to Plasma.

## What it does

- Discovers Gecko profiles (Firefox, Zen, LibreWolf, Floorp, Waterfox)
  and Chromium profiles (Brave, Chrome, Edge, Vivaldi, Opera), plus
  `firefoxpwa` sites.
- Desktop `Name=` wins over the PWA manifest name. QBO shows as QBO, not
  "Home".
- Decision order: rules first, then remembered destination, then unique
  PWA if you prefer PWAs, then picker policy, then default.
- Remembered destinations are path-scoped, not just host.
  `github.com/bitskc` can go somewhere different from `github.com`.
- PWA scopes are origin-wide, so Tern checks that a PWA scope actually
  covers the path before auto-opening. A tenant path like
  `github.com/bitskc` will not get stolen by a broader PWA scope.
- Silent opens (no rule, no remembered choice) get a short hold of
  about 1.6 seconds. Enter opens now. Esc or Space brings up the picker.
- Picker overlay: press 1 through 9 to pick, type to filter, Enter to
  confirm, Esc to cancel. Alt+A toggles always-for-this-site. Comma and
  dot narrow or widen the remembered path.
- Private, incognito, and Tor modes stay in rules. They do not show up
  in the picker.
- Opt-in default browser. Tern does not take mailto or PDF.
- Security: http and https only. Tern rejects `file`, `javascript`,
  `data`, and URLs with embedded credentials. It unshortens known
  shorteners with a HEAD request only. No private or link-local
  addresses. Custom handlers run as argv, not through a shell.
- Outlook and Teams safe-link unwrapping for matching. Short-URL
  expansion for rule matching.

## Config

`~/.config/tern/config.json` is the source of truth. The settings window
writes the same file. If you edit it by hand, restart the daemon.

## CLI

```
tern                              # open settings
tern --daemon                     # run in background, no window
tern https://example.com          # route a URL
tern --pick https://example.com   # force the picker
tern --explain https://claude.ai  # print the routing decision
tern --list                       # print discovered targets
```

## Build (CachyOS / Arch)

```bash
sudo pacman -S --needed cmake extra-cmake-modules ninja qt6-base qt6-declarative qt6-svg \
  kirigami kirigami-addons ki18n kcoreaddons kconfig kdbusaddons knotifications \
  kwindowsystem kiconthemes kstatusnotifieritem layer-shell-qt qqc2-desktop-style
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
update-desktop-database "$HOME/.local/share/applications"
```

Launch Tern from the app menu, then click "Use Tern as default browser".
Turn on "Start Tern when I log in" so the first click is instant.

## License

Source is available under the PolyForm Noncommercial License 1.0.0.
Personal, hobby, and internal use is free. Commercial use needs a paid
license. See [COMMERCIAL.md](COMMERCIAL.md) for details.
