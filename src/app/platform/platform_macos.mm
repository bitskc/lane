// macOS platform seam. QFileOpenEvent + LaunchServices replace D-Bus;
// NSWorkspace handles default-handler queries and registration. The
// notification and autostart methods are still honest stubs and get
// real bodies in task 7.

#import <AppKit/AppKit.h>

#include "platform.h"

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

class MacPlatform final : public Platform
{
public:
    void notify(const QString &summary, const QString &body, const QString &iconName,
                const QString &actionId, const QString &actionLabel,
                std::function<void()> action) override
    {
        // TODO(macos): UNUserNotificationCenter. Until implemented, drop
        // silently (the caller already gates on toastEnabled).
        Q_UNUSED(summary);
        Q_UNUSED(body);
        Q_UNUSED(iconName);
        Q_UNUSED(actionId);
        Q_UNUSED(actionLabel);
        Q_UNUSED(action);
    }

    void autostartSet(bool on) override { Q_UNUSED(on); }
    bool autostartEnabled() const override { return false; }
    AutostartStatus autostartStatus() override
    {
        AutostartStatus s;
        s.error = AutostartError::NotRegistered;
        return s;
    }

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

        [ws setDefaultApplicationAtURL:ourBundle
                       toOpenURLsWithScheme:@"https"
                        completionHandler:^(NSError *error) {
            if (error) {
                qWarning("Lane: setDefaultApplicationAtURL https failed: %s",
                         qPrintable(nsStr([error localizedDescription])));
                finish(false);
                return;
            }
            [ws setDefaultApplicationAtURL:ourBundle
                           toOpenURLsWithScheme:@"http"
                            completionHandler:^(NSError *error2) {
                if (error2) {
                    qWarning("Lane: setDefaultApplicationAtURL http failed: %s",
                             qPrintable(nsStr([error2 localizedDescription])));
                    finish(false);
                    return;
                }
                // Both calls accepted. Claim success only if a re-query
                // shows us as the handler for both schemes.
                const bool ok = handlerForScheme(@"http", ourPath).isUs
                             && handlerForScheme(@"https", ourPath).isUs;
                m_changeDeclined.store(!ok);
                (*donePtr)(ok);
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
        return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
    }

private:
    std::atomic<bool> m_changeDeclined{false};
};

} // namespace

Platform *platform()
{
    static MacPlatform instance;
    return &instance;
}

} // namespace Lane

