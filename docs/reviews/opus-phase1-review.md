# Opus adversarial review: Phase 1 (Clean Links + Alt+P Private)

Reviewed 2026-09-30 11:45 CDT. Branch `fix/env-opt-abbrev-and-pruning` @ e2e4c5e.
Scope: `openspec/changes/p1-clean-links-and-private-window/` (proposal, spec, design, tasks), checked against the design docs, the round-4 lens reviews (`ceo.md`, `eng.md`, `design.md`, `devex.md`), and the **actual code** in `src/`.
Method: read the code paths each task touches. Built a throwaway Qt 6.11.2 probe (scratchpad only, not in the repo) that runs the design's `cleanTrackingParams` snippet **verbatim** on 29 URLs, with input first passed through Lane's real `normalizeInput()`.
No code in `src/` or `tests/` was changed.

## Verdict

**REVISE BEFORE IMPLEMENTING.** The direction is right and the scope is correct. The plan has **4 blocking defects**:

1. The `browserName` sibling lookup opens the private window in the **wrong profile**. On Brave it can open **Tor**.
2. The "append flag" fallback is unreachable for discovered browsers. Built as written, it would leak the URL into a **non-private** window.
3. The `QUrlQuery` rebuild corrupts values containing `=` and `;` (for example base64 `==` becomes `%3D%3D`). It also re-encodes IRIs and IDN hosts, but only when a tracker is present.
4. Unsupported targets (PWA, custom, generic) **fail open**: they launch a normal window with a toast.

The round-4 lens reviews approved several claims that the code contradicts (see the last section). They were self-run in degraded mode and should not count as independent sign-off.

## 1. `openspec validate`

- `openspec validate p1-clean-links-and-private-window` → **valid** (also valid with `--strict`).
- Schema passes, but there are semantic gaps:
  - The proposal lists these under **"Modified Capabilities"**, but the spec uses only `## ADDED Requirements`. The base `Picker overlay` requirement (`openspec/specs/lane/spec.md:55`) lists the picker keys (`Enter, Esc, Alt+A`). Alt+P belongs there as a `## MODIFIED Requirements` delta, or the base spec goes stale on archive.
  - The spec has no scenario for:
    - an unsupported target
    - O365-wrapped links
    - non-http schemes
    - `=`/`;` in values
    - Alt+P on a container row
    - Alt+P not reaching notification history
  - The requirement text says Alt+P "SHALL immediately divert". For targets with no private mode, the design contradicts this (see C4).

## 2. Clean Links

### Probe results (design snippet, verbatim)

| Input (after `normalizeInput`) | Output | OK? |
|---|---|---|
| `…/article?utm_source=twitter&id=42&fbclid=xyz` | `…/article?id=42` | ✅ |
| `youtu.be/X?si=abc` | `youtu.be/X` | ✅ |
| `?redirect=https%3A%2F%2Fapp.com%2Fcallback&utm_source=email` | `?redirect=https%3A%2F%2Fapp.com%2Fcallback` | ✅ |
| `?q=a%2Bb&p=a+b&utm_source=e` | `?q=a%2Bb&p=a+b` | ✅ `+`/`%2B` kept apart |
| `?id=1&utm_source=x#section?utm_source=y` | `?id=1#section?utm_source=y` | ✅ fragment untouched |
| `?utm_source=x#frag` | `/p#frag` (no dangling `?`) | ✅ |
| `?a=1&a=2&utm_source=x` | `?a=1&a=2` | ✅ duplicates and order kept |
| `?next=%3Fa%3D1%26utm_source%3Dz&utm_source=y` | nested value untouched | ✅ |
| **`?token=YWJj==&utm_source=x`** | **`?token=YWJj%3D%3D`** | ❌ **value corrupted** |
| **`?a=1;b=2&utm_source=x`** | **`?a=1;b%3D2`** | ❌ **value corrupted** |
| **`/café?q=ü&utm_source=x`** | **`/caf%C3%A9?q=%C3%BC`** | ❌ **encoding changes only when a tracker was present** |
| **`https://bücher.de/?q=1&utm_source=x`** | **`https://xn--bcher-kva.de/?q=1`** | ❌ **host becomes punycode** |
| `?q=hello world&utm_source=e` (normalizeInput already decoded `%20`) | `?q=hello%20world` | ⚠️ same inconsistency |
| `?keep=1&&utm_source=x&` | `?keep=1&` | ⚠️ empty pairs collapsed |
| `mailto:a@b.com?subject=hi&utm_source=x` | tracker stripped | ⚠️ spec says http/https only; the design has no scheme check |
| `?UTM_SOURCE=x&utm_id=1&utm_source_platform=2&_ga&mkt_tok&yclid&dclid&ttclid&li_fat_id&_hsenc&oly_enc_id&vero_id` | **nothing stripped** | ⚠️ list is too narrow |

