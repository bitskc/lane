// macOS platform seam. QFileOpenEvent + LaunchServices replace D-Bus;
// NSWorkspace handles default-handler queries; UNUserNotificationCenter
// delivers toasts; SMAppService owns the login item. The overlay method
// applies AppKit chrome on every show.

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UserNotifications.h>

#include "platform.h"

#include "../NotificationActionMap.h"

#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QWindow>

#include <atomic>

#include <objc/runtime.h>
#include <memory>

namespace Lane
{

namespace
{

QString nsStr(NSString *s)
{
    return QString::fromNSString(s);
}

// What LaunchServices reports as the handler for one scheme, compared
// against the running bundle. isUs requires both the bundle id AND the
// resolved real path to match so a second Lane.app elsewhere on disk is
// reported as OtherCopy instead of being mistaken for this install.
struct HandlerInfo
{
    bool found = false;
    bool sameId = false;
    bool isUs = false;
    QString name;
    QString path;
};

HandlerInfo handlerForScheme(NSString *scheme, const QString &ourPath)
{
    HandlerInfo h;
    NSURL *probe = [NSURL URLWithString:[NSString stringWithFormat:@"%@://example.invalid/", scheme]];
    NSURL *appURL = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:probe];
    if (!appURL) {
        return h;
    }
    h.found = true;
    NSBundle *bundle = [NSBundle bundleWithURL:appURL];
    NSString *bid = [bundle bundleIdentifier];
    h.sameId = [bid isEqualToString:[[NSBundle mainBundle] bundleIdentifier]];
    h.path = nsStr([[appURL URLByResolvingSymlinksInPath] path]);
    NSString *name = [[bundle infoDictionary] objectForKey:@"CFBundleDisplayName"];
    if (!name) {
        name = [[bundle infoDictionary] objectForKey:@"CFBundleName"];
    }
    if (!name) {
        name = [[appURL lastPathComponent] stringByDeletingPathExtension];
    }
    h.name = nsStr(name);
    h.isUs = h.sameId && (h.path == ourPath);
    return h;
}

NSString *ourResolvedPath()
{
    return [[[[NSBundle mainBundle] bundleURL] URLByResolvingSymlinksInPath] path];
}

// Applies the AppKit-side chrome decisions that only exist once the
// native NSWindow does: joining the active Space instead of the Space
// the window was born in, and making Lane the active app so a borderless
// accessory-app window can take key events. Decision 8 ordering: Qt
// flags first (set in configureOverlayWindow), then this on every show.
void applyMacOverlayChrome(QWindow *window)
{
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *nsw = [view window];
    if (!nsw) {
        return;
    }
    // Move to the Space where the user triggered the pick instead of
    // pinning to the Space the window was created on.
    nsw.collectionBehavior = NSWindowCollectionBehaviorMoveToActiveSpace;
    [nsw makeKeyAndOrderFront:nil];
    [nsw orderFrontRegardless];

    // An accessory app (LSUIElement) does not become active on its own;
    // without this the overlay shows but keystrokes go to the previous app.
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    }

    // On macOS 14+, [NSApp activate] is cooperative and has no effect when
    // invoked from a background daemon without an activation token.
    // If Lane is not yet active, activate via AppleScript so the picker
    // takes keyboard focus from whatever application invoked it.
    if (![NSRunningApplication currentApplication].isActive) {
        NSString *bid = [[NSBundle mainBundle] bundleIdentifier];
        if (bid.length > 0) {
            NSString *source = [NSString stringWithFormat:@"tell application id \"%@\" to activate", bid];
            NSAppleScript *as = [[NSAppleScript alloc] initWithSource:source];
            [as executeAndReturnError:nil];
        }
    }
    window->requestActivate();
}

// Re-applies overlay chrome every time the window is shown: collection
// behavior and app activation do not stick across hides on macOS.
class OverlayWatcher final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            applyMacOverlayChrome(static_cast<QWindow *>(obj));
        }
        return QObject::eventFilter(obj, event);
    }
};

} // namespace

// The action map and delegate are shared state because the notification
// response callback arrives on the delegate long after notify() returned.
// QObject-only: accessible from the test binary too (test_notifymap).
NotificationActionMap &notificationActions()
{
    static NotificationActionMap m;
    return m;
}

// macOS notification authorization is a per-bundle state the user can
// change in System Settings at any time; notify() refreshes it, the
// Overview banner reads this.
bool &notificationsDenied()
{
    static bool denied = false;
    return denied;
}

} // namespace Lane

