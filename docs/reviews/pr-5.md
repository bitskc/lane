# Pre-Landing Review: PR #5 — fix/default-browser-refresh

**Date:** 2026-09-14
**Commit:** `1d5ed9f` (single commit, `main..HEAD`)
**Scope:** `src/app/Controller.cpp` (`openSettings()`, +5 lines), `docs/reviews/retro-main.md` (new, 109 lines).
**Method:** `/review` skill checklist, full diff read plus surrounding source (`Controller.cpp`/`.h` for `isDefaultBrowser`/`refreshDefaultBrowserState`/`reload`/`makeDefaultBrowser`, `OverviewPage.qml` binding, `Q_PROPERTY` NOTIFY wiring, all call sites of `openSettings()`, `CHANGELOG.md`, `tests/`). CI (`build`, `Devin Review`) both pass on the PR.

**Verdict: Approve — safe to merge.** No blocking issues. One informational, non-blocking note below.

---

## What the diff does

Fixes the exact gap `docs/reviews/retro-main.md` (added in this same commit) documents: after `fc57e99` cached `isDefaultBrowser` and stopped spawning `xdg-settings` on every QML property read, the only refresh points were `reload()` (daemon start / `--rediscover`) and `makeDefaultBrowser()`. If the user changed the OS default browser via KDE System Settings instead of Lane's own button, the "Use Lane as default browser" button/description in `OverviewPage.qml:18-26` stayed stale until a daemon restart.

`Controller.cpp:452-458` now does, at the top of `openSettings()`:

```cpp
const bool wasDefaultBrowser = m_isDefaultBrowser;
refreshDefaultBrowserState();
if (wasDefaultBrowser != m_isDefaultBrowser) {
    Q_EMIT defaultBrowserChanged();
}
```

before `ensureSettingsEngine()` / `m_settingsWindow->show()`.

## Pass 1 — CRITICAL categories

Not applicable: no SQL, no concurrency/race pattern, no LLM trust boundary, no shell injection, no new enum value. `refreshDefaultBrowserState()` itself is untouched (`Controller.cpp:164-175`) — this diff only adds a caller.

## Pass 2 — INFORMATIONAL categories, and correctness scrutiny requested

**Emit correctness — no double-emit, no missed emit.** `defaultBrowserChanged()` is used *only* as the `NOTIFY` for `Q_PROPERTY(bool isDefaultBrowser ...)` (`Controller.h:33`, confirmed via grep — no other QML binding or C++ slot listens to it). A `NOTIFY` signal firing only when the backing value actually changes is the textbook-correct Qt pattern; `openSettings()`'s conditional emit is if anything tighter than the unconditional emits already in `reload()` (`:650-651`) and `makeDefaultBrowser()` (`:480-481`), which is pre-existing and untouched by this PR. No risk of a stale QML binding: `refreshDefaultBrowserState()` runs before `ensureSettingsEngine()`, so a first-time window creation binds against the already-fresh value even without the emit; a second window open re-evaluates the binding via the emit before `.show()`/`.raise()` if the value flipped.

**`openSettings()` is the sole path to the settings window** — confirmed via grep: tray context-menu "Settings" (`Controller.cpp:60`), tray icon activation (`:64`), CLI `--settings`/`--configure` (`:323-324`), and default no-args invocation (`:335-336`) all route through it. No bypass exists, so the fix covers every real way a user reaches the settings window.

**Cost is not a regression.** The added `QProcess`/`xdg-settings get` spawn (`refreshDefaultBrowserState()`, up to 1500ms `waitForFinished`, same call used by `reload()`/`makeDefaultBrowser()` already) runs synchronously on the UI thread on every `openSettings()` call. This looks like new UI-thread blocking risk on a hot path, but it isn't: before `fc57e99` introduced caching, the *first* QML evaluation of the `isDefaultBrowser` property binding — which happens exactly when the settings window is first shown — already spawned this same process synchronously. This change restores that original per-open cost rather than adding a new one, and matches the retro doc's own characterization of the fix ("same cost as before this change").

**CHANGELOG.md not updated (non-blocking).** `CHANGELOG.md:190-192`, still under `[Unreleased] / Fixed`, reads: *"The settings page spawned `xdg-settings` on every read... The result is now cached and refreshed only when it can change."* `retro-main.md` itself calls this entry "accurate but slightly overclaims" because externally-changed default browsers weren't covered. This PR is exactly the fix that closes that overclaim, but doesn't touch the CHANGELOG line — so the entry is still either slightly inaccurate (if read as "fully handled") or now simply outdated by not mentioning the settings-open refresh. Every other fix commit in this repo's history (`f34a709`, `47c1d7e`, `c578cac`, etc.) added a matching `CHANGELOG.md` bullet; this is the one fix commit in recent history that doesn't. Not a CI gate (no changelog-enforcement workflow), not a functional risk, but worth a one-line follow-up: append to the existing bullet or add a new one under `Fixed` noting the Settings-page external-change refresh.

**No new test coverage — consistent with existing convention, not a new gap.** `tests/` has no `test_controller.cpp`; nothing in the suite exercises `openSettings()`, `refreshDefaultBrowserState()`, `reload()`, or `makeDefaultBrowser()` today (all of `Controller.cpp`'s Qt-GUI-owning surface is untested, likely because it requires a `QQuickWindow`/tray harness). This diff doesn't reduce coverage of anything that had it. Not flagged as a blocking gap since it matches the file's existing test boundary, not something this PR carves out.

**`docs/reviews/retro-main.md` accuracy.** Spot-checked its commit range (`fe77477..f34a709`, 6 commits: `a696178`, `fc57e99`, `e2bd239`, `f81158b`, `b75da71`, `f34a709` — all verified present via `git log`) and several cited line ranges (`isDefaultBrowser`/`refreshDefaultBrowserState` at `:158-170`, pre-fix `openSettings()`, pre-fix `reload()` at `:620-650`) against `main`'s HEAD *before* this PR's own 5-line insertion — all match exactly. The doc is a dated snapshot (like the existing `main-reviews.jsonl` convention of citing a commit range, not living line numbers); its own citations will drift once this PR's 5 lines land above some of the functions it cites (e.g. `makeDefaultBrowser` moves from `:472-479` to `:477-484`). That's expected for a historical review artifact, not a defect — noting it here only so it isn't mistaken for a live pointer later.

## Suppressions applied

- Did not flag the up-to-1500ms synchronous spawn as new risk — verified it restores pre-caching behavior rather than adding to it (see above).
- Did not flag missing `test_controller.cpp` coverage as a blocking gap — matches the file's pre-existing, unrelated-to-this-diff test boundary.

## Summary

| Area | Verdict |
|---|---|
| `openSettings()` refresh + conditional emit | Correct; no double-emit, no missed emit, no new UI-thread cost |
| Coverage of settings-window entry points | Complete — no bypass path found |
| `docs/reviews/retro-main.md` accuracy | Commit range and cited line numbers verified accurate as of pre-PR `main` |
| CHANGELOG.md | Not updated to reflect this fix — informational, non-blocking |
| Test coverage | Unchanged; consistent with existing `Controller.cpp` gap |

**Safe to merge as-is.** Recommend a trivial non-blocking follow-up: extend the `CHANGELOG.md:190-192` bullet to mention the Settings-open refresh.