### Findings

- **C1 — Blocking: rebuilding with `QUrlQuery` is not "encoding fidelity".**
  - `QUrlQuery` re-serializes each surviving pair. A literal `=` inside a value becomes `%3D`. This hits base64 padding, JWT fragments and signed-URL tokens. Any server that HMACs the raw query string will reject the link.
  - `url.toString(QUrl::FullyEncoded)` also changes encoding outside the query (path IRI, IDN host). `normalizeInput()` (`src/core/urlutil.cpp:50`) returns **PrettyDecoded**, so the same link comes out in two different encodings depending on whether a tracker was present.
  - Downstream impact: `matchUrl` feeds regex/URL rules and `pathOf()`, which **includes the query** (`urlutil.cpp:68`) and is used for Path-scope rules. A rule on `bücher.de` or `/café` would silently stop matching whenever a tracker was stripped.
  - **Fix (simpler and exact):** never parse and rebuild. Split the raw query substring on `&`. Drop a segment only if its key, percent-decoded and compared case-insensitively, is banned. Re-join the surviving segments **byte-for-byte**. Splice them back between `?` and `#`. Drop the `?` if nothing survives. The rest of the string is never touched. It is about the same line count as the design snippet, has zero re-encoding risk, and needs no `QUrl::FullyEncoded` argument.
- **C2 — The allowlist is too narrow and too literal.**
  - Keys are matched case-sensitively, so `UTM_SOURCE` survives.
  - Only 5 `utm_*` keys are listed. `utm_id`, `utm_source_platform`, `utm_creative_format` and `utm_marketing_tactic` are standard GA4 keys and survive.
  - Recommendation: add a prefix rule for `utm_`, and add high-confidence unique keys: `_ga`, `_gl`, `mc_cid`, `mkt_tok`, `yclid`, `dclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`.
  - Keep generic words out (`ref`, `source`, `campaign`, `src`): they carry page state.
- **C3 — `si` is stripped globally. This is a correctness risk.**
  - `si` is a two-letter key. Some apps use it for session or search index, and stripping a session key breaks a login link.
  - It is only known to be a tracker on YouTube and Spotify.
  - **Fix:** scope it by host (`youtube.com`, `youtu.be`, `music.youtube.com`, `open.spotify.com`). One small `{key → host-suffix}` table covers this. Don't build a per-site rules engine.
- **C4 — The O365 interaction is undocumented, and the spec scenario overstates it.**
  - `runPipeline` sets `openUrl = originalUrl` for wrapped links unless `openUnwrapped` is set (`pipeline.cpp:56-60`). This is required by the CLAUDE.md invariant. Trackers inside a Safelinks-wrapped URL are therefore stripped from `matchUrl` **only**; the browser still receives them.
  - That is correct behavior, but the scenario "sets `matchUrl` and `openUrl`" is true only for unwrapped links. Add a scenario that pins this, so no one "fixes" it later by breaking the invariant.
- **C5 — Missing scheme guard.**
  - The spec says "HTTP and HTTPS", but the snippet never checks the scheme.
  - `isSafeOpenUrl` blocks other schemes at launch, but `--explain` output and `matchUrl` would still show a modified `mailto:`.
  - Fix: add `scheme ∈ {http, https}` to the guard.