// UNUserNotificationCenter delegate: action buttons keep a fixed
// actionIdentifier ("lane-action") so this handler stays simple. Each
// toast that has a button gets its own category id (lane.action.<key>)
// so a later toast's label cannot rewrite an earlier toast's button.
@interface LaneNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation LaneNotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
        didReceiveNotificationResponse:(UNNotificationResponse *)response
                 withCompletionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(center);
    // Only the explicit action button runs the stored handler; a body tap
    // (UNNotificationDefaultActionIdentifier) must not fire it, or a
    // plain click on the "default browser changed" toast would pop the
    // LaunchServices consent prompt.
    if ([response.actionIdentifier isEqualToString:@"lane-action"]) {
        const QString key = Lane::nsStr(response.notification.request.identifier);
        // take() extracts and runs the stored std::function, which lands
        // in Controller and mutates GUI state; the delegate callback runs
        // on an arbitrary UserNotifications queue, so marshal the action
        // run onto the main queue like every other completion in here.
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!Lane::notificationActions().take(key)) {
                // Expired or evicted action: the notification still
                // showed, the button just has nothing to run.
                qWarning("Lane: notification action for %s no longer registered", qPrintable(key));
            }
        });
    }
    completionHandler();
}

@end

namespace Lane
{

namespace
{


// Finder/Spotlight "open" of an already-running LSUIElement app does not
// start a second process: LaunchServices sends applicationShouldHandleReopen.
// Without a handler that feels like a dead click (no Dock icon, no window).
static std::function<void()> g_reopenHandler;
static BOOL (*g_originalReopen)(id, SEL, NSApplication *, BOOL) = nullptr;

static BOOL laneApplicationShouldHandleReopen(id self, SEL _cmd, NSApplication *app, BOOL hasVisibleWindows)
{
    if (g_reopenHandler) {
        g_reopenHandler();
    }
    if (g_originalReopen) {
        return g_originalReopen(self, _cmd, app, hasVisibleWindows);
    }
    return YES;
}

static void installReopenHook(std::function<void()> handler)
{
    g_reopenHandler = std::move(handler);
    id del = NSApp.delegate;
    if (!del) {
        qWarning("Lane: no NSApp delegate yet; reopen hook not installed");
        return;
    }
    Class cls = object_getClass(del);
    const SEL sel = @selector(applicationShouldHandleReopen:hasVisibleWindows:);
    Method existing = class_getInstanceMethod(cls, sel);
    if (existing) {
        g_originalReopen = reinterpret_cast<decltype(g_originalReopen)>(
            method_setImplementation(existing, reinterpret_cast<IMP>(laneApplicationShouldHandleReopen)));
    } else if (!class_addMethod(cls, sel, reinterpret_cast<IMP>(laneApplicationShouldHandleReopen), "B@:@B")) {
        qWarning("Lane: could not install applicationShouldHandleReopen hook");
    }
}

class MacPlatform final : public Platform
{
public:
    void notify(const QString &summary, const QString &body, const QString &iconName,
                const QString &actionId, const QString &actionLabel,
                std::function<void()> action) override
    {
        // LSUIElement apps need no iconName: macOS shows our app icon.
        Q_UNUSED(iconName);
        Q_UNUSED(actionId);

        UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
        static LaneNotificationDelegate *delegate = [LaneNotificationDelegate new];
        center.delegate = delegate;

        // The const QString & parameters alias caller temporaries that die
        // when this function returns; the blocks below run after that, so
        // the captures must own copies. (Crash: use-after-free inside
        // QString::toNSString on the completion queue.)
        const QString summaryCopy = summary;
        const QString bodyCopy = body;
        const QString actionLabelCopy = actionLabel;
        auto actionCopy = std::make_shared<std::function<void()>>(std::move(action));
        [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (settings.authorizationStatus == UNAuthorizationStatusAuthorized
                    || settings.authorizationStatus == UNAuthorizationStatusProvisional) {
                    Lane::notificationsDenied() = false;
                    postNotification(center, summaryCopy, bodyCopy, actionLabelCopy, *actionCopy);
                    return;
                }
                if (settings.authorizationStatus == UNAuthorizationStatusDenied) {
                    Lane::notificationsDenied() = true;
                    qWarning("Lane: notifications are denied in System Settings; toast dropped");
                    return;
                }
                // Not determined yet: request once, lazily on first use.
                [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                                      completionHandler:^(BOOL granted, NSError *error) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (error) {
                            qWarning("Lane: notification authorization error: %s",
                                     qPrintable(nsStr([error localizedDescription])));
                        }
                        Lane::notificationsDenied() = !granted;
                        if (granted) {
                            postNotification(center, summaryCopy, bodyCopy, actionLabelCopy, *actionCopy);
                        }
                    });
                }];
            });
        }];
    }

    void autostartSet(bool on) override
    {
        SMAppService *svc = [SMAppService mainAppService];
        NSError *error = nil;
        if (on) {
            if (![svc registerAndReturnError:&error]) {
                logSMError(error);
            }
        } else {
            if (![svc unregisterAndReturnError:&error] && error) {
                logSMError(error);
            }
        }
    }

    bool autostartEnabled() const override
    {
        return [SMAppService mainAppService].status == SMAppServiceStatusEnabled;
    }

    AutostartStatus autostartStatus() override
    {
        AutostartStatus s;
        switch ([SMAppService mainAppService].status) {
        case SMAppServiceStatusEnabled:
            s.enabled = true;
            break;
        case SMAppServiceStatusRequiresApproval:
            s.error = AutostartError::RequiresApproval;
            break;
        case SMAppServiceStatusNotRegistered:
            s.error = AutostartError::NotRegistered;
            break;
        case SMAppServiceStatusNotFound:
            s.error = AutostartError::InvalidSignature;
            break;
        default:
            s.error = AutostartError::None;
            break;
        }
        return s;
    }

    bool notificationsDenied() const override { return Lane::notificationsDenied(); }

    bool watchesDefaultHandlerByFile() const override { return false; }

    DefaultHandlerState defaultHandlerState() override
    {
        const QString ourPath = nsStr(ourResolvedPath());
        const HandlerInfo http = handlerForScheme(@"http", ourPath);
        const HandlerInfo https = handlerForScheme(@"https", ourPath);

        DefaultHandlerState s;
        s.currentHandler = http.found ? http.name : https.name;
        const bool bothUs = http.isUs && https.isUs;
        if (bothUs) {
            s.isDefault = true;
            s.status = DefaultHandlerState::Status::Registered;
            s.currentHandler = QStringLiteral("Lane");
        } else if (http.isUs || https.isUs) {
            s.status = DefaultHandlerState::Status::Partial;
        } else if (http.sameId || https.sameId) {
            // Our bundle id at a different path: another Lane copy owns it.
            s.status = DefaultHandlerState::Status::OtherCopy;
        } else if (m_changeDeclined.load()) {
            // We asked to become the handler, the call finished, and the
            // handler still is not us: the consent prompt was declined.
            s.status = DefaultHandlerState::Status::Declined;
        } else {
            s.status = DefaultHandlerState::Status::Unknown;
        }
        return s;
    }

    void makeDefaultHandler(std::function<void(bool ok)> done) override
    {
        auto donePtr = std::make_shared<std::function<void(bool)>>(std::move(done));
        NSURL *ourBundle = [[NSBundle mainBundle] bundleURL];
        const QString ourPath = nsStr(ourResolvedPath());
        NSWorkspace *ws = [NSWorkspace sharedWorkspace];

        auto finish = [this, donePtr](bool ok) {
            // done(false) is reported as a failed request, never a
            // decline: the declined banner is set only by the
            // accepted-but-handler-is-not-us path below, which is the
            // only outcome that proves the user refused the change.
            // API errors (permErr class included) land here and must
            // not lie about cause.
            Q_UNUSED(this);
            (*donePtr)(ok);
        };

        // On macOS 26 `setDefaultApplicationAtURL:toOpenURLsWithScheme:`
        // fails with permErr (NSError 256 wrapping OSStatus -54 in
        // LSExternalPriv.mm) for every caller/target, signed or not — the
        // scheme-handler write path is gated. The content-type variant is
        // not: setting `public.url` re-points http/https handling
        // (verified by URLForApplicationToOpenURL resolving to this
        // The content-type write path works where the scheme setter is
        // gated. Claim only `public.url`: that is what re-points
        // http/https. Do NOT also claim `public.html` — that would make
        // Lane the preferred opener for local .html/.xhtml documents,
        // whose opens arrive as file:// URLs the router cannot serve
        // (verified: a Finder double-click after this call silently does
        // nothing). Claiming just public.url leaves Safari/etc. owning
        // file types while Lane owns links.
        UTType *urlType = [UTType typeWithIdentifier:@"public.url"];
        if (!urlType) {
            finish(false);
            return;
        }
        [ws setDefaultApplicationAtURL:ourBundle
                       toOpenContentType:urlType
                        completionHandler:^(NSError *error) {
            if (error) {
                qWarning("Lane: setDefaultApplicationAtURL public.url failed: %s",
                         qPrintable(nsStr([error localizedDescription])));
                dispatch_async(dispatch_get_main_queue(), ^{ finish(false); });
                return;
            }
            // Claim success only if a re-query shows us as the handler
            // for both schemes; set m_changeDeclined only when the call
            // was accepted yet the handler still is not us (a real
            // decline), not on API errors.
            const bool ok = handlerForScheme(@"http", ourPath).isUs
                         && handlerForScheme(@"https", ourPath).isUs;
            dispatch_async(dispatch_get_main_queue(), ^{
                m_changeDeclined.store(!ok);
                (*donePtr)(ok);
            });
        }];
    }

    void configureOverlayWindow(QWindow *window, const QString &scope) override
    {
        // Decision 8 step 1: Qt flags. Borderless, top-most, accepts keys
        // (no WindowDoesNotAcceptFocus). The AppKit side (collection
        // behavior, activation) runs on every show via OverlayWatcher.
        Q_UNUSED(scope);
        window->setFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        auto *watcher = new OverlayWatcher(window);
        window->installEventFilter(watcher);
    }

    bool registerActivation(std::function<void(const QStringList &args)> handleArgs,
                            std::function<void(const QString &url)> openUrl) override
    {
        // LaunchServices activation needs no registration: clicks arrive as
        // QFileOpenEvent (task 4.2 installs the filter). Always "registered".
        Q_UNUSED(openUrl);
        // open -a / Spotlight on an already-running LSUIElement app sends
        // applicationShouldHandleReopen instead of a second process; open
        // Settings so that path is not a dead click. --settings goes through
        // the same Controller path as the status-item action.
        installReopenHook([handleArgs = std::move(handleArgs)]() {
            if (!handleArgs) {
                return;
            }
            handleArgs({QCoreApplication::applicationFilePath(), QStringLiteral("--settings")});
        });
        return true;
    }

    void requestActivationToken(QWindow *source,
                                std::function<void(QString)> done,
                                int timeoutMs) override
    {
        // macOS has no xdg-activation token; callers receive an empty token.
        Q_UNUSED(source);
        Q_UNUSED(timeoutMs);
        done(QString());
    }

    bool prefersReducedMotion() const override
    {
        // accessibilityDisplayShouldReduceMotion is macOS 14+; the plist
        // floor is 13.0, so guard and fall back to "no reduction" there.
        if (@available(macOS 14.0, *)) {
            return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
        }
        return false;
    }

    QString clipboardText() const override
    {
        // NSPasteboard works for an LSUIElement agent; QClipboard often
        // returns empty while Lane is not the key app.
        NSString *s = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        return s ? QString::fromNSString(s).trimmed() : QString();
    }

