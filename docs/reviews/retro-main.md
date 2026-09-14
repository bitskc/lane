# Retroactive Pre-Landing Review: fe77477..HEAD (main)

**Date:** 2026-09-14
**Scope:** Six commits that landed on `main` without a PR/review — `a696178`, `fc57e99`, `e2bd239`, `f81158b`, `b75da71`, `f34a709`.
**Method:** `/review` skill checklist (SQL safety / trust boundary / conditional side effects / structural issues), adapted for a C++/QML desktop app. Trust boundary here = URLs arriving over D-Bus from other apps, hand-edited `config.json`, and spawned processes (`xdg-settings`, browsers). Full diff read; suspicious spots cross-checked against surrounding source, not just the diff hunks. Build + `ctest` run against HEAD (12/12 pass).

Verdict: **Clean — no blocking issues.** Two informational findings worth a follow-up commit, not a revert.

---

## Pass 1 — CRITICAL categories

### Re-entrancy guard (fc57e99, `Controller.cpp:340-401`, `Controller.h`)

Traced the whole guarded region: `m_inOpenUrl = true` at line 364, `m_inOpenUrl = false` at line 389, nothing in between returns early — `runPipeline`, `route`, `destinationLadder`, `suggestedLadderIndex`, `applyDecision` all run unconditionally between those two lines, so the flag can't get stuck set. Confirmed by reading the whole function body (not just the hunk), not just trusting the comment.

Each queued call carries its own captured `activationToken` (`PendingUrl::activationToken`), so a second click's token can't leak onto the first click's routing decision or vice versa — this was the actual bug being fixed (a second D-Bus `openRequested` during the ~1.8s nested `QEventLoop::exec()` in `unshortenSync` used to overwrite `m_click` mid-pipeline).

Two low-severity notes, not blockers:
- The drain at the end (`openUrl(next.url, next.forcePicker)`) is a **recursive** call, not a loop — stack depth grows with queue length. In practice the queue can only grow as fast as D-Bus deliveries arrive during one ~1.8s window, so this is bounded in any real scenario (a user or app would need to fire many opens within that window). Not worth a rewrite, but worth knowing if `m_pendingUrls` ever needs a cap.
- `m_pendingUrls` has no size limit. A local app hammering `org.freedesktop.Application.Open`/the CLI during the unshorten window could grow it unboundedly. Same D-Bus trust boundary as everything else in this app (any local user session app can already spawn `lane <url>` in a loop), so this isn't a new attack surface — just flagging that there's no backpressure if that ever becomes a problem.

### LayerShellQt null check (fc57e99, `Controller.cpp:835-841`)

Guard is correctly placed before any dereference of `ls`, has a `qWarning()` (visible, not silent), and falls through to normal window behavior — matches the stated X11/non-wlroots segfault fix. No other call site in `Controller.cpp` dereferences `LayerShellQt::Window::get()` without going through `configureLayerShell()`.

### CLI flag registration (fc57e99, `main.cpp:95-106`)

`--rediscover` and `--configure` (alias on `settingsOpt`) are registered and match what `AGENTS.md`/`CHANGELOG.md` already documented as existing. Confirmed `rediscoverOpt` is both added to the parser and wired to a handler (`Controller::rediscover()` → `reload()`).

### matcher.cpp location-condition warning (fc57e99, `matcher.cpp:34-45`)

`static bool warned` is function-local static inside `ruleMatches()`, so it's process-wide across *all* rules with a location condition, not per-rule. Confirmed this can't spam per click (only ever logs once per daemon lifetime). Side effect worth naming: it also means a second, *different* rule with a location condition will never get its own warning once any rule has triggered the message once — under-warns rather than over-warns. Acceptable for a one-time diagnostic hint; not a functional bug.

### Enum/value completeness

