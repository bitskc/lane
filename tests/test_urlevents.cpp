// UrlEventFilter correctness: FIFO order, bound, scheme filter, drain
// timing. This is a core-level test using a fake event source; whether
// LaunchServices actually posts QFileOpenEvent is proven by the native
// gate (`open -b app.lane.Lane <url>`), not here.

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFileOpenEvent>
#include <QStringList>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <sys/stat.h>

#include "../src/app/UrlEventFilter.h"

using namespace Lane;

namespace
{

void post(UrlEventFilter &filter, const QString &url)
{
    // The QUrl ctor is required: the QString overload is the file-path
    // form and its url() would carry a file: scheme the filter drops.
    QFileOpenEvent event(QUrl{url});
    // eventFilter consumes FileOpen events; it must claim to have
    // handled each one or Qt would warn about an unhandled file open.
    QVERIFY(filter.eventFilter(qApp, &event));
}

}

class UrlEventFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void queuedUrlsDrainInOrder()
    {
        UrlEventFilter filter;
        post(filter, QStringLiteral("https://a.example/1"));
        post(filter, QStringLiteral("https://b.example/2"));
        QCOMPARE(filter.pendingCount(), 2);

        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        QCOMPARE(delivered, QStringList({QStringLiteral("https://a.example/1"),
                                         QStringLiteral("https://b.example/2")}));
        QCOMPARE(filter.pendingCount(), 0);
    }

    void nonWebSchemesAreIgnored()
    {
        UrlEventFilter filter;
        post(filter, QStringLiteral("file:///etc/passwd"));
        post(filter, QStringLiteral("mailto:a@b"));
        post(filter, QStringLiteral("https://ok.example/"));
        QCOMPARE(filter.pendingCount(), 1);

        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        QCOMPARE(delivered, QStringList({QStringLiteral("https://ok.example/")}));
    }

    void queueIsBoundedAtEight()
    {
        UrlEventFilter filter;
        for (int i = 0; i < 12; ++i) {
            post(filter, QStringLiteral("https://x.example/%1").arg(i));
        }
        QCOMPARE(filter.pendingCount(), UrlEventFilter::kMaxPending);
        QCOMPARE(filter.droppedCount(), 4);

        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        QCOMPARE(delivered.size(), UrlEventFilter::kMaxPending);
        // FIFO: the first eight queued survive; the flood victims are
        // the newcomers, not the earliest clicks.
        QCOMPARE(delivered.first(), QStringLiteral("https://x.example/0"));
        QCOMPARE(delivered.last(), QStringLiteral("https://x.example/7"));
    }

    void urlsDispatchImmediatelyOnceSinkSet()
    {
        UrlEventFilter filter;
        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        post(filter, QStringLiteral("https://live.example/"));
        QCOMPARE(delivered, QStringList({QStringLiteral("https://live.example/")}));
        QCOMPARE(filter.pendingCount(), 0);
    }

    void linkFilesUnwrapToTheirEmbeddedUrl()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString webloc = tmp.filePath(QStringLiteral("Page.webloc"));
        const QString shortcut = tmp.filePath(QStringLiteral("Page.url"));
        const QString html = tmp.filePath(QStringLiteral("Page.html"));
        QFile weblocFile(webloc);
        QVERIFY(weblocFile.open(QIODevice::WriteOnly));
        weblocFile.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<plist version=\"1.0\"><dict><key>URL</key>"
                         "<string>https://example.com/from-webloc</string></dict></plist>\n");
        weblocFile.close();
        QFile shortcutFile(shortcut);
        QVERIFY(shortcutFile.open(QIODevice::WriteOnly));
        shortcutFile.write("[InternetShortcut]\nURL=https://example.com/from-url\n");
        shortcutFile.close();
        QFile htmlFile(html);
        QVERIFY(htmlFile.open(QIODevice::WriteOnly));
        htmlFile.write("<html></html>\n");
        htmlFile.close();

        UrlEventFilter filter;
        post(filter, QUrl::fromLocalFile(webloc).toString());
        post(filter, QUrl::fromLocalFile(shortcut).toString());
        post(filter, QUrl::fromLocalFile(html).toString());
        QCOMPARE(filter.pendingCount(), 2);

        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        QCOMPARE(delivered, QStringList({QStringLiteral("https://example.com/from-webloc"),
                                         QStringLiteral("https://example.com/from-url")}));
    }

    void weblocIgnoresDecoyUrlLine()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString webloc = tmp.filePath(QStringLiteral("Page.webloc"));
        QFile file(webloc);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<plist version=\"1.0\"><dict>"
                   "<key>Note</key><string>URL=https://evil.example/phish</string>"
                   "<key>URL</key><string>https://bank.example/real?a=1&amp;b=2</string>"
                   "</dict></plist>\n");
        file.close();

        UrlEventFilter filter;
        post(filter, QUrl::fromLocalFile(webloc).toString());
        QStringList delivered;
        filter.setSink([&delivered](const QString &url) { delivered << url; });
        QCOMPARE(delivered, QStringList({QStringLiteral("https://bank.example/real?a=1&b=2")}));
    }

    void deviceAndFifoLinkFilesDoNotBlock()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString fifo = tmp.filePath(QStringLiteral("fifo.webloc"));
        QVERIFY(mkfifo(fifo.toLocal8Bit().constData(), 0600) == 0);
        const QString zero = tmp.filePath(QStringLiteral("zero.url"));
        QVERIFY(QFile::link(QStringLiteral("/dev/zero"), zero));

        UrlEventFilter filter;
        QElapsedTimer timer;
        timer.start();
        post(filter, QUrl::fromLocalFile(fifo).toString());
        post(filter, QUrl::fromLocalFile(zero).toString());
        QVERIFY(timer.elapsed() < 1000);
        QCOMPARE(filter.pendingCount(), 0);
    }
};

QTEST_MAIN(UrlEventFilterTest)
#include "test_urlevents.moc"
