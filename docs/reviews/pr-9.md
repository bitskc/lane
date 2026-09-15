# PR #9 Review — fix/custom-target-rescan (de3f0a9)

**Verdict: APPROVE — safe to merge.**

## Scope Check
Intent: finish issue #3 — stop `addCustomTarget`/`removeCustomTarget` from
re-running full discovery (desktop file scan, Gecko profiles, Chromium
`Local State`, PWA manifests) just to add/remove one custom entry.
Delivered: exactly that, plus one new unit test. No scope creep — only
`Controller.cpp` and `tests/test_discovery.cpp` touched.

Pre-Landing Review: 0 issues (0 critical, 0 informational blocking)

## What changed
- `addCustomTarget` (Controller.cpp:568): `m_targets = applyConfigToTargets(m_targets, m_config)` instead of re-discovering. Correct because `applyConfigToTargets` (discovery.cpp:844-855) appends any `config.customTargets` entry not already present by `id`, and the freshly-created custom target's id is not yet in `m_targets`.
- `removeCustomTarget` (Controller.cpp:590-598): erases the target from `m_targets` by `id` first, then reapplies config. Necessary because `applyConfigToTargets` only appends missing custom targets — it never removes (discovery.cpp:844-855 has no removal branch) — so without the manual erase the removed target would survive in `m_targets` forever.

This mirrors the pre-existing pattern already used by `renameTarget`/`moveTarget` (Controller.cpp:628-644), which reapply config onto `m_targets` for the same reason and predate this PR. The PR is a straight extension of an established convention, not a new pattern.

## Findings

### 1. Id collision between custom and discovered targets — not possible
[P3] (confidence: 9/10) Controller.cpp:558 assigns `t.id = "custom:" + QUuid::createUuid()...`. Every discovered target id is prefixed `browser:` (discovery.cpp:483,565,625,661,679), `pwa:` (discovery.cpp:708), or `action:` (discovery.cpp:750,759). The `custom:` prefix is disjoint from all of them, so a custom target's id can never collide with a discovered one and `applyConfigToTargets`'s exists-by-id append check can't misfire. Verified by reading every id-assignment site in discovery.cpp — not fine, confirmed fine.

### 2. No dangling pointers/references into `m_targets` across the reassignment
[P3] (confidence: 8/10) `m_targets` is reassigned wholesale (`m_targets = applyConfigToTargets(...)` / `m_targets = remaining`), which can reallocate the underlying `QList<Target>` storage. Grepped every `Target *`/`const Target *` in the codebase (router.cpp, Controller.cpp): all are transient locals returned by `findTarget`/`defaultTarget` and consumed within the same function call before any further mutation of `m_targets` — e.g. `Controller.cpp:414` (`hideTarget`), `:506` (`displayNameFor`), `:653` (`reload`). `m_holdTarget` (Controller.h:207) is a `Target` **value** member (copied via `m_holdTarget = target;` at Controller.cpp:905), not a pointer, so it survives `m_targets` reallocation. No held pointer/reference is invalidated by either mutation site in this diff.

### 3. Stale rules/remembered hosts after `removeCustomTarget` — pre-existing, not a regression
[P4] (confidence: 7/10) `removeCustomTarget` does not call `clearDeadRemembered()`. If a rule or remembered-host mapping points at the removed custom target's id, `findTarget` returns `nullptr` for it and every consumer already null-checks (`matchingRules`: router.cpp:77-79 `if (t && !t->hidden)`; `route()`: router.cpp:179 `if (const Target *t = findTarget(...))`) — no crash, the rule/remembered entry is just silently unusable until the user runs "clear dead" on the Rules page or restarts (which calls `reload()` → `clearDeadRemembered()`, Controller.cpp:668). This behavior is identical before and after this PR (discovery was never re-run to invalidate the id either way, and `clearDeadRemembered()` was never wired into remove before this PR) — not introduced by this diff, out of scope for it.

### 4. New test proves `applyConfigToTargets`'s append/remove-by-id contract, not the `Controller` methods themselves
[P4] (confidence: 6/10) `applyConfigCustomTargetsAppendAndRemoveOnReapplication` (test_discovery.cpp:350) calls `applyConfigToTargets` directly and hand-rolls its own erase-by-id loop to simulate what `removeCustomTarget` does, rather than instantiating a `Controller` and calling `addCustomTarget`/`removeCustomTarget`. If someone reverted `removeCustomTarget`'s erase loop (Controller.cpp:590-596) back to a no-op, this test would still pass, because it never calls that code path. This is consistent with existing repo convention, though: there is no `test_controller.cpp` anywhere in `tests/` (confirmed via `CMakeLists.txt` — only 11 test binaries, none named after Controller), and the identical pattern already exists for `hideTarget`/`renameTarget`/`moveTarget` at test_discovery.cpp:321 (predates this PR). `Controller` is a `QObject` with file I/O (`persist()`) and autostart/XDG side effects, which is presumably why it isn't unit-tested directly in this codebase. Given that constraint, testing the shared helper the Controller methods delegate to is the established and reasonable proxy — flagging as informational only, not blocking.

## Enum/value completeness
No new enum/status/tier value introduced by this diff — N/A.

## Verification performed
- `git diff` read in full against `origin/main` merge-base.
- Read `applyConfigToTargets` (discovery.cpp:832-883) and all id-assignment sites in discovery.cpp.
- Grepped every `Target *`/`Target&` usage across `src/` to rule out dangling references across the `m_targets` reassignment.
- Rebuilt `test_discovery` and the `lane` app target from a clean touch of both changed files — both compile cleanly (ninja, Qt 6.11.1, GCC 16.1.1).
- Ran `./bin/test_discovery applyConfigCustomTargetsAppendAndRemoveOnReapplication` — PASS (3 passed, 0 failed).
- No PR review comments (Greptile or otherwise) exist on PR #9 (`gh api repos/bitskc/lane/pulls/9/comments` and `.../issues/9/comments` both empty).
- No `TODOS.md` in repo root — nothing to cross-reference.

## Safe to merge
**Yes.** Correct fix for the remainder of issue #3, matches existing Controller conventions, builds and passes tests. Findings #3 and #4 are pre-existing repo characteristics, not regressions — noted for awareness only, do not block.
