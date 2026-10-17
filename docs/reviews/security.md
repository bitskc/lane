# Lane security review, macOS port round

## Verdict
No P0. One P1: Lane claims `public.html`/`public.url` as default-handler targets but drops the `file://` events those claims deliver, so making Lane the default browser silently breaks local HTML/.webloc opens. Two P2s (unauthenticated IPC socket allows URL injection and click snooping; flatpak `--command` gate only blocks interpreters). Seven P3s.

## Findings

| # | Sev | Location | Finding |
|---|-----|----------|---------|
| 1 | P1 | `data/mac/Info.plist.in:64-83`, `src/app/platform/platform_macos.mm:336-358`, `src/app/UrlEventFilter.cpp:41-45` | Lane registers `CFBundleDocumentTypes` for `public.html` and `public.url` with `LSHandlerRank Alternate`, and `makeDefaultHandler` calls `setDefaultApplicationAtURL:toOpenContentType:` on both — making Lane the default handler for local `.html` and `.webloc` documents. `UrlEventFilter::enqueue` drops every non-http/https scheme, and `QFileOpenEvent` for a file open carries `file://`, so the open lands, is consumed, logged, and nothing happens. Lane claims a capability it refuses. |
| 2 | P2 | `src/app/SingleInstance.cpp:131-146, 190-241, 261-289` | The forwarding socket has no peer verification beyond filesystem DAC. A same-UID process can inject `{"args":[..., "URL"]}` frames through `handleArgs` -> `openUrl`, or pre-bind `lane-<uid>.sock` before Lane's first launch and ack `ok` to every forwarded click, snooping and dropping all URL traffic. Mitigations: `LOCAL_PEERCRED` pid check or tighter frame validation. |
| 3 | P2 | `src/core/launcher.cpp:198-217` | `flatpakRunArgsBlocked` rejects `--command=<v>` only when basename(v) is a blocked interpreter; `--command=/tmp/evil` still replaces the program flatpak execs, reachable via a hand-authored `custom:` target or hostile desktop file args. Either reject `--command` unconditionally or narrow the comment. |
| 4 | P3 | `src/core/urlutil.cpp:151-188` | `isPrivateOrLocalHost` misses IPv4-mapped IPv6 (`[::ffff:127.0.0.1]`), integer/hex/octal IPv4 forms, and reserved ranges (192.0.0.0/24, 240.0.0.0/4). Defense-in-depth gap only. |
| 5 | P3 | `src/core/discovery.cpp:1245-1275, 1296-1310` | `readPlistValue` recurses with no depth cap and `plutil` output is read unbounded; a crafted deeply-nested Info.plist in `~/Applications` stack-overflows or memory-spikes the daemon during rescan. Same-UID prerequisite. |
| 6 | P3 | `src/core/discovery.cpp:1459-1461` vs `src/core/launcher.cpp:352-361` | Linux `action:email` passes the sanitized URL verbatim as `xdg-email --body <url>`; depending on xdg-utils quoting, the same mailto-parameter injection class the macOS path fixes remains open on Linux. |
| 7 | P3 | `src/core/launcher.cpp:407-431` vs `src/core/discovery.cpp:1383-1395` | Inside-`Contents/MacOS` containment is checked at discovery only; a `CFBundleExecutable` swapped to an escaping symlink post-scan execs an external binary. Repeat the check at launch. |
| 8 | P3 | `src/app/SingleInstance.cpp:125-136` | macOS socket dir honors `XDG_RUNTIME_DIR` if set to a shared/world-writable dir, reopening predictable-path squat. Restrict to test builds or validate mode 0700 self-owned. |
| 9 | P3 | `src/app/UrlEventFilter.h:30` vs `src/app/UrlEventFilter.cpp:50-53` | Header says overflow drops oldest; code drops newcomer (code is correct). Doc skew only. |
| 10 | P3 | `src/app/platform/platform_macos.mm:343-361` | If `public.url` succeeds and `public.html` fails, `finish(false)` marks declined even though http/https already route to Lane. |

## Considered and fine
`isSafeOpenUrl` (http/https allowlist, 8192 cap, CRLF/NUL rejection, userinfo rejection); `unshortenSync` (allowlisted hosts, HEAD-only, manual redirects, private-host gate, 4-hop pipeline cap); macOS `/usr/bin/open` fixed routes (literal bundleId, percent-encoded mailto); `expandArgs` (argv-vector, no shell); exec guards (interpreter blocklist, symlink-chain walk, macOS extras, fail-closed); `scanMacBundles` (bare-name exec, canonical containment, dedupe, self-exclusion); `SingleInstance` protocol (64KiB cap, acks, claim lock, stale reclaim, UserAccessOption, confstr dir); `handleArgs` (argv[0] stripped, bounded re-entrancy); `UpdateChecker` (https+GitHub allowlist, 3 hops, no cookies); `bundle.sh` (rm before ditto, resign in place, verify every Mach-O); `DBusActivation` (session bus, token scoped); `NotificationActionMap` (32 cap, mutex, one-shot take, lane-action only); `CrashLog` (O_EXCL|O_NOFOLLOW, 0600); router (no Action/incognito defaults); matcher (capped anchored regex).

## Open questions
1. `.webloc` delivery form after content-type claim (verify on MacBook).
2. Chromium `info_cache` key sanitation (`--profile-directory=<key>` embeds raw key).
3. QFileOpenEvent during unshortenSync nested loop (probably fine, unverified on device).
