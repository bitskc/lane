# Portability review (Windows readiness)

Second-round audit of `bitskc/lane` on `main` (`ed271df`). Question: can a
Windows backend sit behind `Lane::Platform` (`src/app/platform/platform.h`)
without rewriting `src/core` and the shared app sources? Linux and macOS
behavior already covered by `docs/designs/gstack-full-analysis.md` rows 9-19
is left alone (macOS CI, socket peer credentials, flatpak `--command` scope,
overlay collection behavior, tasks.md checkboxes, host filter gaps, plist
recursion, Linux `xdg-email` injection, residual `.html` drops, macOS toast
identifier, macOS theme-icon misses).

## Verdict

**Not Windows-ready.** The `Platform` header itself is a usable contract:
methods take `QString`, `QWindow *`, and callbacks, and
`DefaultHandlerState::Status` (`Registered`, `Declined`, `Partial`,
`OtherCopy`, `Unknown`) can describe a registry probe. CMake never builds
that contract on Windows, and several shared translation units will not
compile there. Browser discovery, the email action, the instance endpoint,
and focus handoff live beside the seam as Linux/macOS branches.

`lane-core` is the part that can compile: it is Qt only. It still will not
find or launch a normal Windows browser install.

Qt facts below are from the Qt 6.8.0 tag
(`src/corelib/io/qstandardpaths_win.cpp`,
`src/network/socket/qlocalserver_win.cpp`), not from a Windows Lane build.
This session did not configure or compile Lane with MSVC or MinGW.

## Findings

### P1. No Windows app target, and shared app sources are POSIX

`CMakeLists.txt:18-28` sets `LANE_KDE_DESKTOP` only for `UNIX AND NOT APPLE`
and `LANE_MAC_DESKTOP` only for `APPLE`. `WIN32` gets neither. The `lane`
executable exists only inside those two branches
(`src/CMakeLists.txt:104`, `src/CMakeLists.txt:164`). There is no
`platform_windows.cpp`. `platform()` is defined only in
`platform_linux.cpp:257` and `platform_macos.mm:460`.

`lane-app` is still created on every host (`src/CMakeLists.txt:56`) and
always compiles:

- `src/app/CrashLog.cpp:17-19` includes `<fcntl.h>` and `<unistd.h>`.
  `install()` (`CrashLog.cpp:156-164`) calls `sigaltstack` and `sigaction`
  with `SA_ONSTACK`. The crash writer (`CrashLog.cpp:81`) uses
  `::open` with `O_CLOEXEC | O_NOFOLLOW` and mode `0600`. Those headers and
  flags are POSIX. [INFERENCE] An MSVC build fails in this file before
  link; this session did not run that compiler.
- `src/app/SingleInstance.cpp:20` includes `<sys/stat.h>` with no
  `Q_OS_UNIX` guard. `forwardToPrimary` (`SingleInstance.cpp:88-90`) calls
  `::stat` and `S_ISSOCK` on every connect failure that is not
  "not found" or "refused". `getuid` and `confstr` are correctly inside
  `#ifdef Q_OS_MACOS` (`SingleInstance.cpp:17-18`, `:150-165`). `S_ISSOCK`
  is not.
- `Controller` calls `platform()` (`Controller.cpp:122`, and the inline
  `reduceMotion()` at `Controller.h:159`). With no Windows TU, the link of
  `lane-app` has no `Lane::platform`.

A third `platform_*.cpp` matching the macOS pattern (return `true` from
`registerActivation`, empty activation token) does not get these files
through a Windows compile.

### P1. The instance endpoint is a filesystem socket path, and it is not on `Platform`

`singleInstanceSocketPath()` (`SingleInstance.h:31`,
`SingleInstance.cpp:146-177`) is a free function. `Platform` has no hook
for it.

Non-mac path: `XDG_RUNTIME_DIR`, otherwise
`QStandardPaths::RuntimeLocation`, then `dir + "/lane-instance.sock"`.
On Windows, Qt 6.8 maps `RuntimeLocation` to `QDir::homePath()`
(`qstandardpaths_win.cpp` case `RuntimeLocation`, the arm that breaks with
`HomeLocation`). The string is therefore
`C:/Users/<user>/lane-instance.sock` when `XDG_RUNTIME_DIR` is unset.

