# Tern

Plasma-native link router (Qt 6 / Kirigami).

## Skill routing

When the user's request matches an available skill, invoke it via the Skill tool. When in doubt, invoke the skill.

## Layout

- `src/core` — discovery, pipeline, matcher, router, launcher. No UI. Covered by `tests/`.
- `src/app` — resident controller, D-Bus unique instance, picker/settings engines.
- `src/qml` — picker overlay and Kirigami settings.
- `AGENTS.md` — config JSON shape and CLI for AI agents.

## Invariants

- Do not change the user's default browser without the settings button.
- O365 unwrap is for matching only unless `openUnwrapped` is set.
- Hidden targets stay hidden across rediscover.
- Picker must be a layer-shell overlay, not a normal Wayland window.
- Config is `~/.config/tern/config.json`. No live reload; restart daemon after edits.

## License

PolyForm Noncommercial 1.0.0. Commercial use needs a separate license from Andy Hayes. See `LICENSE` and `COMMERCIAL.md`.

## Commands

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
ctest --test-dir build --output-on-failure
```
