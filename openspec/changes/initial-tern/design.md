# Design

Tern is a resident Qt 6 process. The unique D-Bus name is `app.tern.Tern`. Cold-start Qt is too slow for a picker, so autostart runs `tern --daemon`.

## Decision order

1. Explicit rules (first match; picker on conflict unless policy is never)
2. Remembered host from “Always for this site”
3. Unique PWA scope when Prefer PWAs is on
4. Picker policy (`no-rule` default)
5. Default target

## Pipeline

Incoming URL is normalized, Outlook/Teams wrappers are unwrapped for matching, shorteners can be expanded, then substitutions run. The original wrapper is still opened unless `openUnwrapped` is set.

## Overlay

Layer-shell overlay, exclusive keyboard, blur, 520px card. Type-to-filter, 1–9 when the filter is empty, Enter, Esc, Alt+A.