`QLocalServer::listen` on Windows does not create that file. Qt 6.8
(`qlocalserver_win.cpp`, `listen(const QString &name)`) uses the name as a
pipe: if it does not already start with `\\.\pipe\`, it prefixes that.
The listen name becomes `\\.\pipe\C:/Users/<user>/lane-instance.sock`.
The claim lock (`SingleInstance.cpp:207`) is a real file at
`<that path>.lock` in the user profile. `QLockFile` itself has a Windows
backend; the path it is given assumes the endpoint is a filesystem object
next to a Unix socket.

`XDG_RUNTIME_DIR` is a production override (`SingleInstance.cpp:148`),
which tests point at a temp dir (`tests/test_singleinstance.cpp:98-113`).
It is not test-only. An inherited `XDG_RUNTIME_DIR` on Windows still goes
through the same `dir + "/lane-instance.sock"` concatenation.

The 103-byte `sun_path` guard stays under `Q_OS_MACOS`
(`SingleInstance.cpp:169-176`). That part matches the macOS-only fix
already shipped; it does not cover Windows pipe-name rules.

### P1. Discovery and the email action have no Windows branch

`discoverTargets` (`discovery.cpp:1530-1531`) always does
`scanDesktopFiles` plus `scanMacBundles`. `scanDesktopFiles`
(`discovery.cpp:378-387`) lists `*.desktop` only. `defaultDiscoveryPaths`
(`discovery.cpp:1475-1504`) fills `applicationDirs` from
`QStandardPaths::ApplicationsLocation` and then appends Flatpak export
dirs with no OS guard. On Windows, `ApplicationsLocation` is
`FOLDERID_Programs` (the per-user Start Menu Programs folder,
`qstandardpaths_win.cpp` known-folder table). That folder holds `.lnk`
files, which this scanner skips. `bundleDirs` and `applicationSupport`
are set only under `Q_OS_MACOS` (`discovery.cpp:1482-1488`), so
`scanMacBundles` runs on an empty list and returns nothing.

Profile roots are a linux column plus a mac column (`discovery.cpp:569-622`).
The mac column is used only when `applicationSupport` is non-empty
(`discovery.cpp:624-637`). Windows takes the linux column, rooted at
`ConfigLocation` (`%LOCALAPPDATA%/<org>/<app>` once
`organizationName` is set) or at the home directory. Firefox on Windows
stores profiles under `%APPDATA%\Mozilla\Firefox`, Chrome under
`%LOCALAPPDATA%\Google\Chrome\User Data`. Neither relative path is in the
table. There is no registry or App Paths scan, and `DiscoveryPaths` is a
data bag the caller fills, not a virtual scanner.

`actionTargets` (`discovery.cpp:1454-1463`) sets the email exec to
`/usr/bin/open` on macOS and to `xdg-email` on every other OS, including a
future Windows build. `launchTarget` will not find that program.
`QProcess::startDetached` (`launcher.cpp:450`) can start a real `.exe`
with arguments, so a hand-authored custom target with an absolute path
can launch. Discovered browsers cannot, because discovery never produces
those paths.

`macFixedOpenArgs` / `open -b` (`launcher.cpp:350-370`, `:382-394`) and
`plutil` (`discovery.cpp:1302-1316`) are inside `Q_OS_MACOS`. Those gates
are complete. The non-mac arm is "Linux desktop", not "everything else".

Flatpak argv checks in `launchTarget` (`launcher.cpp:424-427`) key off the
basename `flatpak`. They do not run for a normal Windows exe. Flatpak
export dirs appended in `defaultDiscoveryPaths` are harmless missing
directories. The open finding on `--command` scope (analysis row 11) is
unchanged and is a Linux issue.

### P2. Focus handoff is an `XDG_ACTIVATION_TOKEN` string, not a platform operation the child can use

`Platform::requestActivationToken` (`platform.h:74-76`) is asynchronous
and returns a `QString`. Linux fills an xdg-activation token
(`platform_linux.cpp:154-174`). macOS completes with an empty string
(`platform_macos.mm:390-397`). Callers then pass that string into
`launchTarget`, which `qputenv`s `XDG_ACTIVATION_TOKEN` around
`QProcess::startDetached` (`launcher.cpp:444-457`). The same variable is
captured in `Controller::openUrl` (`Controller.cpp:563-568`), restored for
queued clicks (`Controller.cpp:625-628`), and copied through the instance
frame (`main.cpp:156-168`, `SingleInstance.cpp:99`).

On Windows, foreground rights move with `AllowSetForegroundWindow` (or by
being the foreground process). An env var on the child does not do that.
A Windows `requestActivationToken` can only return a dummy string unless
`launchTarget` grows another `#ifdef` beside the macOS `open` branch
(`launcher.cpp:382`). The seam describes a token, and the spawn site
assumes that token is this specific variable.

