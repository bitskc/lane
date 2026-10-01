# Lane DevEx & QA Review, round 4 (Browser Picker Supremacy)

Scope: Task decomposition, testability, CI/CD safety, and maintainability across `openspec/changes/browser-picker-supremacy/tasks.md`.
Mode: degraded — self-run 5-lens review by assistant, all independent channels quota-dead until next reset window.

## Verdict

**APPROVE.**

The implementation tasks in `tasks.md` are well-structured, follow atomic `<=2h` chunks, and define explicit, observable verification commands for every checkbox. Three developer experience recommendations:

## DevEx Findings & Verification Requirements

### 1. Task Sizing & Dependency Graph
- **Phase 1 (Clean Links)**: 3 tasks (~3 hours total). Zero external dependencies. Self-contained in `src/core/pipeline.cpp` and `tests/test_pipeline.cpp`.
- **Phase 2 (Plasma Activities)**: 4 tasks (~5 hours total). Properly isolates CMake packaging from C++ matching and QML UI.
- **Phase 3 (Watchdog)**: 3 tasks (~3 hours total). Uses mock temp file for isolated testing.
- **Phase 4 (Alt+P)**: 2 tasks (~2 hours total). Minimal touch in `src/app/Controller.cpp` and `src/qml/Picker.qml`.
- **Phase 5 (Empty Picker)**: 2 tasks (~2 hours total). Pure QML state polish.
- **Phase 6 (Packaging & Screenshots)**: 2 tasks (~1 hour total). External dependency on AUR key.

### 2. CI/CD Matrix Safety
- Lane's primary CI runs in an Arch Linux container (`.github/workflows/ci.yml`).
- Adding `find_package(KF6Activities)` will pass cleanly on Arch Linux where `kactivities` is already installed or available in sync repos.
- On minimal environments without `KF6Activities`, the conditional compile guard `#ifdef HAVE_KACTIVITIES` ensures the build still passes 100%.

### 3. Test Requirements & Mutation Invariants
- **Clean Links Tests (`tests/test_pipeline.cpp`)**:
  - Must assert that URLs without tracking parameters return unmodified.
  - Must assert that URLs with mixed parameters (e.g. `?utm_source=x&search=term&fbclid=y`) preserve `search=term`.
  - Must assert that query parameter order of surviving parameters is preserved.
  - Must assert that URLs with encoded query characters (e.g. `redirect=https%3A%2F%2Fexample.com`) are not corrupted.
- **Activity Matching Tests (`tests/test_matcher.cpp`)**:
  - Must test rule matching with matching activity ID, mismatching activity ID, and empty (wildcard) rule activity.
- **Watchdog Tests (`tests/test_watchdog.cpp` or standalone)**:
  - Must test debounced event firing on temporary file touch.

### 4. Code Organization
- Fits established directory conventions:
  - `src/core/` remains pure logic and QtCore (no QtQuick, no UI).
  - `src/app/` holds application controller, watchdog, and KActivities consumer.
  - `src/qml/` handles QML presentation and layer-shell key shortcuts.
