# AGENTS.md

Guide for AI agents (Claude Code, Codex, and similar) that need to
configure or inspect Lane without a GUI.

The `version` key in `config.json` below is a schema version integer
for the config file format. It is not the app version. The app
version comes from CMake (`project(lane VERSION x.y.z)` in
`CMakeLists.txt`) and shows up in `lane --version` and the Settings
sidebar. Don't confuse the two.

## Config file

Path: `~/.config/lane/config.json`

A JSON schema is at `docs/config.schema.json`. The inline shape below
covers the same fields for quick reference.

The settings window writes this same file. You can edit it by hand or
with a script. A running daemon keeps its config in memory until you
reload it. After editing config.json, ask the running instance to
reload:

```bash
lane --rediscover
```

That contacts the running daemon over D-Bus when one is already up (the
new process hands off and exits). If no daemon is running, it starts the
full background process: reloads `config.json`, rescans browser targets,
and stays resident. It is not a lightweight one-shot CLI that exits after
the reload.
Flatpak-packaged browsers (Zen, Firefox, Brave, and others) are discovered
too; their profile data lives under `~/.var/app/<app-id>/` instead of the
native paths. To restart the whole process instead:

```bash
pkill -f "lane --daemon"
lane --daemon &
```

## JSON shape

Top-level keys:

```json
{
  "version": 1,
  "pickerPolicy": "no-rule",
  "closeOnFocusLoss": true,
  "showUrl": true,
  "toast": true,
  "toastMs": 2800,
  "unwrapO365": true,
  "unshorten": true,
  "openUnwrapped": false,
  "preferPwa": true,
  "holdAutoOpen": false,
  "holdMs": 1600,
  "autostart": false,
  "defaultTargetId": "",
  "hiddenTargetIds": [],
  "targetOrder": [],
  "targetAliases": {},
  "remembered": {},
  "recentTargetIds": [],
  "rules": [],
  "customTargets": [],
  "substitutions": []
}
```

### pickerPolicy

Controls when the picker overlay appears.

- `"no-rule"` - show picker when no rule matched (default)
- `"always"` - always show picker
- `"conflict"` - show picker only when multiple targets match
- `"never"` - never show picker, use default target

### holdAutoOpen / holdMs

Hold is a Gmail-undo style bar for convenience opens. When the routing
decision is `remembered`, `pwa`, or `default` and `holdAutoOpen` is
true (default false), Lane waits `holdMs` milliseconds (default 1600)
before launching. Enter opens immediately. Esc or Space cancels the hold
and shows the picker. Rules skip the hold and open immediately.

### rules

Array of rule objects. Rules are checked in order. First match wins.

```json
{
  "id": "uuid-string",
  "pattern": "github.com/bitskc",
  "scope": "path",
  "location": "url",
  "regex": false,
  "targetId": "target-id-from-list",
  "enabled": true
}
```

- `scope`: `"any"`, `"domain"`, or `"path"`. Path scope matches the full
  URL path, not just the host.
- `location`: `"url"` (the only value that can match; `"title"` and
  `"process"` are parsed for compatibility but cannot match on Wayland
  because the active caller identity is not available).
- `regex`: if true, `pattern` is a regular expression. Keep regex
  patterns anchored and length-capped. Avoid patterns that can match
  arbitrarily long strings.
- `targetId`: the target ID from `lane --list`. Must match exactly.

To add a rule safely:

1. Run `lane --list` to get the target ID you want.
2. Run `lane --explain <url>` to confirm your pattern matches.
3. Add the rule to the `rules` array with a unique `id` (any UUID).
4. Run `lane --rediscover` so the running daemon picks up the change.

Do not point rules at custom handlers that run wild interpreter commands
(`python -c`, `bash -c`, `sh -c`). Custom handlers run as argv, not
through a shell, but a rule that routes to a dangerous handler is still
dangerous.

### remembered

Object mapping destination keys to target IDs. Keys are path-scoped
destinations like `github.com/bitskc`, not just bare hosts.

```json
{
  "remembered": {
    "github.com/bitskc": "firefox-default",
    "mail.google.com": "zen-work"
  }
}
```

The picker writes to this map when you press Alt+A (always for this
site). Comma narrows the remembered path, dot widens it.

### customTargets

Array of custom target objects for apps or handlers that Lane does not
auto-discover. Each has `id`, `name`, `exec`, `args`, `icon`, and
optionally `browserName` and `kind`.

On load, every custom target becomes a custom app (`Kind::Custom`). The
loader ignores any `kind` value in the file. On save, the settings UI
writes `kind` (usually `"app"`) and `browserName`, but those fields do
not change how a hand-edited entry is loaded.


### Containers

Firefox/Zen contextual identities (containers) show up in `lane --list`
as their own targets, kind `"container"`. The ID looks like
`browser:zen:sahyoxd1.Default (release):container:2`: the parent
profile's ID with `:container:<userContextId>` appended. Rules can
point `targetId` at one of these the same as any other target.

Lane only lists containers for a profile where it can tell a
protocol-handler extension (Open URL in Container, Default Container
Handler, or similar) is installed. Without one, the browser has
nothing to act on an `ext+container` link with, so Lane doesn't offer
the container as a target at all.

Container launch args use a `$urlEncoded` placeholder (the http(s)
URL, percent-encoded) instead of `$url`, since the URL is embedded
inside an `ext+container:name=...&url=...` query value rather than
passed on its own. You don't need to do anything with this when
writing a rule; just set `targetId` to the container's ID.

### targetOrder

Array of target IDs in display order. The settings page and the picker
follow this order after matching web apps and remembered targets. Empty
or missing means discovery order. IDs not in the list keep their discovery
order after the listed ones.

### targetAliases

Object mapping target IDs to custom display names. Empty value or missing
key means use the discovered name. Rename a target in settings to set or
clear an alias.

### substitutions

Array of find/replace pairs applied to the URL before matching.

```json
{
  "find": "old-string",
  "replace": "new-string",
  "regex": false
}
```

## CLI inspection

```bash
lane --version                 # print the installed version and exit
lane --help                    # print usage and exit
lane --config-path             # print the config file path and exit
lane --list                    # print all discovered targets with IDs
lane --explain https://example.com   # print the routing decision for a URL
lane --settings                # open settings
lane --configure               # alias for --settings
lane --rediscover              # reload config/rescan via running daemon (starts full process if none)
lane --daemon                  # run in the background without opening settings
lane -p https://example.com    # route a URL and force the picker
```

`--explain` shows the matched URL, host, action (launch/pick/copy),
reason, target, and `memoryKey` when the decision came from a
remembered destination. Use it to verify rules before committing them.

## Restart note

Lane runs as a resident daemon with a unique D-Bus name
(`app.lane.Lane`). Only one instance runs at a time. The settings window
writes config while the daemon runs. If you edit config.json directly,
run `lane --rediscover` to reload the file and refresh targets. That
contacts the running daemon over D-Bus when one is already up; if none
is running, it starts the full background process and stays resident.
Restart the daemon if you prefer a full process reset.
