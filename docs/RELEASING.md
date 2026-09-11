# Releasing Lane

This is the checklist for cutting a Lane release. It works the same
whether a human runs it by hand or an agent runs it.

The version number lives in one place: `project(lane VERSION x.y.z)`
in the top-level `CMakeLists.txt`. `ecm_setup_version` turns that into
`lane_version.h` at configure time, and the app reads
`LANE_VERSION_STRING` from there. `lane --version`, the About dialog,
and the Settings sidebar all pick the number up automatically. Do not
hardcode a version anywhere else, and do not put a version number in
QML.

## Versioning

Lane is pre-1.0. Normal semver rules apply, with the usual 0.x
caveat: a 0.x minor bump can still contain a breaking change.

- Patch release (`0.1.0` -> `0.1.1`): bug fixes only, no new user
  facing behavior.
- Minor release (`0.1.0` -> `0.2.0`): new features, while still 0.x.
- 1.0.0: Andy decides when Lane is done enough to call it 1.0. Nobody
  else bumps to 1.0.

## Three files move together

Every release touches exactly these three places, in lockstep:

1. `CMakeLists.txt` - bump `project(lane VERSION x.y.z ...)`.
2. `CHANGELOG.md` - turn the `## [Unreleased]` section into a dated
   section for the new version, then add a fresh empty `Unreleased`
   section above it.
3. `data/app.lane.Lane.metainfo.xml` - add a new `<release>` entry at
   the top of `<releases>` (newest first) with the version and date,
   and a short human-readable summary. Do not paste the whole
   changelog entry in here; a few bullet points is enough.

If any one of these is out of sync with the others, the release is not
ready.

## Steps

Release from the `main` branch after the changes you want are merged.

1. **Confirm the changelog is current.** Every user-facing PR since
   the last release should already have an `Unreleased` bullet (see
   `CONTRIBUTING.md`). Read through `git log` since the last tag and
   fill in anything missing.

2. **Bump the version.**

   ```bash
   # edit CMakeLists.txt: project(lane VERSION 0.2.0 LANGUAGES CXX)
   ```

3. **Update the changelog.** Move `## [Unreleased]` content into a new
   `## [0.2.0] - YYYY-MM-DD` section (today's date, in your local
   timezone is fine). Add a blank `## [Unreleased]` section back above
   it with `Nothing yet.` Update the comparison links at the bottom of
   the file.

4. **Update the AppStream metainfo.** Add the matching `<release>`
   entry to `data/app.lane.Lane.metainfo.xml`, newest first:

   ```xml
   <release version="0.2.0" date="YYYY-MM-DD">
     <description>
       <p>Short summary of the release.</p>
       <ul>
         <li>Notable change one</li>
         <li>Notable change two</li>
       </ul>
     </description>
   </release>
   ```

5. **Reconfigure, rebuild, and test.** The version string is baked in at
   configure time (`ecm_setup_version` writes `lane_version.h`), so you
   must re-run CMake after bumping `CMakeLists.txt`. If you do not have a
   `build/` directory yet, create one first.

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

6. **Validate the metainfo file** (optional but recommended if you
   have `appstreamcli` installed):

   ```bash
   appstreamcli validate --pedantic data/app.lane.Lane.metainfo.xml
   ```

   `appstreamcli` will flag `PolyForm-Noncommercial-1.0.0` as not
   being on its list of well-known SPDX license identifiers, or may
   otherwise note it as an uncommon license. That is expected and
   correct. Lane's license is PolyForm Noncommercial, not GPL or MIT,
   and the metainfo file must say so honestly even if some tooling
   does not recognize it. Do not change `project_license` to make a
   validator happy. As of this writing the file validates cleanly
   apart from a pre-existing pedantic note about the component ID
   (`app.lane.Lane`) containing uppercase letters, which is
   intentional: it matches the D-Bus name and desktop file ID.

7. **Commit the release.**

   ```bash
   git add CMakeLists.txt CHANGELOG.md data/app.lane.Lane.metainfo.xml
   git commit -m "Release v0.2.0"
   ```

8. **Tag it.** Use an annotated tag so it carries a message and date:

   ```bash
   git tag -a v0.2.0 -m "Lane 0.2.0"
   ```

9. **Push.**

   ```bash
   git push origin HEAD
   git push origin v0.2.0
   ```

10. **GitHub Release.** Pushing a `v*` tag triggers
    `.github/workflows/release.yml`. That workflow does not build
    anything; it just creates the GitHub Release for the tag and
    fills the release notes from the matching `## [x.y.z]` section of
    `CHANGELOG.md`. GitHub attaches the source `.zip` and `.tar.gz`
    to the release automatically. There is no separate binary,
    AppImage, AUR, or Flathub artifact to build or upload.

    Building and testing happen separately, in
    `.github/workflows/ci.yml`, on every push and pull request to
    `main`.

    If the release workflow is ever missing or broken, create the
    release by hand instead of skipping it:

    ```bash
    gh release create v0.2.0 --title "Lane 0.2.0" \
      --notes-file <(sed -n '/## \[0.2.0\]/,/## \[/p' CHANGELOG.md | sed '$d')
    ```

    (That `sed` pulls just the new version's section out of the
    changelog so the release notes are not the whole file.)

## What not to do

- Don't jump straight to 1.0.0 for the first release. This project
  starts at 0.1.0 on purpose.
- Don't duplicate the version number in QML, in a `#define`, or
  anywhere outside `CMakeLists.txt`.
- Don't tell people to `curl | bash` or `wget` a random install
  script. Point them at the build instructions in `README.md` or, once
  packaging exists, at the actual package.
- Don't lie about the license to satisfy a validator. PolyForm
  Noncommercial is the real license.
- Don't tag or publish a release from a state where `ctest` fails.
