# Design

Lane is a resident Qt 6 process. The unique D-Bus name is `app.lane.Lane`. Cold-start Qt is too slow for a picker, so autostart runs `lane --daemon`.

## Decision order

1. Explicit rules (first match; picker on conflict unless policy is never)
2. Remembered destination key, path-scoped, longest match wins
3. Unique PWA scope when Prefer PWAs is on
4. Picker policy (`no-rule` default)
5. Default target

Rules beat convenience. Containers are targets like any other: discovered from a profile's `containers.json` and launched via `ext+container:` argv. Silent opens (steps 2, 3, 5) show a hold HUD for `holdMs` unless `holdAutoOpen` is off; rules never hold.

## Pipeline

Incoming URL is normalized, Outlook/Teams wrappers are unwrapped for matching, shorteners can be expanded, then substitutions run. The original wrapper is still opened unless `openUnwrapped` is set.

## Overlay

Layer-shell overlay, exclusive keyboard, dim tint, 440px card. Type-to-filter, 1–8 when the filter is empty, Enter, Esc, Alt+A.
