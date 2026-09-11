# AGENTS.md

Guide for AI agents (Claude Code, Codex, and similar) that need to
configure or inspect Tern without a GUI.

## Config file

Path: `~/.config/tern/config.json`

The settings window writes this same file. You can edit it by hand or
with a script. Tern loads config at startup. There is no live reload
yet. After editing config.json, restart the daemon:

```bash
pkill -f "tern --daemon"
tern --daemon &
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
  "holdAutoOpen": true,
  "holdMs": 1600,
  "autostart": false,
  "defaultTargetId": "",
  "hiddenTargetIds": [],
  "recentTargetIds": [],
  "remembered": {},
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

When a silent open has no rule and no remembered choice, Tern waits
`holdMs` milliseconds (default 1600) before auto-opening the default
target. During that hold, Enter opens immediately and Esc or Space shows
the picker. Set `holdAutoOpen` to false to disable the auto-open and
always show the picker for silent opens.

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
- `location`: `"url"`, `"title"` (window title), or `"process"` (process
  name of the app that opened the link).
- `regex`: if true, `pattern` is a regular expression. Keep regex
  patterns anchored and length-capped. Avoid patterns that can match
  arbitrarily long strings.
- `targetId`: the target ID from `tern --list`. Must match exactly.

To add a rule safely:

1. Run `tern --list` to get the target ID you want.
2. Run `tern --explain <url>` to confirm your pattern matches.
3. Add the rule to the `rules` array with a unique `id` (any UUID).
4. Restart the daemon.

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

Array of custom target objects for apps or handlers that Tern does not
auto-discover. Each has `id`, `name`, `exec`, `args`, `icon`, and `kind`
(`"app"`, `"action"`, `"browser"`, or `"pwa"`).

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
tern --list                    # print all discovered targets with IDs
tern --explain https://example.com   # print the routing decision for a URL
```

`--explain` shows the matched URL, host, action (launch/pick/copy),
reason, and target. Use it to verify rules before committing them.

## Restart note

Tern runs as a resident daemon with a unique D-Bus name
(`app.tern.Tern`). Only one instance runs at a time. The config is
loaded at startup. The settings window can persist changes while the
daemon runs, but if you edit config.json directly, the running process
will not pick up changes until you restart it.
