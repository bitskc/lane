# Lane engineering review, round 4 (Browser Picker Supremacy)

Scope: Technical architecture and feasibility of `docs/designs/linux-browser-picker-supremacy.md` and `openspec/changes/browser-picker-supremacy/`.
Mode: degraded — self-run 5-lens review by assistant, all independent channels quota-dead until next reset window.

## Verdict

**APPROVE WITH ARCHITECTURAL GUARDS.**

The proposed technical architecture is solid, fits cleanly into Lane's existing pipeline/matcher/controller separation, and introduces zero avoidable latency on the critical link-dispatch path. Four critical engineering constraints must be enforced during implementation:

## Critical Technical Findings & Guards

### 1. Clean Links: Query parameter encoding corruption hazard
- **Risk**: Using `QUrlQuery::queryItems()` without encoding flags will URL-decode parameter values (e.g. turning `%2B` into `+` or `%20` into space). If a downstream destination expects encoded values (such as an OAuth `redirect_uri` or a nested URL parameter), stripping tracking tokens could silently corrupt the remaining parameters.
- **Guard**: Implementation MUST use `QUrlQuery::queryItems(QUrl::FullyEncoded)` and rebuild the query via `QUrlQuery::setQueryItems(filteredItems)`.
- **Performance**: The allowlist of tracking keys should be stored as `static const QSet<QStringView>`:
  ```cpp
  static const QSet<QStringView> bannedKeys = {
      u"utm_source", u"utm_medium", u"utm_campaign", u"utm_term", u"utm_content",
      u"fbclid", u"gclid", u"gbraid", u"wbraid", u"msclkid", u"twclid", u"mc_eid",
      u"si", u"igshid"
  };
  ```
  This achieves sub-microsecond filtering with zero heap allocations per query key.

### 2. Default Browser Watchdog: Inotify inode destruction on atomic rename
- **Risk**: Tools modifying `~/.config/mimeapps.list` (`xdg-settings`, KDE File Associations, text editors) almost never write in-place; they write to a temporary file (e.g. `mimeapps.list.tmp.XXXXXX`) and invoke `rename()` over the target. On Linux inotify, an atomic `rename()` onto a watched file destroys the watched inode and triggers an `IN_DELETE_SELF` / `IN_MOVE_SELF` event. A standard `QFileSystemWatcher` will stop watching the file after the first modification.
- **Guard**: In the `fileChanged` handler, verify if `watcher.files().contains(mimeappsPath)`. If the path was dropped, re-add `mimeappsPath` to `QFileSystemWatcher`. Additionally, debounce the check using a 2-second `QTimer` to allow settling.

### 3. KActivities: Mandatory conditional compilation & non-KDE degradation
- **Risk**: Linking `KF6::Activities` unconditionally would break headless CI builds on minimal Ubuntu containers and prevent Lane from being packaged or run on non-KDE environments (Sway, Hyprland, GNOME).
- **Guard**: CMakeLists.txt must use:
  ```cmake
  find_package(KF6Activities ${KF_MIN_VERSION} OPTIONAL_COMPONENTS)
  if(TARGET KF6::Activities)
      target_compile_definitions(lane PRIVATE HAVE_KACTIVITIES)
      target_link_libraries(lane PRIVATE KF6::Activities)
  endif()
  ```
  In C++, `#ifdef HAVE_KACTIVITIES` instantiates `KActivities::Consumer`. Without it, `currentActivityId()` returns `QString()`, and activity-scoped rules cleanly fall through.

### 4. Alt+P: Target pairing vs custom engine flag resolution
- **Risk**: Passing `--incognito` to Firefox or `--private-window` to Chrome fails launch. Passing either to an unknown custom shell script or flatpak wrapper without validation could violate Lane's launch security invariants.
- **Guard**:
  1. Prefer discovered target pairing: `TargetModel` maps `target.id` to its discovered private sibling (e.g. `zen` -> `zen-private`). Discovered targets already have verified arguments.
  2. For targets without a paired private sibling: check `target.engine`. If `Engine::Gecko`, append `--private-window`. If `Engine::Chromium`, append `--incognito`. For `Engine::Unknown`, do not guess flags; display a passive warning or launch standard profile.

### 5. Headless CTest Isolation
- **Guard**: None of the new features may depend on live desktop infrastructure during `ctest`.
  - Clean Links unit tests in `tests/test_pipeline.cpp` run purely in-memory.
  - Activity matching unit tests in `tests/test_matcher.cpp` must test `matchRule(rule, url, context)` with an injected `activityId` parameter.
  - Watchdog unit tests must watch a temporary file in `QTemporaryDir`.