`main.cpp:171-179` treats `registerActivation` failure as fatal and prints
"could not register D-Bus activation". The signature
(`platform.h:66-67`) is fine, and macOS shows that returning `true` with
no registration is allowed (`platform_macos.mm:380-387`). A Windows
backend can do the same for argv delivery. Protocol registration
(ProgId / `HKCU\Software\Classes`, command `"lane.exe" "%1"`) has no
function on `Platform`. Second-instance forwarding already goes through
the JSON socket, which is separate from D-Bus (`DBusActivation.cpp` is
compiled only for `LANE_KDE_DESKTOP`, `src/CMakeLists.txt:44-48`).

### P2. Tray and row icons assume a freedesktop theme, except the macOS tray glyph

`ThemeIconProvider::requestPixmap` (`ThemeIconProvider.cpp:31-36`) calls
`QIcon::fromTheme`, then `application-x-executable`, then `image-missing`.
`LaneIcon.qml:4-13` sends every row icon through that provider.
`bundle:` icons resolve only via `macBundlePathForId`
(`ThemeIconProvider.cpp:24-29`), which reads the macOS scan map.

The tray icon is a bundled SVG only under `Q_OS_MACOS`
(`Controller.cpp:58-63`). Every other OS, including Windows, uses
`QIcon::fromTheme("app.lane.Lane")` (`Controller.cpp:64-66`). Windows
installs no hicolor theme, so the tray image is null. `QSystemTrayIcon`
itself (`Controller.cpp:67-78`) is the right widget. Analysis row 19 is
the macOS case where some theme lookups miss; the Windows tray is the
`#else` branch the macOS fix does not cover.

Picker chrome labels are macOS versus everyone else (`Picker.qml:19-27`):
`Qt.platform.os === "osx"` gets command/option glyphs, and every other OS
gets `^C` / `alt+p`. Windows would show the Linux strings.
`StandardKey` bindings stay valid.

### P2. Default-handler watching is mimeapps or a one-minute poll

`watchesDefaultHandlerByFile()` is the right switch.
`Controller.cpp:122-133`: true constructs `DefaultBrowserWatcher`; false
starts a 60s `QTimer` that calls the same `onMimeappsChanged` slot.
macOS returns false and the poll talks to Launch Services. A Windows
backend can return false and implement `defaultHandlerState()` /
`makeDefaultHandler()` against the http/https UserChoice keys. Nothing
does that today. The only readers are `xdg-mime`
(`platform_linux.cpp:184-212`) and `NSWorkspace`
(`platform_macos.mm:46-68`).

`DefaultBrowserWatcher::mimeappsPaths` (`DefaultBrowserWatcher.cpp:6-16`)
is hard-wired to `mimeapps.list` under `ConfigLocation` and
`ApplicationsLocation`. It is not used when the flag is false. Turning
the flag on for Windows would watch the wrong files.

Linux autostart writes
`ConfigLocation/autostart/app.lane.Lane.desktop` (`Autostart.cpp:15-52`)
and is only linked into the KDE backend. macOS uses `SMAppService` inside
`platform_macos.mm`. `Platform::autostartSet` / `autostartStatus`
(`platform.h:37-39`) can host a Run-key or Startup-folder backend. No such
backend exists, and `Autostart.cpp` is not the file to extend for Windows
while it remains inside the `LANE_KDE_DESKTOP` source list.

Notifications have the same shape: `notify()` (`platform.h:24-27`) is
generic; the bodies are freedesktop D-Bus (`platform_linux.cpp:93-141`)
and `UNUserNotificationCenter` (`platform_macos.mm:418-453`). Windows
toast actions need a third body. `notificationsDenied()` defaulting to
false (`platform.h:86`) matches the Linux comment and is overridable.

### P2. The exec blocklist does not treat Windows extensions as the same program

Fixed for the named basenames. `isBlockedInterpreter` compares
`QFileInfo(exec).fileName().toLower()` and does not strip `.exe`. The set
already had `cmd.exe`, `powershell`, and `pwsh`. It now also has
`powershell.exe`, `pwsh.exe`, `python.exe`, `wscript.exe`, `cscript.exe`,
and `mshta.exe`. `windowsInterpreterBasenamesAreBlocked` checks a full
`powershell.exe` path and leaves `firefox.exe` alone. `osChainBlocked`
still selects the macOS chain only under `Q_OS_MACOS`; a Windows build
would use this shared set. `config.cpp` loads custom targets through the
same helpers, so the new names apply there too.

### P3. Single-instance tests are POSIX and excluded until a desktop flag is on

