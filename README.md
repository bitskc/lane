# Tern

Plasma-native link router for KDE. Becomes the default browser, then opens each click in the right **browser profile**, **installed web app**, or **custom handler**.

Tern is the Linux answer to Velja / Choosy / Opener, with Browser Tamer’s rule engine and first-class Firefox PWA support. It is built as a Qt 6 + Kirigami resident process so the picker can appear instantly on Wayland.

## Why it exists

KDE still has one default browser. Real machines do not: Zen work/personal, Brave, Firefox profiles, Outlook PWA, GitHub PWA, Slack links, Outlook safe-links. Tern sits in the middle.

## Features

- Discovers Gecko (Firefox, Zen, LibreWolf, Floorp, Waterfox) and Chromium (Brave, Chrome, Edge, Vivaldi, Opera) profiles, plus `firefoxpwa` sites
- Rules on URL / domain / path / process / window title, substring or regex
- Remember “always for this site” from the picker
- Auto-open PWAs when the URL is inside their scope
- Outlook / Teams safe-link unwrap (match the destination, open the wrapper)
- Short-URL expansion for rule matching
- Keyboard-first overlay: `1–9`, type-to-filter, Enter, Esc, Alt+A
- Plasma toast after a silent rule hit
- `tern --list`, `tern --pick URL`, `tern --explain URL`, `tern --daemon`

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

Launch `Tern` from the app menu, then **Use Tern as default browser**. Turn on **Start Tern when I log in** so the first click is instant.

## Daily use

Click a link anywhere. If Tern already knows (rule, remembered host, or unique PWA), it opens there and can toast. Otherwise a center overlay lists targets, ranked with matching apps first.

## Config

`~/.config/tern/config.json` is the source of truth. The settings window writes the same file.

## CLI

```
tern                         # settings
tern --daemon                # resident, no window
tern https://example.com     # route a URL
tern --pick https://example.com
tern --explain https://claude.ai
tern --list
```

## License

GPL-3.0-or-later

