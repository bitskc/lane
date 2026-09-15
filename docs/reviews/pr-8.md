# PR #8 Review — fix/pkgbuild-0.2.0-checksum

**Repo:** bitskc/lane · **Branch:** `fix/pkgbuild-0.2.0-checksum` · **Commit:** 185adc3 "Set PKGBUILD sha256 for v0.2.0"
**Diff:** `packaging/PKGBUILD` only — replaces `sha256sums=('SKIP')` (+ the `PENDING` comment block explaining why it was skipped) with the real hash for the v0.2.0 tarball.

## Pre-Landing Review: 0 issues found

**AUTO-FIXED:** none
**NEEDS INPUT:** none

## Checksum verification (load-bearing claim)

Independently downloaded the exact URL referenced by `source=`:

```
curl -sL https://github.com/bitskc/lane/archive/refs/tags/v0.2.0.tar.gz | sha256sum
b4bf96183b7bc2407266eca809d17de6024d5bcbdeaa194ec4fce1806190510c
```

Matches the committed value **exactly**, character for character. Author's claimed `makepkg --verifysource` pass and curl|sha256sum are corroborated by an independent fetch from this review session (fresh `curl`, not reusing the author's output).

## PKGBUILD correctness

- `pkgver=0.2.0` matches the `v0.2.0` git tag (verified: tag `v0.2.0` resolves to commit `dfebe06`, the "Release 0.2.0" merge, which is the parent of this PR's commit).
- `source=` URL (`.../archive/refs/tags/v$pkgver.tar.gz`) resolves to that same tag — confirmed by the checksum match above (a mismatched tag would produce a different tarball/hash).
- `pkgrel=1` is correct — this is the first packaging revision for the 0.2.0 upstream version (no prior 0.2.0 PKGBUILD existed; the `SKIP` placeholder was introduced in the same 0.2.0 release PR #7 and never released with a real checksum).
- `depends=`/`makedepends=` unchanged from the already-reviewed PR #7 baseline — out of scope for this diff and not touched.
- `build()`/`check()`/`package()` unchanged. `package()` runs `DESTDIR="$pkgdir" cmake --install build`, which is sufficient: `src/CMakeLists.txt` installs the `lane` binary (`install(TARGETS lane ...)`, line 78) and `CMakeLists.txt` (lines 81–86) installs the desktop file, metainfo XML, notifyrc, D-Bus service file, systemd user unit, and the scalable SVG icon. Nothing needed by the package is missing from the install graph.
- `LICENSE` install line unchanged and still correct.

## Anything else in the diff

None — the diff is a pure two-line swap (drop 4-line `PENDING` comment, fill in the real hash). No unrelated changes, no version bump, no dependency changes.

## Verdict

**safe_to_merge: true**

The one load-bearing claim (checksum correctness) is independently verified against a fresh download, byte-for-byte match. `pkgver`/`pkgrel`/source URL are internally consistent with the existing `v0.2.0` tag. `package()` already installs everything Lane ships. No scope creep in the diff. Clean, mechanical, correct fix — the exact kind of change this PR claims to be.
