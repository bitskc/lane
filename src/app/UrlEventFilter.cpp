#include "UrlEventFilter.h"

#include <QDebug>
#include <QFile>
#include <QFileOpenEvent>
#include <QRegularExpression>
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

// Reads the embedded URL out of a .webloc or .url file (the two link
// document types LaunchServices can hand a public.url claimant). These
// are plist/XML or INI-ish text files; parse the URL field and return
// it as a normal http(s) open, or QString() when the file cannot yield
// a routeable URL.
static QString urlFromLinkFile(const QUrl &fileUrl)
{
    const QString path = fileUrl.toLocalFile();
    if (!path.endsWith(QLatin1String(".webloc"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".url"), Qt::CaseInsensitive)) {
        return {};
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 65536) {
        return {};
    }
    const QString text = QString::fromUtf8(f.readAll());
    // .webloc: <string>URL</string> inside a plist dict keyed "URL".
    // .url:   "URL=..." line (Internet shortcut format).
    static const QRegularExpression re(
        QStringLiteral("(?:<key>URL</key>\\s*<string>|^\\s*URL\\s*=\\s*)([^<\\s][^<\\r\\n]*)"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::MultilineOption);
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

void UrlEventFilter::enqueue(const QUrl &url)
{
    QString scheme = url.scheme().toLower();
    QString routed = url.toString();
    if (scheme == QLatin1String("file")) {
        // Lane claims public.url, so .webloc/.url double-clicks arrive
        // here as file opens. Unwrap them into their embedded link and
        // route that; everything else local (e.g. a stray .html open if
        // some other app points at us) stays ignored.
        const QString embedded = urlFromLinkFile(url);
        if (embedded.isEmpty()) {
            qWarning("Lane: ignoring local file open it cannot route: %s",
                     qPrintable(url.toString()));
            return;
        }
        const QUrl inner(embedded);
        scheme = inner.scheme().toLower();
        routed = embedded;
    }
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        qWarning("Lane: ignoring non-web URL from LaunchServices: %s", qPrintable(scheme));
        return;
    }

    if (m_sink) {
        m_sink(routed);
        return;
    }

    if (m_queue.size() >= kMaxPending) {
        // Drop the newcomer: queued clicks keep their order and nothing
        // the user already triggered gets evicted by a flood.
        qWarning("Lane: dropping a link open; more than %d pending URLs", kMaxPending);
        ++m_dropped;
        return;
    }
    m_queue.push_back(routed);
    qInfo("Lane: queued link open pending controller startup (%d waiting)", pendingCount());
}

} // namespace Lane
