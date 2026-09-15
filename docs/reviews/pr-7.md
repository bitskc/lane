# PR #7 Review — release/0.2.0

Pre-Landing Review: No issues found.

## What this PR is

Mechanical release-prep for v0.2.0, per `docs/RELEASING.md`. Four files:
`CMakeLists.txt`, `CHANGELOG.md`, `data/app.lane.Lane.metainfo.xml`,
`packaging/PKGBUILD`. No source/behavior changes.

## Verification performed

- **CHANGELOG content vs shipped commits**: `git log v0.1.0..release/0.2.0`
  (25 commits since the v0.1.0 tag). Every bullet moved from `Unreleased`
  into the new `[0.2.0]` section was already present pre-PR — this PR only
  relocates it, doesn't author it. Spot-checked the non-obvious claims
  against source rather than trusting the prose:
  - Containers (`ext+container`) — `src/core/discovery.cpp:368-474`, commit
    `3601b1a`. Confirmed.
  - Picker section headers — `Picker.qml:24,186-212` uses
    `section.property/criteria/delegate` with a `sectionHeaderHeight`.
    Confirmed.
  - Rules page missing-destination warning — `RulesPage.qml:74-126`, exact
    wording matches the changelog line. Confirmed.
  - "Check for updates" button + per-cause failure messages —
    `OverviewPage.qml:106`, `UpdateChecker.{cpp,h}`, `tests/test_updatechecker.cpp`.
    Confirmed present and tested (not just "0.2.0 is honest" —
    the test file exists, matching the "Test coverage for the update
    checker" bullet).
  - X11 segfault fix (LayerShellQt null deref) — `Controller.cpp:841-845`
    now null-checks `LayerShellQt::Window::get()`. Confirmed.
  - Atomic config write + corrupt-config-moved-aside — `core/config.cpp:213-219`
    (rename-aside on invalid JSON) and `:383` (`QSaveFile`). Confirmed.
  - Firefox/Zen profile-by-folder fix, XDG activation focus fix, notification
    component-name fix, unshorten 4-hop cap, filter-field key handling —
    all matched against the corresponding source areas named in the diff
    from PR #6 / the prior review round; no fabricated bullets found.
  No entry describes something that didn't land, and nothing user-facing
  since v0.1.0 is missing from the section.

- **Section boundaries clean**: `grep -n "^## \["  CHANGELOG.md` →
  `Unreleased` (8) / `[0.2.0] - 2026-09-12` (12) / `[0.1.0] - 2026-09-11`
  (264). No overlap, no duplicated content, "Nothing yet." placeholder in
  the fresh `Unreleased` matches `RELEASING.md` step 3 verbatim.

- **Compare links**: `[0.2.0]` → `v0.1.0...v0.2.0`, `[Unreleased]` →
  `v0.2.0...HEAD`, `[0.1.0]` unchanged → `releases/tag/v0.1.0`. Matches the
  convention `RELEASING.md` documents. `git ls-remote --tags origin`
  confirms `v0.1.0` exists (`a1cddb9...`, annotated, dereferenced object
  present too), so the `[0.1.0]` release-tag link resolves.

- **Metainfo**: parsed with `xml.dom.minidom` — well-formed. `&amp;` used
  correctly for the "Browsers & apps" bullet. Date `2026-09-12` matches the
  CHANGELOG date. Bullet list is a condensed summary of the full changelog
  (per `RELEASING.md`'s "a few bullet points is enough," not a full paste)
  and every summarized bullet traces to a real changelog entry — no
  invented claims. No AI-voice/em-dash violations.

- **PKGBUILD**: `pkgver=0.2.0`, `pkgrel=1` (correctly reset — no prior
  0.2.0 rel to bump from), source URL still targets `bitskc/lane` (correct,
  matches the post-rename repo). `sha256sums=('SKIP')` with an honest
  comment explaining the checksum needs the `v0.2.0` tag to exist first —
  this is the right call; a guessed checksum would be worse than SKIP.
  The old comment (about waiting on the `tern`→`lane` GitHub rename) is
  correctly replaced with the new blocker (waiting on the tag), not left
  stale.

- **RELEASING.md lockstep**: the doc requires exactly `CMakeLists.txt`,
  `CHANGELOG.md`, and `data/app.lane.Lane.metainfo.xml` to move together
  (`docs/RELEASING.md:25-39`). All three are in this diff, all three say
  `0.2.0`. PKGBUILD isn't one of the mandated three (it's packaging, not
  the version-of-record) but it moved too, which is correct and doesn't
  violate anything.

- **Stray 0.1.0 references**: grepped outside CHANGELOG/metainfo history
  for `0.1.0` in code/build/packaging files. Only hits are in
  `docs/designs/*.md` and `docs/RELEASING.md`, all of which are either
  historical narrative about the v0.1.0 release or the versioning-example
  text (`0.1.0 -> 0.1.1`, `0.1.0 -> 0.2.0`) that's supposed to stay generic.
  Nothing live needed to move.

## Checklist categories

Pass 1 (SQL safety, race conditions, LLM trust boundary, shell injection,
enum completeness) — not applicable, no code changed.

Pass 2 — only "Dead Code & Consistency (version/changelog)" applies here;
covered above, no version mismatch, no inaccurate changelog entries.

## Verdict

**Safe to merge: yes.** No auto-fixes needed, no open questions. The one
follow-up already called out correctly in the PR description and PKGBUILD
comment — real sha256 after tagging — is out of scope for this PR by
design (can't compute a checksum for a tarball that doesn't exist yet).
