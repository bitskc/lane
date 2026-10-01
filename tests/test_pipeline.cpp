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

    void cleanLinksStripsTrackedParam()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?utm_source=newsletter&id=5"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?id=5"));
        QCOMPARE(c.removedTrackingParams, QStringList{QStringLiteral("utm_source")});
    }

    void cleanLinksPreservesFragment()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/page?utm_source=x#section"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/page#section"));
    }

    void cleanLinksEmptiedQueryLeavesNoTrailingMark()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/page?utm_source=x&utm_medium=y"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/page"));
    }

    void cleanLinksPreservesBase64PaddingByteForByte()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?token=YWJj==&utm_source=x"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?token=YWJj=="));
    }

    void cleanLinksStripsSiOnYoutube()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://youtube.com/watch?v=abc&si=xyz"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://youtube.com/watch?v=abc"));
    }

    void cleanLinksKeepsSiOffYoutube()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?si=xyz"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?si=xyz"));
    }

    void cleanLinksStripsUppercaseUtm()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?UTM_SOURCE=x&id=5"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?id=5"));
    }

    void cleanLinksStripsSemicolonSeparatedTracker()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?a=1;utm_source=x"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?a=1"));
    }

    void cleanLinksLeavesMailtoUntouched()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("mailto:test@example.com?subject=hi&utm_source=x"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("mailto:test@example.com?subject=hi&utm_source=x"));
        QVERIFY(c.removedTrackingParams.isEmpty());
    }

    void cleanLinksToggleOffLeavesUrlUntouched()
    {
        Config cfg;
        cfg.stripTrackingParams = false;
        const Click c = runPipeline(QStringLiteral("https://example.com/?utm_source=x&id=5"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?utm_source=x&id=5"));
        QVERIFY(c.removedTrackingParams.isEmpty());
    }

    // Regression guards for the review fixes: a query with no tracker must
    // come back byte-for-byte (separators, double-&, trailing '?' kept), a
    // '?' inside the fragment is not the query start, 'ref' is functional
    // not a tracker, and a kept segment keeps the separator that followed
    // the dropped one.

    void cleanLinksNoTrackerLeavesQueryUntouched()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?a=1;b=2"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?a=1;b=2"));
        QVERIFY(c.removedTrackingParams.isEmpty());
    }

    void cleanLinksNoTrackerKeepsDoubleAmpersand()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?a=1&&b=2"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?a=1&&b=2"));
    }

    void cleanLinksIgnoresQueryMarkInsideFragment()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://x/#/route?utm_source=y&ref=a"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://x/#/route?utm_source=y&ref=a"));
        QVERIFY(c.removedTrackingParams.isEmpty());
    }

    void cleanLinksKeepsRefFunctionalParam()
    {
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://github.com/bitskc/tern?ref=main&utm_source=n"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://github.com/bitskc/tern?ref=main"));
        QCOMPARE(c.removedTrackingParams, QStringList{QStringLiteral("utm_source")});
    }

    void cleanLinksKeepsSeparatorAfterDroppedSegment()
    {
        // "a=1;utm=x&b=2": utm is dropped, b's surviving separator is the
        // '&' that ended the dropped segment, so the result is a=1&b=2.
        Config cfg;
        const Click c = runPipeline(QStringLiteral("https://example.com/?a=1;utm_source=x&b=2"), cfg);
        QCOMPARE(c.matchUrl, QStringLiteral("https://example.com/?a=1&b=2"));
    }

    void cleanLinksLeavesOpenUrlUnchangedForWrappedLink()
    {
        // With unwrapping on but openUnwrapped off, openUrl must keep the
        // dirty wrapper even though matchUrl is cleaned.
        Config cfg;
        cfg.openUnwrapped = false;
        const Click c = runPipeline(
            QStringLiteral("https://nam.safelinks.protection.outlook.com/?url=https%3A%2F%2Fexample.com%2F%3Futm_source%3Dx&utm_medium=email"), cfg);
        QVERIFY(c.openUrl.contains(QStringLiteral("utm_medium=email")));
    }
};

QTEST_MAIN(PipelineTest)
#include "test_pipeline.moc"
