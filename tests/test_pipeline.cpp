#include "core/pipeline.h"

#include <QTest>

using namespace Lane;

class PipelineTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void o365MatchButOpenOriginal()
    {
        Config cfg;
        cfg.unwrapO365 = true;
        cfg.openUnwrapped = false;
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=https%3A%2F%2Fgithub.com%2Faloneguid%2Fbt");
        const Click c = runPipeline(wrapped, cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://github.com/aloneguid/bt"));
        QCOMPARE(c.openUrl, wrapped);
        QCOMPARE(c.host, QStringLiteral("github.com"));
    }

    void o365OpenUnwrapped()
    {
        Config cfg;
        cfg.openUnwrapped = true;
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=https%3A%2F%2Fgithub.com%2Fx");
        const Click c = runPipeline(wrapped, cfg);
        QCOMPARE(c.openUrl, QStringLiteral("https://github.com/x"));
    }

    void substitutions()
    {
        Config cfg;
        cfg.substitutions.push_back({QStringLiteral("google.com"), QStringLiteral("bing.com"), false});
        const Click c = runPipeline(QStringLiteral("https://google.com/search"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://bing.com/search"));
        QCOMPARE(c.openUrl, c.matchUrl);
    }

    void unshortenHook()
    {
        Config cfg;
        cfg.unshorten = true;
        auto fn = [](const QString &) { return QStringLiteral("https://github.com/aloneguid/bt"); };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/47EZHSl"), cfg, fn);
        QCOMPARE(c.matchUrl, QStringLiteral("https://github.com/aloneguid/bt"));
        QCOMPARE(c.openUrl, c.matchUrl);
    }

    void unshortenFollowsChainedRedirects()
    {
        Config cfg;
        cfg.unshorten = true;
        // bit.ly -> tinyurl.com -> the real, non-shortener destination.
        auto fn = [](const QString &url) -> QString {
            if (url.contains(QLatin1String("bit.ly"))) {
                return QStringLiteral("https://tinyurl.com/abc123");
            }
            if (url.contains(QLatin1String("tinyurl.com"))) {
                return QStringLiteral("https://github.com/aloneguid/bt");
            }
            return url;
        };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/47EZHSl"), cfg, fn);
        QCOMPARE(c.matchUrl, QStringLiteral("https://github.com/aloneguid/bt"));
        QCOMPARE(c.openUrl, c.matchUrl);
    }

    void unshortenStopsOnRedirectLoop()
    {
        Config cfg;
        cfg.unshorten = true;
        // bit.ly and tinyurl.com point at each other forever.
        auto fn = [](const QString &url) -> QString {
            if (url.contains(QLatin1String("bit.ly"))) {
                return QStringLiteral("https://tinyurl.com/loop");
            }
            return QStringLiteral("https://bit.ly/loop");
        };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/loop"), cfg, fn);
        // Must terminate (the test itself hanging would fail via timeout) and
        // must not resolve to some private/unsafe address; it just stops
        // wherever the loop was detected, still a shortener host.
        QVERIFY(c.matchUrl == QStringLiteral("https://bit.ly/loop")
                || c.matchUrl == QStringLiteral("https://tinyurl.com/loop"));
    }

    void unshortenCapsHopCount()
    {
        Config cfg;
        cfg.unshorten = true;
        // Every hop yields a brand new bit.ly URL (never repeats, so the
        // visited-set loop guard never fires) to prove the hard hop cap is
        // what stops it, not loop detection.
        int calls = 0;
        auto fn = [&calls](const QString &) -> QString {
            ++calls;
            return QStringLiteral("https://bit.ly/hop-%1").arg(calls);
        };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/hop-0"), cfg, fn);
        QVERIFY(c.matchUrl.startsWith(QStringLiteral("https://bit.ly/hop-")));
        QVERIFY(calls <= 5);
    }
};

QTEST_MAIN(PipelineTest)
#include "test_pipeline.moc"
