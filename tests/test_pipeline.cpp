#include "core/pipeline.h"

#include <QTest>

using namespace Tern;

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
};

QTEST_MAIN(PipelineTest)
#include "test_pipeline.moc"
