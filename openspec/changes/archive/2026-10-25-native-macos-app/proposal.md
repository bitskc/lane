# Proposal: native-macos-app

## Why
Lane routes link clicks on Linux/KDE. On macOS only `lane-core` and its 13 tests build today (`CMakeLists.txt` and `src/CMakeLists.txt` gate the application to `UNIX AND NOT APPLE`). There is no `lane` binary and no `Lane.app` on the maintainer's MacBook (macOS 26.6.1, Apple Silicon, probed 2026-10-14), so a Mac user cannot run Lane at all.

The Linux port work already moved the OS-specific seams (notify, autostart, activation token) behind `Platform` and removed the KDE Frameworks dependency. What is left in the way is a short, known list:

- `Controller.cpp` calls `xdg-mime` to read and set the default browser (`refreshDefaultBrowserState`, `makeDefaultBrowser`).
- `Controller.cpp` configures a Wayland layer-shell window (`configureLayerShell`).
- `SingleInstance.cpp` builds its socket path from `XDG_RUNTIME_DIR`.
- `DefaultBrowserWatcher` watches `mimeapps.list`.
- Discovery reads freedesktop `.desktop` files and Linux profile directories.
- `action:email` execs `xdg-email`.
- macOS delivers link clicks as `QFileOpenEvent`, not argv, and nothing handles that event.

Two older documents say not to do this (`docs/designs/port-assessment.md` "do not port"; `docs/plan-windows-port.md` "macOS not scheduled here"). Both predate the Platform seam and the Qt 6 port. This change supersedes them and annotates them.

## Delivery scope (owner decision D1, 2026-10-14)
"Your Mac first": a complete Apple-Silicon `Lane.app` with bundled Qt, locally signed, installed over the tailnet to the maintainer's MacBook and verified end to end on that machine.

Out of this change, tracked as separate work: Developer ID signing and notarization, Intel or universal builds, a downloadable release artifact, Homebrew cask.

## What changes
- A `LANE_MAC_DESKTOP` application target that produces a relocatable `Lane.app` (bundle id `app.lane.Lane`) from the same C++ core and the same QML tree used on Linux. No second UI.
- `Platform` grows five methods (default-handler query and set, file-watch flag, overlay window setup, activation registration) and gains a macOS implementation in Objective-C++ (one file, `platform_macos.mm`). Linux bodies move behind the same methods with no behavior change. `Controller` also starts calling `platform()->autostart*` instead of the free functions, because the seam methods are unused today.
- A macOS URL path: `QFileOpenEvent` is captured at process start, buffered until the single-instance claim and `Controller` exist, then delivered through the same `Controller::openUrl` that argv and the socket use.
- A macOS `.app` discovery front end that reads `Info.plist`, keeps the existing Gecko, Chromium, container and PWA parsers, and swaps in a per-OS profile-root table.
- macOS behavior for notifications (`UNUserNotificationCenter`), login item (`SMAppService`), default handler (`NSWorkspace`), status-item menu, overlay window, footer key hints, and appearance.
- A packaging and verification gate that runs the real app on the MacBook, not only `ctest`.

## Capabilities
- `cross-platform`: ADDED requirements for the macOS build, URL delivery, single instance, default handler, notifications, login item, overlay window, discovery, launch, and packaging.
- `lane`: one MODIFIED requirement ("System tray presence": macOS status-item menu adds Quit Lane; Linux wording and both original scenarios are kept) and ADDED requirements for macOS key hints, Dock/quit/Settings window behavior, default-handler consent states, and appearance/displays.

No other existing requirement is modified or removed. Linux behavior in the canonical specs is unchanged and must stay green.

## Non-goals
- No Electron, Tauri, SwiftUI or web rewrite, and no second QML tree.
- No Mac App Store, sandbox, notarization or Developer ID work (D1).
- No interception of `mailto:` or PDF handlers, no browser extensions, no Accessibility permission, no event taps.
- No invented Safari profile or private-window routes. Safari is one normal target.
- No `open -a` inside user `custom:` handlers. Custom targets that name `open` or a `.app` fail closed.
- No change to the default browser from a shell or a test. Defaults change only from the in-app button.
- `SourceInfo` stays an empty stub on macOS (same as Linux today).

## Impact
- New: `src/app/platform/platform_macos.mm` (the only Objective-C++ file), a pure-Qt bundle scanner in `lane-core` (`src/core/bundleinfo.{h,cpp}` plus discovery changes), macOS `Info.plist` template, `scripts/` bundle and verify scripts, `tests/fixtures/mac/`, new tests.
- Edited: both `CMakeLists.txt`, `platform.h`, `platform_linux.cpp`, `Controller.{h,cpp}`, `SingleInstance.cpp`, `main.cpp` (activation through the seam), `discovery.cpp` (profile-root table), `launcher.cpp` (macOS-only guard additions and `open` construction), `types.h` (bundle fields on `Target`, not serialized), `Picker.qml` (hint and accessible strings from one table), `Settings.qml` (quit shortcut), docs.
- Linux: zero behavior change, enforced by running the full Linux ctest and the existing native smoke before and after the seam refactor.

## Open owner decisions
D1 is decided. D2 is decided (owner, after reviewers split on it). D3 to D5 carry a default in `design.md`; the specs encode the defaults and each can be reversed without redoing the others.

| ID | Question | Default encoded |
|----|----------|-----------------|
| D2 | Does macOS Quit exist? | **Decided 2026-10-14:** Quit Lane in the status-item menu (documented macOS exception to the Linux no-Quit rule); one quit path that cancels a pending pick first |
| D3 | Dock icon? | None (`LSUIElement=1`), matching the Linux tray-only daemon |
| D4 | Option or Command accelerators for always/private/email? | Keep Option (`Alt+` in QML); ⌘C stays copy; verify on device, fallback is ⌘ variants |
| D5 | Picker over a full-screen Space? | Show on the active Space with full-screen-auxiliary behavior; verify on device |
