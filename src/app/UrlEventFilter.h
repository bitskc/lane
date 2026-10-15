#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <functional>

class QUrl;

namespace Lane
{

// Delivers LaunchServices URL opens to the Controller on macOS.
//
// LaunchServices posts a QFileOpenEvent to the application object for
// every `open -b` or link click routed to Lane, including ones that
// arrive before Controller exists (cold start). The filter is installed
// on QApplication right after construction: events before a sink is
// set are held in a bounded FIFO and drained when setSink() runs; after
// that they dispatch inline.
//
// Only http and https are accepted; anything else LaunchServices offers
// (files, mailto handed by mistake) is ignored with a log. The queue is
// capped at kMaxPending: past it, the oldest pending URL is dropped with
// a qWarning rather than buffering clicks without bound or dropping the
// newest intent.
class UrlEventFilter : public QObject
{
    Q_OBJECT
public:
    static constexpr int kMaxPending = 8;

    explicit UrlEventFilter(QObject *parent = nullptr);

    // Sets where accepted URLs go and immediately drains anything queued
    // while the controller was being created. Pass an empty function to
    // detach (tests only).
    void setSink(std::function<void(const QString &url)> sink);

    int pendingCount() const { return static_cast<int>(m_queue.size()); }
    int droppedCount() const { return m_dropped; }

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void enqueue(const QUrl &url);

    std::deque<QString> m_queue;
    std::function<void(const QString &url)> m_sink;
    int m_dropped = 0;
};

} // namespace Lane
