# Lane

Lane is a link router. It becomes your default browser, then sends each
click to the right browser profile, web app, or custom handler. It runs
on KDE Plasma (Linux) and macOS.

If you run Zen for personal stuff, Firefox for work, and a GitHub PWA
for repos, Lane sorts that out without you thinking about it.

It is a Qt 6 Quick app that stays running in the background, so the
picker shows up instantly. Same idea as Browser Tamer, Choosy,
or Velja, but native to Plasma (and now macOS).

![Picker overlay](docs/screenshots/picker.png)
*The overlay you get when Lane does not already know where to send a link.*

![Hold bar](docs/screenshots/hold.png)
*The hold bar when enabled in Preferences. Press Enter to go now, Esc or Space to pick instead.*

![Settings overview](docs/screenshots/settings.png)
*Overview page: default browser, autostart, and discovered targets.*

## Releases

The version you have lives in [GitHub Releases](https://github.com/bitskc/lane/releases).
What changed between versions is in [CHANGELOG.md](CHANGELOG.md). Run
`lane --version` to check what's installed.

## What it does

Lane discovers Gecko profiles (Firefox, Zen, LibreWolf, Floorp, Waterfox)
and Chromium profiles (Brave, Chrome, Edge, Vivaldi, Opera), plus
`firefoxpwa` sites. Flatpak-installed browsers are discovered too; their
profiles live under `~/.var/app/<app-id>/`. Desktop `Name=` wins over the
PWA manifest name.

Zen and Firefox containers (contextual identities) show up as
destinations too, wherever `containers.json` exists and the browser
has a protocol-handler extension installed to act on them.

Decision order: rules first, then remembered destination, then unique
PWA if you prefer PWAs, then picker policy, then default.

Remembered destinations are path-scoped, not just host.
`github.com/bitskc` can go somewhere different from `github.com`. PWA
auto-open respects scope, so an origin-wide PWA scope like
`https://github.com/` does not grab tenant paths like `github.com/bitskc`.

Optional hold bar: when enabled in Preferences, Lane pauses for about 1.6
seconds before opening from a remembered choice, a unique PWA, or the default
target, so you can stop it. Explicit rules skip the hold and open immediately.

Opt-in default browser. Lane does not take mailto or PDF. Security: http
and https only. Lane rejects `file`, `javascript`, `data`, and URLs with
embedded credentials. Custom handlers run as argv, not through a shell.

## Tutorials

### First run

After you build and install:

1. Open Lane from your app menu. The settings window opens on the
   Overview page.
2. Turn on "Start Lane when I log in". This keeps the daemon running so
   the picker shows up instantly.
3. Click "Use Lane as default browser" when you are ready. Lane takes
   http and https only. mailto and PDF stay with their own apps.
4. Not ready to commit? Click "Try the picker" on the Overview page. It
   opens the overlay with a sample URL so you can see how it works
   without making Lane your default.
5. Click "Rediscover" if you install or remove a browser after the first
   run.

### Pick a destination

Say you click a link to `https://github.com/bitskc/lane`. Lane does not
know where you want it yet, so the picker overlay appears.

1. Press 1 through 8 to pick a target by number. Or type to filter the
   list, then press Enter to confirm.
2. Press Esc to cancel and do nothing.
3. To remember this choice, press Alt+A or check "Always for
   github.com/bitskc". Next time you open a link to that destination,
   Lane skips the picker and opens it there directly.
4. The checkbox remembers the path, not just the host. Checking "Always
   for github.com/bitskc" remembers `github.com/bitskc` by default, not
   all of `github.com`. Press comma to narrow the path further, for
   example down to a specific repo. Press period to widen it back out.
   The chevrons in the footer do the same thing with the mouse.

### Stop a silent open

When "Pause before opening" is enabled in Preferences, Lane shows a hold
bar for about 1.6 seconds before launching from a remembered choice, a
unique PWA, or the default target.

1. The bar says "Opening in" followed by the target name, with a
   progress bar underneath.
2. Press Enter to open now, without waiting.
3. Press Esc or Space to cancel the hold and show the picker instead.
4. Written rules skip the hold and open immediately.
5. The hold is off by default. Turn it on in Preferences with "Pause
   before opening" if you want the countdown before each silent open.

### Teach it a rule

Rules send a link to a specific target without asking. You can add them
in the settings window or by editing the config file.

To add a rule in the UI:

1. Open Settings and go to the Rules page in the sidebar.
2. Click "Add rule".
3. Set "When the link matches" to `github.com/bitskc`.
4. Set "Look at" to "Path".
5. Set "Open in" to your work profile.
6. Make sure "Enabled" is on.

Now any link to `github.com/bitskc` opens in that profile. No picker, no
hold.

To add a rule by hand:

1. Run `lane --list` to get the target ID for your work profile.
2. Run `lane --explain https://github.com/bitskc/lane` to confirm the
   URL matches your pattern.
3. Edit `~/.config/lane/config.json` and add a rule object to the
   `rules` array with the pattern, scope `"path"`, and the target ID.
4. Run `lane --rediscover` so the running daemon reloads the file. Or
   restart the daemon if you prefer a clean process:

```bash
lane --rediscover
# or:
pkill -f "lane --daemon"
lane --daemon &
```

`lane --rediscover` is not a lightweight one-shot command. It goes through
the same D-Bus entry point as opening a link: if a daemon is already
running, the new process hands off to it and exits; if not, it starts the
full background process, reloads config, rescans targets, and stays
resident.

For Claude Code, Codex, and similar agents, see `AGENTS.md` for the full
config shape and CLI inspection commands.

## Config

`~/.config/lane/config.json` is the source of truth. The settings window
writes the same file. If you edit it by hand, run `lane --rediscover` to
reload the file and rescan targets in the running daemon (via D-Bus when
one is already up; otherwise it starts the full background process).
Restarting the daemon (`pkill -f "lane --daemon"` then `lane --daemon &`)
still works if you want a clean process.

## CLI

```
lane                              # open settings
lane --version                    # print the installed version
lane --help                       # print usage
lane --daemon                     # run in background, no window
lane https://example.com          # route a URL
lane -p https://example.com       # force the picker
lane --pick https://example.com   # force the picker
lane --explain https://claude.ai  # print the routing decision
lane --config-path                # print the config file path
lane --list                       # print discovered targets
lane --settings                   # open settings
lane --configure                  # alias for --settings
lane --rediscover                 # reload config/rescan via running daemon (starts full process if none)
lane --clipboard                  # route the URL on the clipboard
```

The tray menu's "Open clipboard URL" does the same thing, greyed out when
the clipboard holds no URL. On KDE the desktop entry exposes it as a
right-click action too. On macOS you can route from Shortcuts or a script
with `open "lane://open?url=<encoded-url>"`.

### Raycast (macOS)

Two ways to control Lane from Raycast (especially helpful when a crowded
menu bar hides the status item):

#### 1. Native Raycast Extension (recommended)

A full TypeScript extension lives in `packaging/mac/raycast-extension/`.
To install it in one command:

```bash
packaging/mac/raycast-extension/install.sh
```

Raycast registers it immediately under Developer Extensions. The extension
stays installed in Raycast permanently.

Or manually: `cd packaging/mac/raycast-extension && npm install && npm run dev`.

Commands:
- **Open Clipboard URL in Lane**: routes the clipboard URL through Lane
- **Open URL in Lane**: native Raycast prompt with argument field (bare hosts auto-promote to `https://`)
- **Open Lane Settings**: brings up the Lane Settings window
- **Rescan Lane Targets**: reload config and re-scan browser profiles with HUD confirmation

#### 2. Standalone Script Commands (zero dependencies)

If you don't want Node/npm: ready-made bash scripts live in
`packaging/mac/raycast/`. Add that directory in Raycast → Settings →
Extensions → "Add Script Directory".
## Build (CachyOS / Arch)

For a system-wide install, use the PKGBUILD in `packaging/`:
`cd packaging && makepkg -si`. An AUR package is planned.

To install under `$HOME/.local` instead:

```bash
sudo pacman -S --needed cmake extra-cmake-modules ninja qt6-base qt6-declarative qt6-svg \
  kwindowsystem layer-shell-qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
ctest --test-dir build --output-on-failure
update-desktop-database "$HOME/.local/share/applications"
```

`kwindowsystem` is optional: without it, Lane still works, it just skips the
xdg-activation token handoff when launching a browser on Wayland (the
launched browser may open unfocused instead of raised).

`appstreamtest` only runs after `cmake --install`; before that it reports
"Not installed yet, skipping". Install to the same prefix you passed to
CMake so the manifest paths exist.

The install prefix is baked into the desktop and service files, so the
launcher finds the binary even when `$HOME/.local/bin` is not on PATH.
Run `update-desktop-database` after installing so the menu picks up the
new entry.

Launch Lane from the app menu, then click "Use Lane as default browser".
Turn on "Start Lane when I log in" so the first click is instant.

## Build (macOS)

The macOS app is a plain Qt 6 bundle: `Lane.app`, `LSUIElement`, lives in
the menu bar, no Dock icon. Requires macOS 13 or newer on Apple Silicon.

```bash
brew install qt cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
ctest --test-dir build --output-on-failure
packaging/mac/bundle.sh build /Applications
```

`bundle.sh` renders `Lane.icns` from the SVG, runs `macdeployqt`, rewrites
Homebrew library paths to `@rpath`, and ad-hoc signs the bundle (set
`LANE_SIGN_IDENTITY` for an Apple Development identity). Installing to
`/Applications` matters: the System Settings default-browser list only
scans `/Applications`, not `~/Applications`.

On first run Lane asks for notification permission and offers "Use Lane
as default browser", which triggers the macOS consent prompt for
http/https. Lane stays in the menu bar; open it again (or click the
status item) to reach Settings. ⌘Q and the status-item "Quit Lane" both
quit the app.

## License

Source is available under the PolyForm Noncommercial License 1.0.0.
Personal and hobby use is free. Using Lane in a product you sell needs a
commercial license. See [COMMERCIAL.md](COMMERCIAL.md) for details.
