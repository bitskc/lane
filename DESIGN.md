# DESIGN.md

## Product

Tern is a default-browser proxy. The product is **not** a settings window. The product is what happens in the 200ms after a click: either nothing visible (correct app already opened) or a quiet overlay that can be dismissed from the keyboard.

## Feel

Mac quality on Plasma means:

- Resident process. Cold-start Qt is too slow for a picker.
- Layer-shell overlay, exclusive keyboard, blur, Plasma accent.
- One decision per unknown site, then memory.
- PWAs rank above browser windows. GitHub BITS is an app, not a tab.

## Visual

- Overlay dim 45% black
- Card ~520px, 18px radius, 94% opaque `Kirigami.Theme.backgroundColor`
- Rows 52px, 12px radius, highlight at 28% accent
- Typeface: system / Noto Sans via Kirigami
- Motion: compositor handles the layer map; no bouncy animation theatre
- Icon: Plasma blue rounded square, white tern silhouette

## Decision order

1. Explicit rules (first match)
2. Remembered host from “Always for …”
3. Unique PWA scope, if Prefer PWAs is on
4. Picker policy (`no-rule` default)
5. Default target

Rules beat convenience. A work GitHub rule will beat the GitHub PWA.

## Non-goals (v1)

- Lua. Browser Tamer’s scripts are power-user surface area we can add with QJSEngine later.
- Firefox containers unless the open-external-links addon is present (not detected yet).
- Stealing PDF / mailto handlers.

## Strategic debt from Browser Tamer

Keep: discovery fingerprinting, O365 unwrap-but-open-original, shortener list, picker vs toast split, process/title matching.

Leave: ImGui radial picker, Windows registry, frameless Chromium windows (can add).
