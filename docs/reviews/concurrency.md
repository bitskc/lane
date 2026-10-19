# Concurrency and lifecycle

Follow-up: the two cross-thread and re-entry holes below are fixed in the tree. `g_macBundlePaths` is published as one map under a mutex, and `bundle:` icons load on the GUI thread so `QFileIconProvider` does not run on the image loader. `unshortenSync` excludes user input. `aboutToQuit` sets a flag that stops `openUrl` from routing, launching, or draining after `quit()` unwinds the nested loop. `SIGTERM` writes one byte to a pipe and quits on the GUI thread, so `systemctl restart` runs `~Controller`. `--rediscover` and `--settings` that arrive during `openUrl` run after that click. A D-Bus `Open` of several URIs restores the activation token before each one. Notification, default-handler, and activation callbacks hold a `QPointer`. The pending-URL cap logs the URL it drops. An open picker's rows are still the decision that opened it; reload does not rebuild them mid-choice.

## Verdict

The URL queue inside `openUrl` does what its comment says: a second open during `unshortenSync` is stored and does not overwrite `m_click`. Two other holes are real. `unshortenSync` runs `QEventLoop::exec()` with every event class enabled, so tray actions, an already-visible picker, and Quit run while that click is still unresolved. And `g_macBundlePaths` is a plain `QHash` written on the GUI thread and read from Qt's image-loader thread whenever a picker or Settings icon is in flight.

Shutdown is not a clean barrier. `QCoreApplication::quit()` (Qt 6.11.2) exits every event loop on the thread, including the unshorten loop, and `openUrl` then keeps going into `applyDecision`. A `systemctl restart` never enters that path at all: nothing handles `SIGTERM`.

## Findings

### P1: `g_macBundlePaths` is written on the GUI thread and read off it

`scanMacBundles` clears and refills a translation-unit `QHash` with no lock:

```1331:1335:src/core/discovery.cpp
QHash<QString, QString> g_macBundlePaths;

QList<DesktopApp> scanMacBundles(const QStringList &dirs)
{
    g_macBundlePaths.clear();
```

```1502:1504:src/core/discovery.cpp
QString macBundlePathForId(const QString &bundleId)
{
    return g_macBundlePaths.value(bundleId);
```

`discoverTargets` always calls `scanMacBundles` (`src/core/discovery.cpp:1563`), and `Controller::reload` always calls `discoverTargets` (`src/app/Controller.cpp:997`). Reload runs from the constructor, from `openSettings` when Settings is not already visible (`src/app/Controller.cpp:737`), and from every `rediscover()` (`src/app/Controller.cpp:753`).

The reader is not the GUI thread. `LaneIcon.qml:20` sets `asynchronous: true`, and `ThemeIconProvider::requestPixmap` calls `macBundlePathForId` for `bundle:` icons (`src/app/ThemeIconProvider.cpp:24`). Qt 6.11's `QQuickPixmapReader` is a `QThread`; `processJob` calls `requestPixmap` on that thread. `QHash` is reentrant and not thread-safe, so a Rediscover (or a Settings open) that overlaps an in-flight bundle icon is a data race. Practical trigger is macOS, where those icons exist. On Linux the hash is still cleared on every reload, but theme icons do not read it.

The same loader call then constructs a `QFileIconProvider` icon and a `QPixmap` (`src/app/ThemeIconProvider.cpp:29`). [INFERENCE] Qt's macOS file-icon backend talks to AppKit on the calling thread, which is not the main thread here.

`reload` also does not reset `m_pickerModel`. An open picker keeps its old rows while `m_targets` and the bundle map are replaced underneath it.

### P1: The unshorten loop processes input before `m_click` is updated

`runPipeline` may call `unshortenSync` up to four times (`src/core/pipeline.cpp:154`). Each call does:

```32:38:src/core/unshorten.cpp
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
```

`exec()` with no flags processes user input, timers, sockets, and deferred deletes. `Controller::openUrl` sets `m_inOpenUrl` before `runPipeline` (`src/app/Controller.cpp:586`) and only assigns `m_click` after it returns (`src/app/Controller.cpp:596`). It stops a running hold, but it does not hide an already-visible picker. `hidePicker` happens later, inside `applyDecision` (`src/app/Controller.cpp:1051`).

So for up to about 7.2s:

