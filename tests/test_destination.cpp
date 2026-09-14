#include "core/destination.h"
#include "core/router.h"
#include "core/config.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Lane;

static Target makeBrowser(const QString &id, const QString &name, bool def = false)
{
    Target t;
    t.id = id;
    t.kind = Kind::BrowserProfile;
    t.engine = Engine::Gecko;
    t.name = name;
    t.browserName = QStringLiteral("Zen");
    t.isBrowserDefault = def;
    return t;
}

static Target makePwa(const QString &id, const QString &name, const QString &scope)
{
    Target t;
    t.id = id;
    t.kind = Kind::Pwa;
    t.engine = Engine::Pwa;
    t.name = name;
    t.browserName = QStringLiteral("PWA");
    t.pwaScope = scope;
    return t;
}

class DestinationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void ladderIncludesHostAndPath()
    {
        const QStringList ladder = destinationLadder(QStringLiteral("https://github.com/bitskc/lane"));
        QCOMPARE(ladder.size(), 3);
        QCOMPARE(ladder[0], QStringLiteral("github.com"));
        QCOMPARE(ladder[1], QStringLiteral("github.com/bitskc"));
        QCOMPARE(ladder[2], QStringLiteral("github.com/bitskc/lane"));
    }

    void ladderEmptyForNoHost()
    {
        const QStringList ladder = destinationLadder(QStringLiteral(""));
        QVERIFY(ladder.isEmpty());
    }

    void lookupRememberedMatchesExactKey()
    {
        QMap<QString, QString> remembered;
        remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        QCOMPARE(lookupRemembered(QStringLiteral("https://github.com/bitskc/lane"), remembered),
                 QStringLiteral("browser:brave"));
    }

    void lookupRememberedDoesNotStealSibling()
    {
        QMap<QString, QString> remembered;
        remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        QVERIFY(lookupRemembered(QStringLiteral("https://github.com/other"), remembered).isEmpty());
    }

    void lookupRememberedPicksLongestMatch()
    {
        QMap<QString, QString> remembered;
        remembered.insert(QStringLiteral("github.com"), QStringLiteral("browser:zen"));
        remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        QCOMPARE(lookupRemembered(QStringLiteral("https://github.com/bitskc/lane"), remembered),
                 QStringLiteral("browser:brave"));
    }

    void hostOnlyMemoryMatchesWithQuery()
    {
        QMap<QString, QString> remembered;
        remembered.insert(QStringLiteral("news.ycombinator.com"), QStringLiteral("browser:brave:personal"));
        QCOMPARE(lookupRemembered(QStringLiteral("https://news.ycombinator.com/item?id=1"), remembered),
                 QStringLiteral("browser:brave:personal"));
    }

    void pwaOriginWideAutoOpensChromePaths()
    {
        const Target pwa = makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/"));
        QVERIFY(pwaShouldAutoOpen(pwa, QStringLiteral("https://github.com/login")));
        QVERIFY(pwaShouldAutoOpen(pwa, QStringLiteral("https://github.com/settings")));
        QVERIFY(pwaShouldAutoOpen(pwa, QStringLiteral("https://github.com/")));
    }

    void pwaOriginWideDoesNotAutoOpenTenantPath()
    {
        const Target pwa = makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/"));
        QVERIFY(!pwaShouldAutoOpen(pwa, QStringLiteral("https://github.com/bitskc/lane")));
        QVERIFY(!pwaShouldAutoOpen(pwa, QStringLiteral("https://github.com/bitskc")));
    }

    void pwaScopedAutoOpensEverything()
    {
        const Target pwa = makePwa(QStringLiteral("pwa:claude"), QStringLiteral("Claude"), QStringLiteral("https://claude.ai/"));
        QVERIFY(pwaShouldAutoOpen(pwa, QStringLiteral("https://claude.ai/chat/1")));
        QVERIFY(pwaShouldAutoOpen(pwa, QStringLiteral("https://claude.ai/")));
    }

    void routerRememberedPathScoped()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:brave"), QStringLiteral("Brave")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/bitskc/lane");
        c.host = QStringLiteral("github.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("browser:brave"));
        QCOMPARE(d.reason, QStringLiteral("remembered"));
    }

    void routerRememberedDoesNotStealSibling()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:brave"), QStringLiteral("Brave")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        cfg.pickerPolicy = PickerPolicy::NoRule;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/octocat/Hello-World");
        c.host = QStringLiteral("github.com");
        const Decision d = route(c, targets, cfg);
        // Should NOT use the bitskc memory; should pick (NoRule policy)
        QCOMPARE(d.action, Decision::Action::Pick);
    }

    void routerPwaOriginWideDoesNotAutoOpenTenant()
    {
        QList<Target> targets{makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.preferPwa = true;
        cfg.pickerPolicy = PickerPolicy::NoRule;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/bitskc/lane");
        c.host = QStringLiteral("github.com");
        const Decision d = route(c, targets, cfg);
        // Origin-wide PWA must not auto-open a tenant path; should pick
        QCOMPARE(d.action, Decision::Action::Pick);
    }

    void routerPwaOriginWideAutoOpensChromePath()
    {
        QList<Target> targets{makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.preferPwa = true;
        cfg.pickerPolicy = PickerPolicy::NoRule;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/login");
        c.host = QStringLiteral("github.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("pwa:gh"));
        QCOMPARE(d.reason, QStringLiteral("pwa"));
    }

    void suggestedLadderIndexTenantPathDefaultsToPathScope()
    {
        const int idx = suggestedLadderIndex(QStringLiteral("https://github.com/bitskc/lane"), nullptr, {});
        QCOMPARE(idx, 1);
        const QStringList ladder = destinationLadder(QStringLiteral("https://github.com/bitskc/lane"));
        QCOMPARE(ladder.value(idx), QStringLiteral("github.com/bitskc"));
    }

    void suggestedLadderIndexChromePathStaysHost()
    {
        QCOMPARE(suggestedLadderIndex(QStringLiteral("https://news.ycombinator.com/item?id=1"), nullptr, {}), 0);
    }

    void suggestedLadderIndexRootPathStaysHost()
    {
        QCOMPARE(suggestedLadderIndex(QStringLiteral("https://example.com/"), nullptr, {}), 0);
    }

    void suggestedLadderIndexLoginIsChrome()
    {
        QCOMPARE(suggestedLadderIndex(QStringLiteral("https://github.com/login"), nullptr, {}), 0);
    }

    void configRoundTripHoldAndPathKey()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        Config c;
        c.holdAutoOpen = false;
        c.holdMs = 3000;
        c.remembered.insert(QStringLiteral("github.com/bitskc"), QStringLiteral("browser:brave"));
        QVERIFY(saveConfig(path, c));
        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.holdAutoOpen, false);
        QCOMPARE(loaded.holdMs, 3000);
        QCOMPARE(loaded.remembered.value(QStringLiteral("github.com/bitskc")), QStringLiteral("browser:brave"));
    }

    void configRoundTripHoldDefaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        Config c;
        QVERIFY(saveConfig(path, c));
        const Config loaded = loadConfig(path);
        // holdAutoOpen defaults to off: DESIGN.md promises "either nothing
        // visible" for a correct open, the pause is opt-in via Preferences.
        QCOMPARE(loaded.holdAutoOpen, false);
        QCOMPARE(loaded.holdMs, 1600);
    }
};

QTEST_MAIN(DestinationTest)
#include "test_destination.moc"
