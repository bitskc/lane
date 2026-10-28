#pragma once

#include <QElapsedTimer>
#include <QList>

namespace Lane
{

// The ordering rules for one in-flight click, with no Qt event loop, window
// or platform object in it, so they can be tested without a Controller.
//
// Controller::openUrl runs a nested event loop while a shortened link is
// resolved. During that loop another link can arrive, and so can
// --rediscover, --settings or a quit. This type decides what each of those
// does. Controller owns one and only performs the side effects it names.
class ClickGate
{
public:
    // Links that arrive while a click is being resolved are kept in order.
    // Past this many the oldest is dropped, because the newest is the one
    // the user most likely still means.
    static constexpr qsizetype kMaxPending = 4;

    struct Pending {
        QString url;
        bool forcePicker = false;
        QString activationToken;
    };

    enum class Entry {
        Run,    // start resolving this click now
        Queued, // a click is in flight; it was added to the queue
        Quit,   // Lane is quitting; do not resolve or launch anything
    };

    // Called at the top of openUrl. `dropped` receives the url pushed out of
    // a full queue so the caller can log it, and is empty otherwise.
    Entry enter(const Pending &click, QString *dropped = nullptr);

    // The in-flight click finished (applied or abandoned).
    void finish();

    // --settings while a click is in flight waits for it.
    // Returns true when the caller must run the command now.
    bool requestSettings();

    enum class Rediscover {
        RunNow,       // run the rescan now; consumes the rate-limit window
        Deferred,     // a click is in flight (or quitting); the gate's own
                      // follow-up handles it, no trailing timer needed
        QueueTrailing,// inside the window: caller should schedule one
                      // trailing rescan at window end (trailingDelayMs)
    };

    // --rediscover: callers include tray, argv, D-Bus actions and any
    // session-bus client, so bursts are coalesced to one rescan per
    // `windowMs` (default 2s; tests inject a shorter window). A call inside
    // the window reports QueueTrailing every time; the caller dedupes the
    // actual trailing timer with its own pending flag, so a rescan can
    // never be silently dropped.
    Rediscover requestRediscover(qint64 windowMs = 2000);

    // Milliseconds until the current window ends, for scheduling the
    // trailing rescan. Only meaningful after QueueTrailing.
    qint64 trailingDelayMs(qint64 windowMs = 2000) const;


    // What to run once the in-flight click is done, in order. Settings
    // implies a rediscover, so asking for both runs only Settings.
    struct Followup {
        bool settings = false;
        bool rediscover = false;
    };
    Followup takeFollowup();

    // The next queued link, if any and not quitting.
    bool takeNext(Pending *out);

    // aboutToQuit. Drops everything that was waiting.
    void quit();

    bool quitting() const { return m_quitting; }
    bool inFlight() const { return m_inFlight; }
    qsizetype pendingCount() const { return m_pending.size(); }

private:
    QList<Pending> m_pending;
    bool m_inFlight = false;
    bool m_quitting = false;
    bool m_settings = false;
    bool m_rediscover = false;
    // Rate-limit window state for requestRediscover.
    QElapsedTimer m_rediscoverTimer;
};

} // namespace Lane