- Return on the old picker calls `pickId`, which launches `m_click` as it still is (`src/app/Controller.cpp:663`), the previous URL.
- The tray's Rediscover and Settings actions run `reload()` on the same stack, which is the `g_macBundlePaths` write above, and which changes the `m_config` / `m_targets` that `route()` will use when the pipeline returns (`src/app/Controller.cpp:609`). `runPipeline` itself was given a `Config` copy, so the click and the route can disagree.
- A second `openUrl` is queued (`src/app/Controller.cpp:574`). That part holds.

`QFileOpenEvent` during this window does not show the new URL's picker early. With the sink installed, `UrlEventFilter` calls `openUrl`, the guard queues it, and the new decision runs only after `applyDecision` of the in-flight click. Startup is the other order: `main` calls `handleArgs` before `setSink` (`src/app/main.cpp:182` then `187`), so file-open events that arrive during the first unshorten sit in `UrlEventFilter`'s queue and drain after that `openUrl` returns.

### P1: Quit exits the nested loop, then the pipeline still launches

`requestQuit` calls `QCoreApplication::quit()` when no picker or hold is visible (`src/app/Controller.cpp:766`). On this machine's Qt 6.11.2, `quit()` sends `QEvent::Quit`, and `QCoreApplication::exit()` emits `aboutToQuit` and then calls `QEventLoop::exit` on every loop on the thread. `unshortenSync`'s loop is one of them, so `loop.exec()` returns even when the HEAD request is still outstanding. `unshortenSync` then keeps the original URL (`src/core/unshorten.cpp:40`).

`openUrl` does not look at the quit flag. It still routes, `applyDecision` can still `showPicker` or `launch`, and the pending-URL drain still runs (`src/app/Controller.cpp:609` and `623`). `aboutToQuit` has already been emitted, and Lane connects nothing to it, so the signal does not stop that work either. The process then leaves `app.exec()` and `~Controller` deletes the engines.

The live triggers are macOS: the tray "Quit Lane" action (`src/app/Controller.cpp:87`) and `StandardKey.Quit` (`quitShortcutEnabled` is false on Linux). Linux has no tray Quit.

### P2: Notification and activation callbacks keep a raw `Controller*`

`NotificationActionMap` is a function-local static (`src/app/platform/platform_macos.mm:126`). `take()` runs the stored `std::function` with the lock released (`src/app/NotificationActionMap.h:55`). The macOS delegate now hops to the main queue before `take()` (`src/app/platform/platform_macos.mm:166`). That fixes the off-queue GUI call. It does not tie the callback to the controller.

`onMimeappsChanged` stores `[this]() { makeDefaultBrowser(); }` (`src/app/Controller.cpp:313`). `makeDefaultBrowser` stores another `[this]` into `makeDefaultHandler` (`src/app/Controller.cpp:760`). The macOS completion calls that function from `dispatch_async` onto the main queue (`src/app/platform/platform_macos.mm:362`). The map and `MacPlatform` both outlive the stack `Controller` in `main`. [INFERENCE] A block already queued on the main GCD queue can run while `~Controller` is tearing down `QWindow`s, because AppKit spins the run loop there. There is no `QPointer` or generation check around those callbacks.

The Linux activation fallback has the same shape. `requestActivationAndLaunch` captures `this` in `finish` (`src/app/Controller.cpp:1147`). `LinuxPlatform::requestActivationToken` arms `QTimer::singleShot(timeoutMs, this, ...)` where `this` is the process-lifetime platform object (`src/app/platform/platform_linux.cpp:168`), and `QFuture::then(this, finish)` uses that same context. Destroying the controller does not cancel the timer. It does not fire after `app.exec()` returns unless something processes timers during teardown. The capture is still the wrong lifetime.

`QTimer::singleShot` in `SingleInstance` uses the socket as context (`src/app/SingleInstance.cpp:281`), so that one is cancelled with the socket. Hold and tray lambdas are connected to children of `Controller`, so those are cancelled with it.

### P2: Socket and D-Bus delivery re-enter `handleArgs` under the nested loop

`SingleInstance` acks, disconnects, then calls `onMessage` (`src/app/SingleInstance.cpp:270`). The comment is right that `handleArgs` can run a nested loop and that the socket may be deleted there. Each connection has its own buffer and stops after one newline-terminated frame, including a frame split across `readyRead`. Concurrent connects are accepted in a loop.