`holdAutoOpen` default flip (`true` → `false`) touches three places and all three were updated together: `types.h:150` (struct default), `config.cpp:261` (load fallback), `docs/config.schema.json` (schema default). Traced `holdAutoOpen` through every consumer (`Controller.cpp:870` gate, `:908` animation duration) — both already handle `false`/`0` defensively (`m_config.holdMs <= 0` short-circuits, `qMax(1, …)` on duration), so the flip doesn't need new guard code, it just changes which branch is taken by default. `test_destination.cpp` was updated to assert the new default; nothing else in the repo asserts the old `true` default (checked via grep for `holdAutoOpen`).

No new enum/status values were introduced in this diff range — `UpdateOutcome` is new but has exactly 3 states and both consumers (`UpdateChecker::handleReply`, the new test file) handle all 3 exhaustively.

---

## Pass 2 — INFORMATIONAL categories

### `isDefaultBrowser` caching (fc57e99, `Controller.cpp:158-170`, `:472-479`, `:643-644`) — staleness gap

Previously `isDefaultBrowser()` spawned `xdg-settings get default-web-browser` synchronously on **every** QML read (expensive, but always fresh). Now it's cached in `m_isDefaultBrowser` and only re-checked in `refreshDefaultBrowserState()`, called from exactly two places: after `makeDefaultBrowser()` and inside `reload()` (daemon startup + `--rediscover`).

