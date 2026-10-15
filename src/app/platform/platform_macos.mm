// macOS platform seam. QFileOpenEvent + LaunchServices replace D-Bus;
// NSWorkspace handles default-handler queries; UNUserNotificationCenter
// delivers toasts; SMAppService owns the login item. The overlay method
// applies AppKit chrome on every show.

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UserNotifications.h>

#include "platform.h"

#include "../NotificationActionMap.h"

#include <QDebug>
#include <QUrl>
#include <QWindow>

#include <atomic>
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

// UNUserNotificationCenter delegate: our notification action buttons
// carry the dynamic label Qt callers pass; the single "lane.action"
// category is re-created with the label for each request that has one.
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
        if (!Lane::notificationActions().take(key)) {
            // Expired or evicted action: the notification still showed,
            // the button just has nothing to run. Log, never crash.
            qWarning("Lane: notification action for %s no longer registered", qPrintable(key));
        }
    }
    completionHandler();
}

@end

namespace Lane
{

namespace
{

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

        // All UNUserNotificationCenter completion handlers run on an
        // Apple-managed queue; the denied flag is read on the GUI thread
        // and postNotification touches Qt objects, so every piece lands
        // on the main queue.
        [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (settings.authorizationStatus == UNAuthorizationStatusAuthorized
                    || settings.authorizationStatus == UNAuthorizationStatusProvisional) {
                    Lane::notificationsDenied() = false;
                    postNotification(center, summary, body, actionLabel, action);
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
                            postNotification(center, summary, body, actionLabel, action);
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

        auto finish = [this, donePtr, ourPath](bool ok) {
            if (!ok) {
                // A finished attempt that did not make us the handler is
                // a decline, whether the user cancelled the prompt or the
                // system rejected the change.
                m_changeDeclined.store(true);
            }
            (*donePtr)(ok);
        };

        // NSWorkspace completions run on an Apple-managed queue, but
        // `done` lands in Controller and emits Qt signals, so finish work
        // is marshalled to the main queue.
        [ws setDefaultApplicationAtURL:ourBundle
                       toOpenURLsWithScheme:@"https"
                        completionHandler:^(NSError *error) {
            if (error) {
                qWarning("Lane: setDefaultApplicationAtURL https failed: %s",
                         qPrintable(nsStr([error localizedDescription])));
                dispatch_async(dispatch_get_main_queue(), ^{ finish(false); });
                return;
            }
            [ws setDefaultApplicationAtURL:ourBundle
                           toOpenURLsWithScheme:@"http"
                            completionHandler:^(NSError *error2) {
                if (error2) {
                    qWarning("Lane: setDefaultApplicationAtURL http failed: %s",
                             qPrintable(nsStr([error2 localizedDescription])));
                    dispatch_async(dispatch_get_main_queue(), ^{ finish(false); });
                    return;
                }
                // Both calls accepted. Claim success only if a re-query
                // shows us as the handler for both schemes.
                const bool ok = handlerForScheme(@"http", ourPath).isUs
                             && handlerForScheme(@"https", ourPath).isUs;
                dispatch_async(dispatch_get_main_queue(), ^{
                    m_changeDeclined.store(!ok);
                    (*donePtr)(ok);
                });
            }];
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
        Q_UNUSED(handleArgs);
        Q_UNUSED(openUrl);
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

        NSString *identifier = @"lane";
        if (action) {
            // One category, re-declared per request so the button label
            // follows the caller's actionLabel instead of being frozen.
            UNNotificationAction *btn =
                [UNNotificationAction actionWithIdentifier:@"lane-action"
                                                     title:actionLabel.toNSString()
                                                   options:UNNotificationActionOptionForeground];
            UNNotificationCategory *category =
                [UNNotificationCategory categoryWithIdentifier:@"lane.action"
                                                       actions:@[ btn ]
                                             intentIdentifiers:@[]
                                                       options:0];
            [center setNotificationCategories:[NSSet setWithObject:category]];
            content.categoryIdentifier = @"lane.action";
            identifier = notificationActions().add(std::move(action)).toNSString();
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