- **C6 — One rationale in the proposal is wrong.** "Clutter path-scoped memory lookups" is not true: `destinationLadder()` (`destination.cpp:124`) builds keys from host and path segments only, never the query. The real, smaller effect is on **Path-scope rules**, because `pathOf()` includes the query. Fix the rationale so reviewers don't expect a memory fix.
- **C7 — Config plumbing tasks are missing.**
  - `config.cpp:231` has a `knownKeys` set that warns about unrecognized keys. Without `stripTrackingParams` in that set, every user who saves the setting gets a spurious warning.
  - `AGENTS.md` (config shape for agents) needs the new key.
  - `Controller.h` needs a `Q_PROPERTY(bool stripTrackingParams …)` following the `unshorten` pattern (`Controller.h:39`). Task 1.4 binds to it, but no task creates it.
  - `CHANGELOG.md` needs an entry.
- **C8 — Default `true` is a behavior change for existing users.** URLs that opened unmodified yesterday are rewritten after upgrade. That is acceptable (Velja does the same), but it goes in the CHANGELOG as a behavior change.
- **C9 — Discoverability.** `lane --explain URL` should print the stripped keys, so a user whose site broke can see why without reading JSON. This is about one line in the explain output, and it makes the toggle's escape hatch discoverable.
- **Performance claims** ("<10 µs, zero allocations"): unmeasured and irrelevant. `unshorten` already costs up to ~1.8 s. Delete the claim instead of promising it; nothing will enforce it.

## 3. Alt+P Private Window

### Findings

- **P1 — Blocking: `browserName` sibling pairing picks the wrong profile.**
  - Design/task 2.1 scan `m_targets` for `candidate.browserName == t.browserName && candidate.incognito`. Discovery produces **one private sibling per profile**:
    - Gecko: `id + ":private"` (`discovery.cpp:963`)
    - Chromium: `id + ":incognito"` (`discovery.cpp:1028`)
  - With profiles Work and Personal, Alt+P on Work can open **Personal (Private)**, which means the wrong cookie jar and the wrong account.
  - **Brave is worse.** `:tor` also has `incognito = true` and `browserName = "Brave"` (`discovery.cpp:1049-1058`). Alt+P can launch **Tor** instead of incognito.
  - **Fix:** look the sibling up by exact id: `t.id + ":private"`, else `t.id + ":incognito"`.
  - For container rows (`profile.id + ":container:N"`, `discovery.cpp:809`), strip the `:container:N` suffix first. Private windows cannot host containers, so a container row maps to its profile's private window.
  - Put this in `src/core` as a pure function, e.g. `const Target *privateCounterpart(const Target &, const QList<Target> &)` in `router.cpp`, so `tests/test_router.cpp` can test it. As written, task 2.1 puts the logic in `Controller` (`src/app`), which has no test harness. Its "Verify" step cannot be done.
- **P2 — Blocking: the "append engine flag" fallback is unreachable, and would leak the URL if reached.**
  - Every discovered Gecko or Chromium profile already has a sibling. The targets without one are `genericBrowser` and custom targets, and both are `Engine::Generic` (`discovery.cpp:1071`, `launcher.cpp:273`, `config.cpp:120`). So the Gecko/Chromium flag branch never runs. It is dead code (YAGNI): **delete it**.
  - If it ever did run: discovered args end in `… --new-tab $url`. *Appending* `--private-window` gives `--new-tab URL --private-window`. Firefox opens the URL in a **normal** tab plus an empty private window, and the link lands in normal history. That is a privacy failure that looks like success.
  - Appending flags to user-authored custom commands would also bypass the launcher's argv-validation assumptions (`launcher.cpp:23-160`).
- **P3 — Blocking: unsupported targets fail open.**
  - The design risk section says PWAs and custom actions "open normally with a passive toast". The user explicitly asked for private and got a normal window, with the link in history and cookies. For a privacy feature that is the worst outcome.
  - **Fix: fail closed.** When the highlighted row has no counterpart, do not launch and keep the picker open. Show an inline footer message ("No private mode for <name>") and announce it via `Accessible`. Add a spec scenario for this.
