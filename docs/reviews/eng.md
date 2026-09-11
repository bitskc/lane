# Tern engineering review

Scope: `src/core`, `src/app`, `tests`, `data`, build/CI, and the docs that make
claims about behavior. Read on a live checkout with a real daemon running
(PID confirmed via `/proc`), a real `~/.config/tern/config.json`, and a real
browser/profile/container/PWA set (45 targets via `tern --list`).

## Verdict

**SHIP WITH FIXES.** The routing core (`router.cpp`, `destination.cpp`,
`matcher.cpp`) is small, well-factored, and the tests that exist for it are
real behavioral tests, not scaffolding. The argv-only launcher and the
http/https-only URL gate are correctly built and correctly tested. But three
things will bite the first real user or packager: custom targets loaded from
`config.json` skip the interpreter blocklist that the Settings UI enforces,
`saveConfig()` is not atomic so a bad-timed crash wipes rules/remembered/custom
targets with zero warning, and the README's own build command is missing two
Arch packages the code actually requires. None of these need a redesign. Fix
the three, ship 0.1.1.

## Blocking

**1. Hand-edited custom targets bypass the interpreter blocklist entirely.**
`src/core/config.cpp:105-124` (`targetFromJson`) builds a `Target` straight
from JSON with no validation of `exec` at all, and `src/core/launcher.cpp:134-151`
(`launchTarget`) resolves and executes `target.exec` via
`QProcess::startDetached` without ever calling `isBlockedInterpreter`
(`launcher.cpp:70-73`). That check exists and is enforced — but only inside
`parseCustomCommand` (`launcher.cpp:91-132`), which is the function the
Settings-window "Add custom handler" dialog calls
(`Controller.cpp:376-394`, `addCustomTarget`). `AGENTS.md:19` explicitly
documents and encourages hand/agent-editing `config.json`, and `AGENTS.md:109-112`
warns in prose ("do not point rules at custom handlers that run wild
interpreter commands") — but nothing in code enforces that warning for the
hand-edit path. Symptom: add a `customTargets` entry with
`"exec": "python3", "args": ["-c", "..."]`, point `defaultTargetId` or a rule
at its id, run `tern --rediscover` (which reloads config live, see Should-fix
#3) or restart the daemon, and the next matching click runs it — no picker,
no confirmation, no log line. Fix: call `isBlockedInterpreter` (and
`resolveExecutable`) in `targetFromJson` at load time, or in `launchTarget`
itself so both the GUI and hand-edit paths get the same guarantee; reject and
`qWarning()` rather than silently loading a dangerous target.

**2. Config writes are not atomic; a bad-timed crash wipes all rules and
memory with no warning.** `src/core/config.cpp:271-276` opens the real config
path with `QIODevice::WriteOnly | QIODevice::Truncate` and writes directly —
no temp file, no rename, no fsync. Every settings toggle calls `persist()`
synchronously (`Controller.cpp`, e.g. `setPickerPolicy` at line 99-104, and a
dozen more like it), so a crash, `kill -9`, session logout, or full disk
mid-write leaves a truncated or invalid `config.json`. `loadConfig()`
(`config.cpp:131-141`) checks `doc.isObject()` and, on any parse failure,
silently returns a bare-default `Config` — no backup, no warning, no log.
Symptom: one mistimed crash and the user's rules, remembered destinations,
target order, aliases, and custom handlers are all gone on next launch, with
no indication anything went wrong. Fix: write to a temp file in the same
directory and `QFile::rename()` over the target (or use `QSaveFile`); on
parse failure, rename the bad file aside and log a warning instead of
discarding it.

**3. README's own build command and the shipped PKGBUILD both omit two
required packages.** `CMakeLists.txt:39-49` does
`find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS ... ColorScheme ...
Crash)`. Those components are provided by the Arch packages `kcolorscheme`
and `kcrash` (confirmed: `pacman -Ql kcolorscheme` and `pacman -Ql kcrash`
ship `KF6ColorSchemeConfig.cmake` / `KF6CrashConfig.cmake` respectively).
`.github/workflows/ci.yml:19-24` installs both explicitly — added in commit
`7930946 Install kcrash and kcolorscheme in CI` — but `README.md:158-160`
("Build (CachyOS / Arch)") and `packaging/PKGBUILD:8-14` (`depends=`) were
never updated to match. Symptom: `cmake -S . -B build` fails at configure
time with `Could NOT find KF6ColorScheme` (or `KF6Crash`) on any machine that
doesn't already have unrelated KDE apps pulling those frameworks in — i.e.
exactly the "clean clone on a fresh machine" scenario the docs are written
for. `makepkg` on the shipped PKGBUILD fails the same way, since `depends=`
is what makepkg installs before building. Fix: add `kcolorscheme kcrash` to
both the README pacman line and PKGBUILD's `depends=`.

## Should fix

**Conflicting rules under `pickerPolicy: never` are silently dropped, not
first-match.** `router.cpp:136-157`. `DESIGN.md:34` documents decision order
as "1. Explicit rules (first match)" with no caveat. The code only takes
"first match" when every matching rule agrees on the same target
(`conflict == false`, line 145-151); if two enabled rules genuinely conflict
and `pickerPolicy` is `never`, the code doesn't return the picker (line
152-156 only fires when policy `!= Never`) and doesn't take the first rule
either — it falls through the bottom of the `if` block (past line 157) into
remembered → PWA → default, ignoring both rules. A user who wrote two
conflicting rules and set "never show picker" gets neither rule honored, with
no error. Either make "first match" unconditional (matching the docs) or
document the `never`-policy exception.

**Unshorten runs synchronously on the GUI thread via a nested event loop,
and `openUrl` has no re-entrancy guard.** `Controller.cpp:600-606`
(`unshortenFn`) wires `unshortenSync(url, 1800)` directly into
`runPipeline()`, called from `Controller::openUrl` (`Controller.cpp:229-243`).
`unshorten.cpp:31-37` blocks on a `QEventLoop` for up to 1800ms on every URL
whose host is one of the ~23 known shorteners. The nested loop keeps the UI
responsive, but it also keeps Qt's event dispatcher running, so a second
D-Bus `Open`/`Activate` arriving mid-unshorten re-enters `openUrl` while
`m_click`, `m_holdAnimation`, and friends are mid-update from the first call.
Two links clicked in quick succession, one of them a `t.co`/`bit.ly` link,
is a plausible everyday trigger. Add a simple in-flight guard, or move
unshorten off the synchronous-nested-loop pattern.

**Docs say "no live reload, restart the daemon"; `--rediscover` already does
a live reload.** `README.md:128`, `AGENTS.md:20-21,195-201` all instruct
restarting the daemon after hand-editing `config.json`. But
`Controller::rediscover()` (`Controller.cpp:323-326`) just calls `reload()`
(`Controller.cpp:440-454`), which calls `loadConfig(m_configPath)` fresh from
disk — exposed as the tray menu's "Rediscover browsers" action and as
`tern --rediscover` / D-Bus `Activate(["--rediscover"])`. Either the docs are
stricter than reality (harmless but confusing — tell users they have a lighter
option than `pkill`), or the coupling of "rescan browsers" and "reread config"
wasn't intentional and deserves separating.

**`LayerShellQt::Window::get()` result is dereferenced with no null check and
no platform branch.** `Controller.cpp:579-598` (`configureLayerShell`), called
from `ensurePickerEngine` (line 555) and `ensureHoldEngine` (line 700), calls
`LayerShellQt::Window::get(window)` and immediately calls `ls->setLayer(...)`
on the result. `CMakeLists.txt:39` makes `LayerShellQt` a hard `REQUIRED`
build dependency, so the whole app is Wayland-layer-shell-only by
construction, but nothing at runtime checks
`QGuiApplication::platformName()` or branches on the compositor lacking
`wlr-layer-shell`. If the picker window ever fails to get placed as an
overlay (X11 session, XWayland fallback, a non-wlroots Wayland compositor)
this is a hard failure for a default-browser handler — no picker, no error,
possibly a null-pointer crash. I run Wayland here and could not exercise this
path without killing the live daemon (out of scope), so this is a source
read, not a reproduction. Add an explicit `platformName() != "wayland"` check
at startup that refuses to run with a clear stderr message, rather than
depending on undocumented graceful-degradation behavior from LayerShellQt.

**Stale rule/remembered targets are invisible.** `router.cpp:59-72`
(`matchingRules`) and `router.cpp:159-168` silently drop rules and remembered
mappings whose `targetId` no longer resolves (browser uninstalled, profile
deleted) — which is the *right* runtime behavior, no crash, sensible
fallthrough — but there's no signal back to the user that a rule they wrote
is now dead. `RuleModel` (`RuleModel.cpp`) stores `targetId` as an opaque
string with no existence check, and `Controller::displayNameFor`
(`Controller.cpp:356-362`) falls back to printing the raw internal id (e.g.
`browser:firefox:x9sh86ht.default`) when it can't resolve a name — which is
the only hint a Rules-page user gets that something is broken. `config.remembered`
also has no garbage collection, so it grows forever with dead entries.

**Unshorten only resolves one redirect hop.** `pipeline.cpp:16` calls
`unshorten(working)` exactly once, and `unshorten.cpp:16-21` uses
`ManualRedirectPolicy` / `MaximumRedirectsAllowed(0)`, returning after a
single `Location` header. A chained shortener (`bit.ly` → `tinyurl.com` →
real site) leaves `matchUrl` pointed at the second-hop shortener host, so
rules and the picker still see a shortener domain instead of the real
destination.

## Considered and fine

**Discovery cost is not on the click path.** `Controller::openUrl`
(`Controller.cpp:229-243`) never calls `discoverTargets()`. Discovery only
runs from the constructor (`reload()`, line 440-454) and from explicit
settings mutations (`hideTarget`, `addCustomTarget`, `removeCustomTarget`,
`renameTarget`, `moveTarget`, `rediscover`) — all user-initiated, none on the
hot path. Measured cold-start cost on this machine (`tern --list`,
`tern --explain`, which run discovery standalone outside the daemon) was
~1.4s wall for 45 real targets/containers/PWAs, but that includes cold Qt
init, not pure discovery, and it's paid once per daemon lifetime in the
normal case.

**`containerHandlerStatus`'s `Unknown` fallback is a reasonable heuristic,
not a guess.** `discovery.cpp:366-393` and `417-429`. When `extensions.json`
can't be read at all, falling back to "did this profile ever get a
custom-named container" as a weaker signal for "does this browser have a
container-handler extension" is defensible: the alternative is either always
showing containers (noisy default for users without the extension) or never
showing them when the signal is merely unavailable (breaks users who do have
it). The in-code comment states the tradeoff; I agree with the call.

**The unshorten HTTP client is not a general SSRF proxy.** `urlutil.cpp`'s
`kShorteners` set is a fixed, ~23-entry hardcoded allowlist; `pipeline.cpp:19`
only invokes the network call when `isShortener(working)` is true. Tern's own
`QNetworkAccessManager` (`unshorten.cpp:23`) never connects to an
attacker-chosen host — only to one of those 23 literal domains. The
`Location`-header safety check (`isSafeOpenUrl` + `isPrivateOrLocalHost`,
`unshorten.cpp:47-54`) is string/literal-based rather than resolved-IP based,
so a compromised shortener could in theory redirect to a hostname that
*resolves* to a private IP without the literal string matching
`isPrivateOrLocalHost`'s suffix/IP-literal checks — but Tern never itself
connects to that redirect target, it only proposes it as the URL to hand to
the user's browser, exactly as if the user had pasted the link. This matches
`DESIGN.md:25`'s stated scope ("known hosts only, HEAD, no cookies, no
private/link-local/metadata destinations") for the one host it does contact.

**The D-Bus surface is exactly what a URL-opener is supposed to expose, plus
one framework freebie.** Confirmed live via `busctl --user introspect
app.tern.Tern /app/tern/Tern`: no custom `Q_SCRIPTABLE` adaptor, just
`KDBusService`'s standard `org.freedesktop.Application` (`Activate`, `Open`,
`ActivateAction`) and `org.kde.KDBusService.CommandLine`. `Open()` runs the
exact same `isSafeOpenUrl`-gated pipeline a real click would — any local
process calling it has no more power than running `tern <url>` on the CLI,
which any local process can already do. `/MainApplication` does expose
`org.qtproject.Qt.QCoreApplication.quit` (standard Qt/KDBusService behavior,
not something Tern added), so any local process can kill the daemon — but
the desktop entry is `DBusActivatable=true` with a bus-activation service
file (`data/dbus/app.tern.Tern.service.in`), so the daemon self-respawns on
the next real `Open` call. Worst case is a slower first click, not a
permanently dead default-browser handler.

