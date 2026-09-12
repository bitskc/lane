# Review: PR #1 — Round 2 (post-response-commits)

Re-reviewed at commit `c5f4e2c` (branch `lane-rename-and-review-fixes` into `main`).
Response commits: `47c1d7e` (bypass closures + review fixes) and `c5f4e2c` (CI fix).

Source read in full: `src/core/launcher.cpp`, `launcher.h`, `src/core/config.cpp`,
`src/app/Controller.cpp`, `src/app/PickerModel.cpp`/`.h`, `src/qml/Picker.qml`,
`src/qml/pages/TargetsPage.qml`, `src/app/main.cpp`, `data/app.lane.Lane.notifyrc`,
`.github/workflows/ci.yml`. All tests read: `tests/test_launcher.cpp`,
`tests/test_config.cpp`, `tests/test_pipeline.cpp`.

Build: `cmake --build build` (up to date). `ctest --test-dir build --output-on-failure`:
10/10 pass.

`QFileInfo::symLinkTarget()` verified to return the **immediate** hop target (not
canonical), confirming `isBlockedInterpreterChain()` walks hop-by-hop as intended:
a two-link chain `linkA → linkB → /bin/sh` yields `symLinkTarget(linkA) = linkB`,
`symLinkTarget(linkB) = /bin/sh`, each checked by basename in sequence.

## Verdict

**APPROVE.** Both original blocking bypasses are closed. All three should-fix
items are addressed. Tests are discriminating. Residual risk is honestly stated
and appropriate for the threat model.

**Blocking: 0. Should-fix: 0. Follow-up: 1 (informational).**

## Blocking findings (round 1) — re-verification

### 1. Symlink with an innocent name — CLOSED

**Old code** (`c578cac`): `launchTarget()` used `target.exec` verbatim for
absolute paths (`QFileInfo(target.exec).isAbsolute() ? target.exec :
resolveExecutable(target.exec)`), then checked only `isBlockedInterpreter(exe)`
(basename). `targetFromJson()` checked only `isBlockedInterpreter(t.exec)`. A
symlink `/tmp/mybrowser → /bin/bash` passed both and launched bash.

**New code** (`47c1d7e`):

- `launchTarget()` (line 227): unconditionally calls `resolveExecutable(target.exec)`
  for all paths, absolute or relative. No more verbatim-absolute shortcut.
- `resolveExecutable()` (line 162): for absolute paths, returns `trimmed` (not
  `canonicalFilePath()`), preserving the first hop's basename for the chain check.
  Deliberate, with an explanatory comment.
- `isBlockedInterpreterChain()` (lines 112–142): walks every symlink hop via
  `QFileInfo::symLinkTarget()`, checking each hop's basename against
  `isBlockedInterpreter()`. `launchTarget()` calls this at line 235.
- `targetFromJson()` (lines 142–148): first checks `isBlockedInterpreter(t.exec)`
  (catches literal blocked names and unresolvable relative names), then if not
  blocked, resolves and calls `isBlockedInterpreterChain(resolved)`. An exec that
  doesn't resolve is left in place (launchTarget will refuse it later; dropping it
  here would lose the config entry on next save).

**Proof:** `test_launcher.cpp:launchTargetRejectsSymlinkToBlockedInterpreter`
creates a real symlink `totally-a-browser → /bin/sh`, sets `t.exec` to that path,
and asserts `!launchTarget(...)`. Passes. Against the old code, `launchTarget()`
would use the path verbatim, `isBlockedInterpreter` would see basename
`totally-a-browser` (not blocked), and the process would spawn — test would fail.

`test_config.cpp:customTargetsDropSymlinkToBlockedInterpreter` writes the same
symlink exec into a `config.json` and asserts `loadConfig` drops it (only the
`ok` entry survives). Against old code, `targetFromJson` had no chain walk; the
entry would load — test would fail.

### 2. Re-exec wrapper (`env`) — CLOSED

**Old code:** `env` was not in `blockedInterpreters()`. `exec: "/usr/bin/env"`,
`args: ["bash", "-c", ...]` passed `isBlockedInterpreter()` (basename `env` not
blocked) and launched bash via env's re-exec.

