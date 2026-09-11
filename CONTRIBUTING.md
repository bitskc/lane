# Contributing

Lane is a small project. Patches welcome.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Adding a browser family

Most new browser support lands in `src/core/discovery.cpp`.

1. **`fingerprint()`** matches a `.desktop` entry by scanning its exec
   basename, name, id, and `StartupWMClass` together. Order matters:
   checks run top to bottom, and substring matches collide (for example
   `firefox` must come after the `firefoxpwa` exclusion, and `zen`
   before `firefox`). A match sets `Engine::Gecko` or `Engine::Chromium`
   plus the profile data directory. Unknown browsers fall through to
   `Engine::Generic`.
2. **Profile walk.** Gecko installs call `geckoProfiles()`, which reads
   `profiles.ini` under the fingerprinted data dir. Chromium installs
   call `chromiumProfiles()`, which reads `Local State`. Generic engines
   get a single default target from `genericBrowser()`.
3. **Tests.** Add a fixture tree under `tests/fixtures/` (desktop entry,
   profile store, and any `prefs.js` files the walk expects) and assert
   the new targets in `tests/test_discovery.cpp`. Copy whole Gecko trees
   with the helper there when profiles need real directories on disk.

Two traps to expect:

- A Gecko profile row is skipped when its directory is missing or has no
  `prefs.js`, even if `profiles.ini` still lists it.
- Firefox/Zen containers appear only when a protocol-handler extension
  is detected in the profile's `extensions.json` (or a weaker fallback
  when that file cannot be read).

## Rules

- Keep changes focused. One feature per PR.
- User-facing changes need a bullet under `## [Unreleased]` in
  `CHANGELOG.md`. See `docs/RELEASING.md` for the format.
- Version bumps only happen in a release commit, following
  `docs/RELEASING.md`. Don't bump `CMakeLists.txt`'s `VERSION` in a
  feature PR.
- Do not change the user's default browser without the settings button.
- Custom handlers run as argv, not through a shell. Keep it that way.
- http and https only. Do not add handlers for file, javascript, or
  data URLs.
- Picker is a Wayland layer-shell overlay. Do not make it a normal
  window.
- Config lives in `~/.config/lane/config.json`. The settings window and
  manual edits write the same file.

## License

By contributing, you agree your changes are licensed under the PolyForm
Noncommercial License 1.0.0, the same as the rest of the project.