What is not serialized is `onMessage`. `readyRead` is a direct connection. A second client's frame, processed because the unshorten loop is running, calls `handleArgs` on the same stack. A URL hits the `m_inOpenUrl` queue. `--rediscover` and `--settings` do not: they call `reload()` / `openSettings()` immediately (`src/app/Controller.cpp:529`).

The ack is sent before that work. `data/systemd/lane.service.in` is `Restart=on-failure`. `CrashLog` installs handlers for `SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGILL`, and `SIGFPE` only (`src/app/CrashLog.cpp:164`). `SIGTERM` from `systemctl restart` kills the process with no `aboutToQuit` and no `~Controller`. If the forwarder already received `ok\n`, it has exited, and the URL dies with the daemon.

D-Bus `Open` has a related token hole. `withActivationToken` sets the env var for the whole URI list (`src/app/DBusActivation.cpp:56`). The first `openUrl` copies it and unsets the variable (`src/app/Controller.cpp:563`). Later URIs in that same `Open` capture an empty token. The drain path puts a queued token back only when that pending entry had one (`src/app/Controller.cpp:625`).

### P3: The pending-URL cap drops clicks with no log

Past four queued opens the oldest entry is removed (`src/app/Controller.cpp:579`). Nothing is logged. `UrlEventFilter` does the opposite on purpose: it drops the newcomer and warns (`src/app/UrlEventFilter.cpp:97`, cap 8). A burst during one 1.8s hop keeps the newest four and discards an earlier click the user already made.

### P3: The default-handler poll runs inside the unshorten loop

On macOS the 60s timer calls `onMimeappsChanged` (`src/app/Controller.cpp:130`). On Linux the file watcher can do the same. Either delivery during `loop.exec()` calls `defaultHandlerState()`. The Linux implementation blocks in `QProcess::waitForFinished(1500)` per scheme (`src/app/platform/platform_linux.cpp:195`). Qt 6.11's unix `waitForFinished` polls the child; it does not pump the Qt event loop, so this stalls the nested loop instead of re-entering it. The timer object itself is a child of `Controller` and is disconnected in `~QObject` before children die, so a quit does not leave the timeout pointing at a dead controller.

## Considered and fine

- `updateStateChanged` is a direct connection from `UpdateChecker::statusChanged` (`src/app/Controller.cpp:80`). `QNetworkReply::finished` is delivered on the thread that owns the reply, which is the GUI thread. `m_changeDeclined` is an `std::atomic<bool>` written from the main queue and read from `defaultHandlerState` (`src/app/platform/platform_macos.mm:455`).
- `NotificationActionMap`'s mutex covers the map. After the main-queue hop, `add` and `take` in production both run on the main thread. Calling the callback outside the lock avoids a deadlock if the callback calls `add`. The actions shipped today do not call `take` reentrantly.
- `OverlayWatcher` is parented to the `QWindow` it filters (`src/app/platform/platform_macos.mm:376`). Window destruction deletes the child, and Qt removes an event filter when the filter object dies. `configureOverlayWindow` runs once per engine, from `ensurePickerEngine` / `ensureHoldEngine`.
- `~Controller` deletes the three QML engines before the `QObject` child phase (`src/app/Controller.cpp:137`), so bindings drop while the controller and the models are still alive. The parentless tray `QMenu` is deleted in that destructor.
- The pending-URL drain clears `m_inOpenUrl` and calls `openUrl` again with no event loop in between (`src/app/Controller.cpp:618`), so a third click cannot slip through that gap. `QProcess::waitForFinished` inside plist conversion does not pump Qt events, so a scan does not re-enter `openUrl` by itself. The cross-thread hash race above is the one that remains.
- Hold `finished` is not emitted by `stop()`. `openUrl` stops a running hold before the pipeline (`src/app/Controller.cpp:590`), so the hold's launch lambda does not fire for a click that a new URL superseded.

## Open questions

- Whether a GCD block queued just before Quit actually runs inside `~QWindow` on the deployed macOS Qt. The missing lifetime guard is in the source; the teardown spin is the part not executed here.
- `QFileIconProvider` on the loader thread: the call site is off the GUI thread; the AppKit crash is inferred from Qt's macOS icon backend, not from a Lane crash log.
- There is no in-app "restart service" slot. A supervised restart is `SIGTERM`. Nothing in the process turns that into `aboutToQuit`.