**New code:** `env` and 17 other re-exec/process-wrapper binaries added to
`blockedInterpreters()`: `xargs`, `nohup`, `setsid`, `timeout`, `stdbuf`, `nice`,
`ionice`, `watch`, `sudo`, `doas`, `pkexec`, `systemd-run`, `flatpak-spawn`,
`ssh`, `awk`, `gawk`, `find`, `sed`. Each is blocked by basename at the first hop
of `isBlockedInterpreterChain()`, so neither `launchTarget()` nor
`targetFromJson()` lets one through.

**Proof:** `test_launcher.cpp:launchTargetRejectsEnvReExecWrapper` sets
`exec="/usr/bin/env"`, `args=["bash","-c","echo pwned","$url"]`, asserts
`!launchTarget(...)`. Passes. Against old code, `env` wasn't blocked and the
process would spawn — test would fail.

`test_config.cpp:customTargetsDropEnvReExecWrapper` writes the same into
`config.json`, asserts `loadConfig` drops it. Against old code, the entry would
load — test would fail.

### resolveExecutable() canonicalization — CONFIRMED FIXED

Old code: `return info.isExecutable() ? info.canonicalFilePath() : QString();`
— collapsed `python3 → python3.14` straight to `python3.14`, laundering the
blocked basename `python3` past `isBlockedInterpreter()`.

New code: `return info.isExecutable() ? trimmed : QString();` — returns the path
as-is. `isBlockedInterpreterChain()` then walks from that path, so the first
hop's basename (`python3`) is checked before any symlink is followed. A relative
`python3` that resolves to `/usr/bin/python3` (itself a symlink to
`python3.14`) is caught at hop 0 by basename `python3`.

### Fail-closed behavior — CONFIRMED

- **Dangling symlink:** `QFileInfo::exists()` returns false → `return true`
  (blocked). ✓
- **Self-referential symlink (A → A):** `target == current` → `return true`
  (blocked). ✓