private:
    static void logSMError(NSError *error)
    {
        qWarning("Lane: SMAppService failed: %s (code %ld)",
                 qPrintable(nsStr([error localizedDescription])),
                 static_cast<long>([error code]));
    }

    static void postNotification(UNUserNotificationCenter *center,
                                 const QString &summary, const QString &body,
                                 const QString &actionLabel,
                                 std::function<void()> action)
    {
        UNMutableNotificationContent *content = [UNMutableNotificationContent new];
        content.title = summary.toNSString();
        content.body = body.toNSString();

        // Unique request ids so consecutive plain toasts stack instead of
        // replacing each other under the shared "lane" key.
        static std::atomic<quint64> toastSeq{0};
        NSString *identifier =
            [NSString stringWithFormat:@"toast-%llu",
                      static_cast<unsigned long long>(toastSeq.fetch_add(1))];
        if (action) {
            identifier = notificationActions().add(std::move(action)).toNSString();
            // Per-request category so the button label tracks this toast.
            // Keeping a bounded set (same size as NotificationActionMap)
            // means an older outstanding toast still has its category
            // registered when a newer one posts.
            UNNotificationAction *btn =
                [UNNotificationAction actionWithIdentifier:@"lane-action"
                                                     title:actionLabel.toNSString()
                                                   options:UNNotificationActionOptionForeground];
            NSString *categoryId =
                [NSString stringWithFormat:@"lane.action.%@", identifier];
            UNNotificationCategory *category =
                [UNNotificationCategory categoryWithIdentifier:categoryId
                                                       actions:@[ btn ]
                                             intentIdentifiers:@[]
                                                       options:0];
            static NSMutableDictionary *categories = [NSMutableDictionary new];
            static NSMutableArray *categoryOrder = [NSMutableArray new];
            [categories setObject:category forKey:categoryId];
            [categoryOrder addObject:categoryId];
            while (static_cast<int>(categoryOrder.count)
                   > NotificationActionMap::kMaxEntries) {
                NSString *oldest = categoryOrder.firstObject;
                [categoryOrder removeObjectAtIndex:0];
                [categories removeObjectForKey:oldest];
            }
            [center setNotificationCategories:
                [NSSet setWithArray:categories.allValues]];
            content.categoryIdentifier = categoryId;
        }

        UNNotificationRequest *request =
            [UNNotificationRequest requestWithIdentifier:identifier content:content trigger:nil];
        [center addNotificationRequest:request withCompletionHandler:^(NSError *error) {
            if (error) {
                qWarning("Lane: notification request failed: %s",
                         qPrintable(nsStr([error localizedDescription])));
            }
        }];
    }

    std::atomic<bool> m_changeDeclined{false};
};

} // namespace

Platform *platform()
{
    static MacPlatform instance;
    return &instance;
}

} // namespace Lane
