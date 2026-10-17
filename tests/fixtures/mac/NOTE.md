# macOS test fixtures

These `.app` bundles are synthetic `Contents/Info.plist` trees used by
`test_discovery_mac` and `test_launch_mac`. They are not launchable apps:
their `Contents/MacOS/*` entries are one-line shell stubs.

**Do not let them reach LaunchServices.** `lsregister` scans
`~/src/lane/tests/fixtures/mac/` opportunistically and each fixture's
bundle id can shadow the real application it mimics (or steal
`open -b app.lane.Lane` from the `Lane.app` fixture). If a fixture
somehow gets registered, clear it with:

```bash
LS=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister
for p in tests/fixtures/mac/*.app tests/fixtures/mac-user/*.app; do
  "$LS" -u "$p" 2>/dev/null
done
```

This is why `tests/fixtures/mac/Lane.app` exists at all: a realistic
fixture for discovery parsing that is never meant to be the system's
registered `app.lane.Lane`.