`tests/CMakeLists.txt:37-42` builds `test_singleinstance` only when
`LANE_KDE_DESKTOP OR LANE_MAC_DESKTOP`. The test includes `<sys/stat.h>`
unconditionally (`tests/test_singleinstance.cpp:16`) and identifies the
bound socket by `ino_t` / `::stat` / `st_ino`
(`tests/test_singleinstance.cpp:85-93`). Widening the CMake gate to
`WIN32` without replacing that helper fails the test compile. The comment
at `tests/CMakeLists.txt:38-39` calls the production code cross-platform
via `XDG_RUNTIME_DIR` or `confstr`. The Windows `RuntimeLocation` behavior
above is outside that comment.

### P3. Overlay QML flags are portable; the platform configure step is not

`Picker.qml:10` and `Hold.qml:10` set
`Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint` and a transparent,
full-screen `Window`. `Controller` always calls
`platform()->configureOverlayWindow` (`Controller.cpp:1215`,
`:1319`). Linux applies layer-shell anchors and exclusive keyboard
(`platform_linux.cpp:215-237`) and returns immediately when layer-shell
is null (`:217-220`). macOS sets the same Qt flags again and installs an
AppKit show-filter (`platform_macos.mm:369-377`). A Windows function has
to set topmost/tool-window extended styles and per-monitor geometry
itself. The QML flags are a start; they do not grab the keyboard the way
layer-shell does, and they are not a substitute for that function.

`SourceInfo::activeSource` (`SourceInfo.cpp:5-10`) returns an empty
source on every OS. Process-name and window-title rules stay inert.
That is the Linux Wayland behavior, carried forward.

## Windows-ready checklist

| Subsystem | Status | Where | Blocker |
|---|---|---|---|
| Socket path | not-ready | `SingleInstance.cpp:88-90`, `:146-177`, `:207` | Unguarded `stat`/`S_ISSOCK`. Path is `XDG_RUNTIME_DIR` or `RuntimeLocation` plus `/lane-instance.sock`. On Windows Qt 6.8, `RuntimeLocation` is the user profile, and `QLocalServer::listen` turns that string into a named pipe. Lock file is `<pipe-string>.lock` in the profile. Not a `Platform` method. |
| Activation | needs-work | `platform.h:66-76`; `main.cpp:171-179`; `launcher.cpp:444-457`; `platform_macos.mm:380-397` | Callback signature can no-op the way macOS does, and argv already reaches `Controller::handleArgs`. ProgId / protocol-handler registration is absent. Focus handoff only publishes `XDG_ACTIVATION_TOKEN`. Failure text says D-Bus. |
| Default handler | needs-work | `platform.h:41-55`; `Controller.cpp:122-133`; `DefaultBrowserWatcher.cpp:6-16`; `platform_linux.cpp:184-212` | Poll path is the right hook when `watchesDefaultHandlerByFile()` is false. No UserChoice / `IApplicationAssociationRegistration` reader or setter. The file watcher only knows `mimeapps.list`. |
| Overlay | needs-work | `Picker.qml:10`; `Hold.qml:10`; `Controller.cpp:1215`; `platform_linux.cpp:215-220`; `platform_macos.mm:369-377` | QML already asks for a borderless topmost window. `configureOverlayWindow` exists only as layer-shell and AppKit. No Win32 extended-style or per-monitor setup. |
| Autostart | not-ready | `platform.h:37-39`; `Autostart.cpp:15-52`; `src/CMakeLists.txt:44-48` | Interface is enough for a Startup-folder or Run-key backend. The only implementations are an XDG desktop file (Linux TU) and `SMAppService` (macOS TU). |
| Discovery | not-ready | `discovery.cpp:378-387`, `:569-637`, `:1475-1531` | `*.desktop` scan plus optional `.app` bundles. Profile table is linux or mac. `ApplicationsLocation` on Windows is Start Menu Programs. No App Paths / registry scan. Flatpak dirs are appended on every OS. |
| Launch | needs-work | `launcher.cpp:52-54`, `:269-271`, `:382-394`, `:424-457`; `discovery.cpp:1454-1463` | `QProcess::startDetached` can run an absolute `.exe`. Email action is `xdg-email` outside macOS. `open -b` is correctly `Q_OS_MACOS`. The named `*.exe` interpreters are now in the shared blocklist. Another OS branch would still extend the `Q_OS_MACOS` chain in `launchTarget`. |
| Notifications | not-ready | `platform.h:24-27`, `:83-86`; `platform_linux.cpp:93-141`; `platform_macos.mm:418-453` | Signature (summary, body, one action) can host a toast. Bodies exist only for freedesktop D-Bus and UserNotifications. Icon argument is a theme name. |
| Icon theme | not-ready | `ThemeIconProvider.cpp:24-36`; `LaneIcon.qml:4-13` | Every non-`bundle:` icon is `QIcon::fromTheme`. Windows has no freedesktop theme, so row icons and the fallback names resolve empty. `bundle:` lookup is the macOS path map. |
| Tray | needs-work | `Controller.cpp:58-78`, `:85-88`; `Controller.h:105-110` | `QSystemTrayIcon` and its menu are cross-platform. The glyph is `fromTheme("app.lane.Lane")` except on macOS, where a bundled mask SVG is used. Quit stays off because that gate is `Q_OS_MACOS` only. |

