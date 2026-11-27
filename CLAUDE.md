# Lane

Plasma-native link router (plain Qt 6).

## Skill routing

When the user's request matches an available skill, invoke it via the Skill tool. When in doubt, invoke the skill.

## Layout

- `src/core` — discovery, pipeline, matcher, router, launcher. No UI. Covered by `tests/`.
- `src/app` — resident controller, D-Bus unique instance, picker/settings engines.
- `src/qml` — picker overlay and Qt Quick Controls settings
- `AGENTS.md` — config JSON shape and CLI for AI agents.

## Invariants

- Do not change the user's default browser without the settings button.
- O365 unwrap is for matching only unless `openUnwrapped` is set.
- Hidden targets stay hidden across rediscover.
- Picker must be a layer-shell overlay, not a normal Wayland window.
- Config is `~/.config/lane/config.json`. No automatic live reload; after editing it directly, run `lane --rediscover` (or restart the daemon) so the running process picks it up.

## License

PolyForm Noncommercial 1.0.0. Commercial use needs a separate license from Andy Hayes. See `LICENSE` and `COMMERCIAL.md`.

## Commands

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
ctest --test-dir build --output-on-failure
```

## gstack

gstack is required for AI-assisted work in this repo. A PreToolUse hook
(`.claude/hooks/check-gstack.cjs`, registered in `.claude/settings.json`)
blocks every Skill tool call the model makes until gstack is installed
globally. It is a workflow gate, not a security control: a `/command` the
user types, or a SKILL.md read directly, is not gated. The hook needs Node.js
on PATH and Claude Code 2.1.295 or later: older versions ignore `onFailure`
and let a hook that fails to run through. Start Claude Code from the repo
root: a session started in a subdirectory, or moved with `/cd`, does not load
this `.claude/settings.json`, so skills are not gated there.

If a skill is blocked, stop and tell the user to install gstack:

```bash
git clone --depth 1 https://github.com/garrytan/gstack.git ~/.claude/skills/gstack
cd ~/.claude/skills/gstack && ./setup --team
```

`--team` also turns on gstack auto-update at the start of each Claude Code
session. Restart the AI coding tool after installing. Do not run the install
yourself, skip skills, or work around the block.

If the block says node was not found (`node: not found` or
`node: command not found`), the user needs Node.js. If it says
`Cannot find module ... check-gstack.cjs`, the hook file is missing from this
checkout.

Use the /browse skill from gstack for all web browsing. Never use mcp__claude-in-chrome__* tools.
