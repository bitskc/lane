#include "UrlEventFilter.h"

#include <QDebug>
#include <QFileOpenEvent>
#include <QUrl>

namespace Lane
{

UrlEventFilter::UrlEventFilter(QObject *parent)
    : QObject(parent)
{
}

void UrlEventFilter::setSink(std::function<void(const QString &url)> sink)
{
    m_sink = std::move(sink);
    if (!m_sink) {
        return;
    }
    while (!m_queue.empty()) {
        m_sink(m_queue.front());
        m_queue.pop_front();
    }
}

bool UrlEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        const auto *foe = static_cast<QFileOpenEvent *>(event);
        enqueue(foe->url());
        // Consumed: the default handler would only log a "no handler"
        // warning, and http/https is ours either way.
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void UrlEventFilter::enqueue(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        qWarning("Lane: ignoring non-web URL from LaunchServices: %s", qPrintable(scheme));
        return;
    }

    if (m_sink) {
        m_sink(url.toString());
        return;
    }

    if (m_queue.size() >= kMaxPending) {
        // Drop the newcomer: queued clicks keep their order and nothing
        // the user already triggered gets evicted by a flood.
        qWarning("Lane: dropping a link open; more than %d pending URLs", kMaxPending);
        ++m_dropped;
        return;
    }
    m_queue.push_back(url.toString());
    // Diagnosing a cold start depends on knowing whether the queue was
    // ever used; this is the only place that is observable.
    qInfo("Lane: queued link open pending controller startup (%d waiting)", pendingCount() + 1);
}

} // namespace Lane
