# Releasing Tern

This is the checklist for cutting a Tern release. It works the same
whether a human runs it by hand or an agent runs it.

The version number lives in one place: `project(tern VERSION x.y.z)`
in the top-level `CMakeLists.txt`. `ecm_setup_version` turns that into
`tern_version.h` at configure time, and the app reads
`TERN_VERSION_STRING` from there. `tern --version`, the About dialog,
and the Settings sidebar all pick the number up automatically. Do not
hardcode a version anywhere else, and do not put a version number in
QML.

## Versioning

Tern is pre-1.0. Normal semver rules apply, with the usual 0.x
caveat: a 0.x minor bump can still contain a breaking change.

- Patch release (`0.1.0` -> `0.1.1`): bug fixes only, no new user
  facing behavior.
- Minor release (`0.1.0` -> `0.2.0`): new features, while still 0.x.
- 1.0.0: Andy decides when Tern is done enough to call it 1.0. Nobody
  else bumps to 1.0.

## Three files move together

Every release touches exactly these three places, in lockstep:

1. `CMakeLists.txt` - bump `project(tern VERSION x.y.z ...)`.
2. `CHANGELOG.md` - turn the `## [Unreleased]` section into a dated
   section for the new version, then add a fresh empty `Unreleased`
   section above it.
3. `data/app.tern.Tern.metainfo.xml` - add a new `<release>` entry at
   the top of `<releases>` (newest first) with the version and date,
   and a short human-readable summary. Do not paste the whole
   changelog entry in here; a few bullet points is enough.

If any one of these is out of sync with the others, the release is not
ready.

## Steps

1. **Confirm the changelog is current.** Every user-facing PR since
   the last release should already have an `Unreleased` bullet (see
   `CONTRIBUTING.md`). Read through `git log` since the last tag and
   fill in anything missing.

2. **Bump the version.**

   ```bash
   # edit CMakeLists.txt: project(tern VERSION 0.2.0 LANGUAGES CXX)
   ```

3. **Update the changelog.** Move `## [Unreleased]` content into a new
   `## [0.2.0] - YYYY-MM-DD` section (today's date, in your local
   timezone is fine). Add a blank `## [Unreleased]` section back above
   it with `Nothing yet.` Update the comparison links at the bottom of
   the file.

4. **Update the AppStream metainfo.** Add the matching `<release>`
   entry to `data/app.tern.Tern.metainfo.xml`, newest first:

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

5. **Rebuild and test.**

   ```bash
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

6. **Validate the metainfo file** (optional but recommended if you
   have `appstreamcli` installed):

   ```bash
   appstreamcli validate --pedantic data/app.tern.Tern.metainfo.xml
   ```

   `appstreamcli` will flag `PolyForm-Noncommercial-1.0.0` as not
   being on its list of well-known SPDX license identifiers, or may
   otherwise note it as an uncommon license. That is expected and
   correct. Tern's license is PolyForm Noncommercial, not GPL or MIT,
   and the metainfo file must say so honestly even if some tooling
   does not recognize it. Do not change `project_license` to make a
   validator happy. As of this writing the file validates cleanly
   apart from a pre-existing pedantic note about the component ID
   (`app.tern.Tern`) containing uppercase letters, which is
   intentional: it matches the D-Bus name and desktop file ID.

7. **Commit the release.**

   ```bash
   git add CMakeLists.txt CHANGELOG.md data/app.tern.Tern.metainfo.xml
   git commit -m "Release v0.2.0"
   ```

8. **Tag it.** Use an annotated tag so it carries a message and date:

   ```bash
   git tag -a v0.2.0 -m "Tern 0.2.0"
   ```

9. **Push.**

   ```bash
   git push origin HEAD
   git push origin v0.2.0
   ```

10. **GitHub Release.** Pushing a `v*` tag triggers the release
    workflow in `.github/workflows`, which builds and publishes the
    GitHub Release from the tag. If that workflow is ever missing or
    broken, create the release by hand instead of skipping it:

    ```bash
    gh release create v0.2.0 --title "Tern 0.2.0" \
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