- **P4 — `pickId()` already persists remembered hosts. Do not reuse it.**
  - `Controller::pickId` writes `m_config.remembered` **before** launch whenever `m_alwaysForHost` is set (`Controller.cpp:434-437`).
  - `pickPrivate` must call `requestActivationAndLaunch` directly and not go through `pickId`/`pick`.
  - The spec scenario ("does NOT save … even if `alwaysForHost` was toggled") is correct. Add a test that asserts `config.remembered` is unchanged. This requires the `src/core` extraction from P1, or a Controller-level test.
- **P5 — The non-persistence invariant is incomplete: notifications leak the host.**
  - After launch, `Controller::launch` calls `toast(target, reason, click.host)` (`Controller.cpp:837`). The `launch-failed` notification also includes the host (`:829-833`).
  - KNotifications persist in Plasma's notification history. A private open would leave "Opened in Firefox · Work (Private) — bank.example" in history.
  - **Fix:** for private launches, suppress the toast or omit the host from it. Spec it as part of the ephemerality requirement, next to `remembered`.
- **P6 — Activation-token and grab-release order: correct if Alt+P reuses the existing path, so spec it that way.**
  - The existing order is: request the xdg-activation token from the still-visible picker window, **then** `hidePicker()` to release the layer-shell exclusive keyboard grab. `requestActivationAndLaunch` then waits up to 300 ms for the token before launching (`Controller.cpp:444-451`, `841-868`).
  - The design's wording, "Obtains XDG activation token … and dismisses picker", matches. Tighten task 2.1 to "call `requestActivationAndLaunch(sibling, "picker-private", window, m_click)` then `hidePicker()` — identical to `pickId`". Otherwise someone will hide the picker first and lose focus-raise on KWin.
- **P7 — Keyboard wiring is fine.** Alt-modified keys are not claimed by the focused `TextField`, which is why Alt+A is already a `Shortcut{}` (`Picker.qml:53`, comment at `:153-158`). Alt+P as a sibling `Shortcut{}` will fire.
  - Guard `list.currentIndex >= 0`, not only `list.count > 0`.
- **P8 — The footer spec cites UI that doesn't exist.**
  - The scenario says to place the hint "alongside `Alt+A Always`". The footer has no such label: Alt+A is the `alwaysBox` checkbox, and the footer labels are `esc`, `. widen`, `, narrow` (`Picker.qml:360-487`).
  - `design.md` review §1's "current footer" string is also invented.
  - Fix: add an `alt+p private` label in the same lowercase monospace style as `esc`, with `Accessible.name`.
- **P9 — Hidden siblings.** If the user hid the `:private` row in Targets, does Alt+P still use it? The "Hidden targets stay hidden" invariant is about listing, not an explicit private request. Recommendation: Alt+P **ignores `hidden`** on the counterpart, because the user asked for it by keystroke. Say so in the spec either way.
- **P10 — Out of scope, but name it.** Alt+P exists only in the picker. There is no private escape on the hold HUD or on silent rule/remembered launches. Add a one-line non-goal so nobody files it as a bug.

## 4. tasks.md: completeness and testability

