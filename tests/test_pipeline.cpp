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
        // bit.ly/a -> tinyurl.com/b -> bit.ly/c -> tinyurl.com/b: the loop
        // only closes on the *second* visit to tinyurl.com/b, at hop 3,
        // not on the very first redirect. A naive single-hop
        // implementation (one unshorten() call, then stop, which is what
        // this code did before the hop cap and loop guard existed) would
        // also "stop" after exactly one call and happens to land on this
        // same URL text, so matchUrl alone does not prove a loop guard
        // ran at all; the call count is what actually discriminates "the
        // visited-set guard walked three hops and then caught the
        // repeat" from "the old code only ever made one call".
        int calls = 0;
        auto fn = [&calls](const QString &url) -> QString {
            ++calls;
            if (url == QStringLiteral("https://bit.ly/a")) {
                return QStringLiteral("https://tinyurl.com/b");
            }
            if (url == QStringLiteral("https://tinyurl.com/b")) {
                return QStringLiteral("https://bit.ly/c");
            }
            if (url == QStringLiteral("https://bit.ly/c")) {
                return QStringLiteral("https://tinyurl.com/b");
            }
            return url;
        };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/a"), cfg, fn);
        QCOMPARE(c.matchUrl, QStringLiteral("https://tinyurl.com/b"));
        QCOMPARE(calls, 3);
    }

    void unshortenCapsHopCount()
    {
        Config cfg;
        cfg.unshorten = true;
        // Every hop yields a brand new bit.ly URL (never repeats, so the
        // visited-set loop guard never fires) to prove the hard hop cap
        // (kMaxUnshortenHops = 4 in pipeline.cpp) is what stops it, not
        // loop detection. Pinned to the exact count and exact final URL,
        // not just an upper bound: a single-hop implementation would
        // trivially satisfy "calls <= 5" too, proving nothing about a cap
        // actually existing.
        int calls = 0;
        auto fn = [&calls](const QString &) -> QString {
            ++calls;
            return QStringLiteral("https://bit.ly/hop-%1").arg(calls);
        };
        const Click c = runPipeline(QStringLiteral("https://bit.ly/hop-0"), cfg, fn);
        QCOMPARE(c.matchUrl, QStringLiteral("https://bit.ly/hop-4"));
        QCOMPARE(calls, 4);
    }
};

QTEST_MAIN(PipelineTest)
#include "test_pipeline.moc"
