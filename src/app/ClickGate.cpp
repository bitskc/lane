#include "ClickGate.h"

namespace Lane
{

ClickGate::Entry ClickGate::enter(const Pending &click, QString *dropped)
{
    if (dropped) {
        dropped->clear();
    }
    // Checked before the in-flight test. After quit, a click that arrives
    // with nothing in flight would otherwise start a full resolve (a
    // network round trip per shortener hop) only to be discarded.
    if (m_quitting) {
        return Entry::Quit;
    }
    if (m_inFlight) {
        m_pending.append(click);
        if (m_pending.size() > kMaxPending) {
            const Pending oldest = m_pending.takeFirst();
            if (dropped) {
                *dropped = oldest.url;
            }
        }
        return Entry::Queued;
    }
    m_inFlight = true;
    return Entry::Run;
}

void ClickGate::finish()
{
    m_inFlight = false;
}

bool ClickGate::requestSettings()
{
    if (m_quitting) {
        return false;
    }
    if (m_inFlight) {
        m_settings = true;
        return false;
    }
    return true;
}

ClickGate::Rediscover ClickGate::requestRediscover(qint64 windowMs)
{
    if (m_quitting) {
        return Rediscover::Refused;
    }
    if (m_inFlight) {
        // The gate's own follow-up runs the rescan after the click; no
        // window is consumed and no trailing timer is needed.
        m_rediscover = true;
        return Rediscover::Deferred;
    }
    if (m_rediscoverTimer.isValid() && m_rediscoverTimer.elapsed() < windowMs) {
        // Inside the window: the caller schedules one trailing rescan and
        // dedupes repeats with its own flag, so nothing is dropped.
        return Rediscover::QueueTrailing;
    }
    m_rediscoverTimer.start();
    return Rediscover::RunNow;
}

qint64 ClickGate::trailingDelayMs(qint64 windowMs) const
{
    if (!m_rediscoverTimer.isValid()) {
        return 0;
    }
    const qint64 remaining = windowMs - m_rediscoverTimer.elapsed();
    return remaining > 0 ? remaining : 0;
}

ClickGate::Followup ClickGate::takeFollowup()
{
    // Once quitting, the request methods refuse and quit() cleared both
    // flags, so there is nothing to guard against here.
    Followup f;
    // Settings reloads discovery, so it covers a pending rediscover.
    f.settings = m_settings;
    f.rediscover = m_rediscover && !m_settings;
    m_settings = false;
    m_rediscover = false;
    return f;
}

bool ClickGate::takeNext(Pending *out)
{
    // quit() cleared the queue and enter() refuses to refill it, so an empty
    // queue is the whole quit check.
    if (m_pending.isEmpty()) {
        return false;
    }
    *out = m_pending.takeFirst();
    return true;
}

void ClickGate::quit()
{
    m_quitting = true;
    m_pending.clear();
    m_settings = false;
    m_rediscover = false;
}

} // namespace Lane