**Custom-handler argv, never a shell, holds up.** `launcher.cpp:134-151`
(`launchTarget`) calls `QProcess::startDetached(exe, args)` with a real
argv array, never a shell string; `expandArgs` (`launcher.cpp:45-68`)
substitutes `$url`/`$urlEncoded` into individual argv tokens. Verified by
`test_launcher.cpp`'s `urlEncodedPlaceholderNeverInjectsNewline` and
`rejectsShellCustomCommand` — a URL with a newline or shell metacharacters
can't break out of its own argv slot. This defense is real; it's the
load-time/exec-time validation gap (Blocking #1) that undermines it, not the
mechanism itself.

**`version` field with no migration code is fine pre-1.0.** `config.cpp`
round-trips `version` but nothing branches on it. `CHANGELOG.md:5-6` states
"while the major version is 0, minor releases may still contain breaking
changes," and every field load in `loadConfig` already falls back to a
sensible default (`.toBool(true)`, `.toInt(1)`, etc.), so an old or partial
file degrades gracefully today. Worth building real migration before 1.0,
not before 0.2.

## Test gaps

- `launchTarget()` itself never rejects a blocked-interpreter `exec` built
  directly on a `Target` (bypassing `parseCustomCommand`) — the one test
  that would have caught Blocking #1.
- `loadConfig()` on truncated/corrupt JSON — no test proves or guards the
  silent-default-reset behavior in Blocking #2.
- No test that `saveConfig()` survives an interrupted write (or that it's
  even meant to — there's nothing to assert against today).
- No discovery test for a target that disappears between two
  `discoverTargets()` calls (browser uninstalled mid-session) and how a
  live rule/remembered mapping pointing at the vanished id behaves.
- No test for `route()`/`rankForPicker()` with a rule or remembered id
  pointing at a hidden or nonexistent target.
- No test for the `pickerPolicy: never` + conflicting-rules fallthrough
  (Should-fix #1).
- `unshorten.cpp` (65 lines, real HTTP/redirect logic) has zero direct test
  coverage — only the pipeline's injected-function seam is exercised
  (`test_pipeline.cpp::unshortenHook`), never the real network code path.

## Numbers

- LOC: `src/core` 2555 (cpp+h), `src/app` 1622 (cpp+h), `src/qml` 1297,
  `tests` 1219. Total under `src/` (core+app+qml): 5474.
- Tests: 9 ctest targets (`appstreamtest` + 8 C++ unit suites: `test_url`,
  `test_matcher`, `test_pipeline`, `test_discovery`, `test_router`,
  `test_config`, `test_launcher`, `test_destination`). All pass
  (`ctest --test-dir build`, 0.41s total, offscreen QPA).
- Build: clean `cmake --build` (Release, Ninja, `-j12` on a 6-core/12-thread
  AMD Ryzen 5 5500U): 34.1s wall.
- Binary: `/home/andy/.local/bin/tern`, 13,102,064 bytes (~13 MB).
- Daemon RSS (live, PID confirmed via `/proc/<pid>/status`): 139,300 kB
  (~136 MB), VmSize 1,424,044 kB, 19 threads, 24 open FDs.
- Real-world discovery: 45 targets on this machine (browser profiles +
  containers + PWAs) via `tern --list`. Cold CLI invocation
  (`tern --list`/`--explain`, full Qt init plus discovery, standalone
  process outside the daemon): ~1.4s wall.