| Task | Problem | Fix |
|---|---|---|
| 1.1 | Missing `knownKeys`, `AGENTS.md`, `CHANGELOG.md` | Add them to 1.1 |
| 1.2 | Prescribes the corrupting `QUrlQuery` rebuild; no scheme guard; no `si` host scoping | Replace with the raw-segment splice (C1, C3, C5) |
| 1.3 | Missing cases: `=` in value, `;`, IRI path, IDN host, uppercase key, `utm_` prefix, O365 wrapped (matchUrl cleaned, openUrl untouched), `mailto:` untouched, all-params-stripped with no dangling `?`, fragment containing `?utm_source` untouched, `si` kept on a non-YouTube host | Add these rows. The probe table above is a ready-made test list |
| 1.4 | Binds to `controller.stripTrackingParams`, which no task creates. "Verify" is manual | Add the `Q_PROPERTY` to 1.4. Verify persistence via the `test_config` round-trip, which is already in 1.1 |
| 2.1 | Logic lives in `Controller`, which has no tests, so "Verify" is impossible. Pairing is wrong (P1). Flag fallback is dead (P2). No fail-closed path (P3). Toast leak (P5) | Split into **2.1a** `privateCounterpart()` in `src/core/router.cpp` plus `test_router` cases (Gecko, Chromium, container→profile, Brave not→Tor, PWA/custom→null, hidden sibling). Then **2.1b** `Controller::pickPrivate`: no `pickId`, no remembered write, no host toast, reuse `requestActivationAndLaunch` |
| 2.2 | "Visual inspection in offscreen test" is not a check. Footer anchor doesn't exist (P8). No fail-closed message | Keep the verification manual and state it honestly (manual step on KWin). Add the inline "No private mode" state with `Accessible` |
| — | Missing: MODIFIED delta for the `Picker overlay` requirement; `--explain` shows stripped keys (C9) | Add as 2.3 / 1.5 |

Sizing still fits the DevEx lens's ≤2h-per-task bar after the split.

## 5. Recommended spec additions (drop-in scenarios)

- **Stripping must not re-encode:** `?token=YWJj==&utm_source=x` → `?token=YWJj==`
- **Uppercase and prefix:** `?UTM_Source=a&utm_id=b&q=1` → `?q=1`
- **`si` is host-scoped:** `https://example.com/?si=2&lang=en` is unchanged
- **O365 wrapped:** trackers are stripped from `matchUrl`; `openUrl` stays the original wrapper unless `openUnwrapped`
- **Private pairs by profile:** Alt+P on `Firefox · Work` launches `Firefox · Work (Private)`, never another profile's private window and never Brave Tor
- **Private fails closed:** Alt+P on a PWA or custom row launches nothing; the picker stays open with a "no private mode" message
- **Private leaves no trace:** Alt+P writes nothing to `remembered` and posts no notification containing the host

## 6. Credibility of the round-4 lens reviews

These claims were approved without checking the code:

- `eng.md` §4: "`TargetModel` maps `target.id` to its discovered private sibling." **No such map exists.**
- `eng.md` §4: flag fallback as a guard. It is unreachable (P2).
- `eng.md` §1: `queryItems(FullyEncoded)` + `setQueryItems` as the corruption guard. It **causes** C1.
- `design.md` §1: the "current footer" string does not match `Picker.qml`.
- `devex.md` §1: "every checkbox has concrete verification". 2.1 and 2.2 do not.
- Proposal: `zen-private` / `brave-incognito` ids. Real ids are `browser:<app>:<key>:private` / `:incognito`.

Also: `gstack-full-analysis.md` makes the AUR publish (Phase 0) a **hard gate before any Phase 1 code**. The Phase 1 proposal doesn't mention the gate. Restate it in `proposal.md` or record that it has been lifted.

## Status (2026-10-06)

Phase 1 shipped and was archived (`openspec/changes/archive/2026-09-30-p1-clean-links-and-private-window/`). Re-checked against `main` @ 96ba9e0:

- **Fixed in shipped code:** C1, C2, C3, C5, C6, C7, C9, P1, P2, P3, P4, P5, P6, P7, P8, P9. Includes the byte-exact splice, `utm_`/`hsa_` prefixes, host-scoped `si`, exact-id `privateCounterpart()` with `test_router` coverage, fail-closed notice, and host-free notifications for private launches.
- **Regression found and fixed on `fix/clean-links-open-url`:** `runPipeline` set `openUrl` to the *unstripped* URL, so the browser still got trackers. This contradicted the spec scenario "`openUrl` equals that same clean `matchUrl`". `openUrl` now uses the clean `matchUrl` for non-wrapped links; O365 behavior is unchanged. Added test `cleanLinksCleansOpenUrlForPlainLink`, and updated the CHANGELOG and Preferences copy.
- **Still open:** P10 (no Alt+P on hold HUD or silent launches) is an accepted non-goal. The AUR gate question is for the owner.