Config storage is in better shape than those rows.
`defaultConfigPath` (`config.cpp:167-169`) is
`QStandardPaths::ConfigLocation + "/lane/config.json"`. On Windows Qt 6.8,
`ConfigLocation` is `FOLDERID_LocalAppData` plus organization and
application (`qstandardpaths_win.cpp`). With `main.cpp:76-78` setting
organization `lane` and application `Lane`, the file lands under
`%LOCALAPPDATA%\lane\Lane\lane\config.json`. That directory is writable.
It is a different layout than `~/.config/lane`, and it does not need a
new API.

## Considered and fine

- `Platform` (`platform.h:18-87`) does not mention D-Bus types, POSIX
  paths, or AppKit types. `configureOverlayWindow(QWindow *)`,
  `registerActivation(handleArgs, openUrl)`, and `DefaultHandlerState`
  can be implemented for the registry and for a topmost Win32 window.
- `getuid` and `confstr(_CS_DARWIN_USER_TEMP_DIR)` stay inside
  `Q_OS_MACOS` (`SingleInstance.cpp:150-165`). The tests that call
  `getuid` do too (`tests/test_singleinstance.cpp:17-18`, `:297-305`).
- `/usr/bin/open` is spawned only inside `Q_OS_MACOS`
  (`launcher.cpp:382-394`). `macFixedOpenArgs` is pure `QString` work and
  is safe to compile everywhere (`launcher.h:35-47`).
- Binary plist conversion is `Q_OS_MACOS`-only (`discovery.cpp:1301-1316`).
  Other hosts skip `bplist` files.
- `Autostart.cpp`, `DBusActivation.cpp`, LayerShellQt, Plasma Activities,
  and the systemd/D-Bus install rules are compiled or installed only for
  the KDE desktop (`src/CMakeLists.txt:44-48`, `CMakeLists.txt:65-75`,
  root `CMakeLists.txt` install block). They will not be pulled into a
  Windows target that follows the macOS `elseif` shape.
- `UrlEventFilter` is installed only under `Q_OS_MACOS` (`main.cpp:83-89`).
  The class itself is a Qt event filter and already builds on Linux for
  tests.
- QML imports Qt Quick, Qt Quick Controls, and `app.lane.ui`
  (`Picker.qml:1-5`). The style is forced to Fusion before
  `QApplication` (`main.cpp:71`). There is no Kirigami runtime import.
  `platform.h:16-17` still says the rest of the app is Qt/Kirigami and
  never touches the seam. The UI half of that sentence is stale; the
  "never touches" half is false (`Controller`, `main.cpp`,
  `ThemeIconProvider`, `SingleInstance`, `CrashLog`, `discovery.cpp`,
  `launcher.cpp`).
- `QLockFile` (`SingleInstance.cpp:207-208`) is a reasonable cross-platform
  lock. The problem is the path it locks, listed above.
- `SourceInfo` being empty does not hide an X11 or Wayland call that would
  fail to compile on Windows.

## Open questions

- Should a Windows Lane quit like the macOS status item
  (`Controller.cpp:85-88`) or stay a no-quit daemon the way Linux does?
  The current `#else` chooses the daemon.
- Should `singleInstanceSocketPath()` move onto `Platform`, or should
  Windows keep a fixed pipe name such as `lane-<user-sid>` inside
  `SingleInstance.cpp` under `Q_OS_WIN`? The reclaim logic around
  `S_ISSOCK` has to change either way, because a pipe has no socket inode.
- Is v1 default-browser support allowed to call
  `IApplicationAssociationRegistration`, or should Settings only open the
  Windows default-apps page and report `Unknown` until the user returns?
  The status enum can carry either outcome.
- For focus, is the macOS empty-token behavior acceptable on Windows
  (child may open behind the current app), or does `launchTarget` need a
  real foreground grant before `startDetached`?
