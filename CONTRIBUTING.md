# Contributing

Tern is a small project. Patches welcome.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
```

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
- Config lives in `~/.config/tern/config.json`. The settings window and
  manual edits write the same file.

## License

By contributing, you agree your changes are licensed under the PolyForm
Noncommercial License 1.0.0, the same as the rest of the project.