- **Looping chain (A → B → A):** Not caught by the `target == current` check
  (each hop's target differs from `current`), but the `kMaxHops = 40` cap
  terminates the loop and returns `true` (blocked). ✓
- **Too-long chain (>40 hops):** Loop exits after `kMaxHops` iterations →
  `return true` (blocked). ✓

### Residual risk — stated honestly

A custom wrapper binary not on the blocklist (e.g., `~/.local/bin/my-launcher`
that internally calls `exec("/bin/bash", ...)`) is not caught: its basename
isn't blocked, it's not a symlink to a blocked interpreter, and the blocklist
doesn't inspect `args`. This is explicitly acknowledged in the
`blockedInterpreters()` comment: "Blocking the wrapper outright closes that
whole class without having to parse its argv, which is not a fight this
blocklist can win in general."

This is appropriate for the threat model: defense in depth against a hand-edited
`~/.config/lane/config.json` (AGENTS.md invites agents to edit it), not a
privilege boundary. The blocklist raises the bar from "trivially bypassed with
a symlink or `env`" to "requires compiling a custom wrapper binary," which is
the right cost trade-off for this layer.

## Should-fix items (round 1) — re-verification

### 1. Controller::persist() swallows saveConfig() bool — FIXED

`Controller::persist()` (lines 582–600) now checks `saveConfig()`'s return value.
On failure, it fires `KNotification("save-failed")` with
`setComponentName("app.lane.Lane")`, a user-visible popup: "Could not save
settings / Lane could not write its config file. This change may be lost on
restart."

`data/app.lane.Lane.notifyrc` has the matching `[Event/save-failed]` section
with `Action=Popup`. ✓

### 2. migrateLegacyConfig tests — ADDED

Four tests in `test_config.cpp`:

- `migrateLegacyConfigFreshCopy` (line 193): creates old file, migrates, asserts
  new file exists with correct content and old file is byte-for-byte intact. ✓
- `migrateLegacyConfigIdempotentOnSecondRun` (line 220): migrates, overwrites
  new file, migrates again, asserts new file is unchanged. ✓
- `migrateLegacyConfigDestinationExistsWins` (line 249): creates both old and
  new files with different content, migrates, asserts new file is unchanged. ✓
- `migrateLegacyConfigFailureLeavesSourceIntact` (line 278): creates a file
  where the destination directory should be (forcing `mkpath` to fail),
  migrates, asserts new file doesn't exist and old file is intact. ✓

All four call the two-arg `migrateLegacyConfig(oldPath, newPath)` overload
directly — the real function, not a helper. The no-arg overload
(`config.cpp:197–200`) forwards to the two-arg overload. `main.cpp:60` calls
`migrateLegacyConfig()` as the first statement after `QApplication`
construction, before any `loadConfig()` (lines 34, 115, or Controller
constructor). ✓

### 3. Picker height uses real sectionCount — FIXED

`PickerModel::applyFilter()` (line 147): `m_sectionCount = sectionOrder.size()`
— counts the actual number of distinct sections in the filtered list, updated
on every filter change.

`PickerModel.h` (line 19): `Q_PROPERTY(int sectionCount READ sectionCount NOTIFY
countChanged)` — exposed to QML, updates on `countChanged`.

`Picker.qml` (line 187): `+ (count > 0 ? controller.pickerModel.sectionCount : 0)
* root.sectionHeaderHeight` — uses the real section count, not the hardcoded 2. ✓

## Test quality — discriminating?

### New bypass tests (would fail against old code)

| Test | File | Old code behavior | Discriminating? |
|------|------|-------------------|-----------------|
| `launchTargetRejectsSymlinkToBlockedInterpreter` | test_launcher.cpp:144 | Verbatim exec, basename "totally-a-browser" not blocked → launches → `!launchTarget` fails | ✓ |
| `launchTargetRejectsEnvReExecWrapper` | test_launcher.cpp:162 | "env" not in blocklist → launches → `!launchTarget` fails | ✓ |
| `customTargetsDropSymlinkToBlockedInterpreter` | test_config.cpp:80 | No chain walk in `targetFromJson` → entry loads → size 2, not 1 | ✓ |
| `customTargetsDropEnvReExecWrapper` | test_config.cpp:119 | "env" not blocked → entry loads → size 2, not 1 | ✓ |

All four go through the real `launchTarget()` / `loadConfig` → `targetFromJson`
path, not a helper. ✓

### Pipeline tests (round-1 flagged as "pass either way" — now fixed)

| Test | Round-1 issue | Round-2 fix | Discriminating? |
|------|---------------|-------------|-----------------|
| `unshortenStopsOnRedirectLoop` | Only checked `matchUrl`, which old single-hop code also produced | Now also asserts `QCOMPARE(calls, 3)` — old single-hop code makes 1 call | ✓ |
| `unshortenCapsHopCount` | Asserted `calls <= 5`, trivially satisfied by 1-call old code | Now asserts `QCOMPARE(calls, 4)` and `QCOMPARE(c.matchUrl, "https://bit.ly/hop-4")` — old code makes 1 call, lands on `hop-1` | ✓ |

### Tests that pass either way (acceptable)

`launchTargetRejectsBlockedInterpreterEvenWithoutParsing` (literal `/bin/sh`)
and `customTargetsDropBlockedInterpreter` (literal `python3`) pass on both old
and new code. They are basic guards, not bypass tests. The new symlink/env tests
are the ones that prove the bypasses are closed. These basic guards are fine to
keep alongside the discriminating tests.

## Other items checked

### Drag-reorder disabled while search filter active — DONE

All four `Kirigami.ListItemDragHandle` instances in `TargetsPage.qml` (browser
line 73, container line 138, pwa line 203, custom line 262) have
`enabled: page.searchText.trim().length === 0`. Dragging is disabled while a
filter is active; clearing the search box restores it. ✓

### CI: appstream + prefix install — DONE

`.github/workflows/ci.yml`:
- `appstream` added to the pacman install list (line 27). ✓
- `cmake --install build --prefix build/throwaway-install` (line 42) — this is a
  real `--prefix` install, not `DESTDIR`. The comment (lines 33–41) correctly
  explains why: `appstreamtest` reads `build/install_manifest.txt` and checks
  each path with `QFile::exists`-style logic against the exact recorded paths;
  a `DESTDIR` install records undecorated system paths that don't exist, causing
  the test to silently no-op. The prefix lives inside `build/` and is discarded. ✓

## Follow-up (informational, not blocking)

- **Custom wrapper binary not on the blocklist.** As discussed under Residual
  Risk above, a hand-compiled wrapper that internally execs a blocked
  interpreter is not caught. This is an accepted limitation of a basename-based
  blocklist and is honestly documented in the code. No action required for this
  PR; if the threat model ever expands to a privilege boundary, argv inspection
  or a seccomp/eBPF exec filter would be needed instead.