Gap: if the user changes the default browser through KDE System Settings (not Lane's own "Use Lane as default browser" button), `OverviewPage.qml:18-26`'s "Use Lane as default browser" button/description stays stale until the daemon restarts or `lane --rediscover` runs. `openSettings()` doesn't trigger a refresh, so opening Settings after an external change still shows the old state.

The CHANGELOG entry ("The result is now cached and refreshed only when it can change") is accurate for changes Lane itself makes, but slightly overclaims for changes made outside Lane. Not a regression that breaks anything — worst case the button stays clickable/unclickable one state behind reality — but a real, user-visible staleness gap that didn't exist before this commit.

**Suggested fix (small, non-blocking):** call `refreshDefaultBrowserState()` + `Q_EMIT defaultBrowserChanged()` in `openSettings()` before showing the window, or on the settings window's visibility-changed signal. Cheap (one more `xdg-settings` spawn per Settings open, same cost as before this change), and closes the only place a stale read is user-visible.

### Cosmetic-mutation target reuse (b75da71, `Controller.cpp:483-491`, `:586-619`, `:612-619`)

`hideTarget`/`renameTarget`/`moveTarget` now call `applyConfigToTargets(m_targets, m_config)` instead of `applyConfigToTargets(discoverTargets(...), m_config)`. Checked whether anything downstream depends on the discovery side effects that get skipped:
- Icon resolution, exec-path freshness, and PWA/profile scanning happen inside `discoverTargets()`, not `applyConfigToTargets()` — so a target's icon/exec won't go stale from these three calls, because those fields were already correct in `m_targets` and are untouched by `applyConfigToTargets`.
- `renameTarget` explicitly patches the live `m_targets` copy's `name` field before reapplying config (`Controller.cpp:594-603`), with a comment explaining why: `applyConfigToTargets` only *appends* custom targets not already present, it never updates fields on an existing entry. Verified this is the only field `renameTarget` touches that `applyConfigToTargets` wouldn't otherwise refresh.
- `applyConfigToTargets`'s hidden-flag fix (`discovery.cpp:832-841`, unconditional `t.hidden = hidden.contains(t.id)` instead of only-set-true) is required exactly because these three call sites now reapply repeatedly onto an already-applied list — confirmed via the new `applyConfigHiddenFlagIsIdempotentAcrossReapplication` test in `test_discovery.cpp`.

No downstream breakage found. A real browser install/uninstall or new PWA still requires an explicit `reload()`/`--rediscover` to show up, same as before this commit — these three mutation paths were never a discovery mechanism.

### `reload()` ordering (b75da71, `Controller.cpp:622-647`)

Checked execution order: `m_targets` is populated at line 626 (`discoverTargets` + `applyConfigToTargets`) **before** `clearDeadRemembered()` at line 643, so the dangling-check never runs against an empty target list (would otherwise prune everything as "dead" on first daemon start). `clearDeadRemembered()`'s own `persist()` call is safe against the just-loaded rules being clobbered, because `m_ruleModel->setRules(m_config.rules)` (line 633) runs before it — `persist()` reads `m_ruleModel->rules()` back into `m_config.rules`, so the model already holds the freshly-loaded rules by the time `clearDeadRemembered()`'s `persist()` fires. The ordering comment in the diff matches what the code actually does.

### `urlInScope` segment matching (fc57e99, `urlutil.cpp:125-142`)

Confirmed old behavior was a raw string prefix (`up.startsWith(sp)`), so a scope of `/bits` matched `/bitskc/lane` — a real false-positive scope match, not a hypothetical. New code requires an exact match or a `/`-terminated prefix. Checked `destination.cpp::destinationKeyMatches` (cited in the diff's own comment) uses the same segment-boundary approach already, so this isn't inventing a new convention, it's applying the existing one consistently. `test_url.cpp` covers both the false-positive-prevented case and the still-matches-subpath case.

### QML footer (`Picker.qml:309-430` region)

`list.currentItem ? list.currentItem.name : ""` is correctly null-guarded — confirmed the delegate declares `required property string name` (`Picker.qml:216-224`), so an unguarded `list.currentItem.name` would throw when the list is empty/fully filtered; the ternary avoids that. `Settings.qml`'s new `Accessible.role`/`Accessible.name`/`Accessible.selected` on the nav delegate are additive only, no existing bindings touched.

### Config unknown-key warning (b75da71, `config.cpp:220-249`)

`knownKeys` set was cross-checked against every field `loadConfig` actually reads (`version` through `substitutions`) — no field is missing from the allowlist, so no legitimate key will spuriously warn. `test_config.cpp`'s new test (`unknownTopLevelKeyWarnsButStaysTolerant`) uses a realistic typo (`pickerPolcy`) and asserts the config still loads with defaults rather than rejecting the file — matches the stated "tolerant on purpose" design.

---

## Documentation commits (a696178, e2bd239)

`a696178` (review docs) and `e2bd239` (openspec repair/archive) are pure documentation — no source changes. Spot-checked that `openspec/specs/lane/spec.md` (the new canonical spec) and the archived `initial-tern` change don't contradict the CHANGELOG or AGENTS.md claims read elsewhere in this review (rule `location` semantics, hold-bar default, default-browser caching) — consistent throughout.

## CHANGELOG accuracy (f34a709)

Every changelog entry added in this diff range was checked against the actual code change it describes (re-entrancy fix, LayerShellQt null check, CLI flags, cosmetic-mutation reuse, remembered-entry pruning, location-condition warning, scope segment matching, unknown-key warning, default-browser caching) — all accurate. The default-browser-caching entry is accurate but incomplete in a way that matches the staleness gap above (see that section).

---

## Summary

| Area | Verdict |
|---|---|
| Re-entrancy guard | Correct; two low-severity notes (recursion depth, unbounded queue) — not blocking |
| LayerShellQt null check | Correct |
| CLI flags | Correct |
| matcher.cpp warning scope | Correct, intentionally coarse |
| holdAutoOpen default flip | Correct, all three sites + tests updated together |
| isDefaultBrowser caching | **Real staleness gap** — no refresh path when default browser changes outside Lane |
| Cosmetic-mutation target reuse | Correct, verified no discovery-side-effect dependency |
| reload() ordering | Correct |
| urlInScope segment matching | Correct, fixes a real false-positive |
| QML footer null guard | Correct |
| Config unknown-key warning | Correct, allowlist complete |
| CHANGELOG | Accurate throughout |

**Needs a fix PR:** No, nothing here is a correctness bug. Recommend a small follow-up (not urgent) to refresh `isDefaultBrowser` state when Settings opens — one-line change, closes the only user-visible staleness gap found in this review.
