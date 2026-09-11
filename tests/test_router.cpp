#include "core/router.h"

#include <QTest>

using namespace Tern;

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

class RouterTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void ruleWinsOverPwa()
    {
        QList<Target> targets{makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/")),
                              makeBrowser(QStringLiteral("browser:zen:work"), QStringLiteral("Work"), true)};
        Config cfg;
        cfg.preferPwa = true;
        Rule r;
        r.pattern = QStringLiteral("github.com");
        r.scope = MatchScope::Domain;
        r.targetId = QStringLiteral("browser:zen:work");
        cfg.rules = {r};
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/x");
        c.host = QStringLiteral("github.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("browser:zen:work"));
        QCOMPARE(d.reason, QStringLiteral("rule"));
    }

    void pwaAutoOpen()
    {
        QList<Target> targets{makePwa(QStringLiteral("pwa:claude"), QStringLiteral("Claude"), QStringLiteral("https://claude.ai/")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.preferPwa = true;
        cfg.pickerPolicy = PickerPolicy::NoRule;
        Click c;
        c.matchUrl = QStringLiteral("https://claude.ai/chat/1");
        c.host = QStringLiteral("claude.ai");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("pwa:claude"));
        QCOMPARE(d.reason, QStringLiteral("pwa"));
    }

    void rememberedHost()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:brave:personal"), QStringLiteral("Personal")),
                              makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.remembered.insert(QStringLiteral("news.ycombinator.com"), QStringLiteral("browser:brave:personal"));
        Click c;
        c.matchUrl = QStringLiteral("https://news.ycombinator.com/item?id=1");
        c.host = QStringLiteral("news.ycombinator.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.target.id, QStringLiteral("browser:brave:personal"));
        QCOMPARE(d.reason, QStringLiteral("remembered"));
    }

    void pickerWhenNoRule()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.preferPwa = true;
        cfg.pickerPolicy = PickerPolicy::NoRule;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        c.host = QStringLiteral("example.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Pick);
    }

    void neverUsesDefault()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        cfg.defaultTargetId = QStringLiteral("browser:zen:def");
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        c.host = QStringLiteral("example.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.reason, QStringLiteral("default"));
    }

    void forcePicker()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        c.forcePicker = true;
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Pick);
    }

    void conflictPicker()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("a"), QStringLiteral("A")),
                              makeBrowser(QStringLiteral("b"), QStringLiteral("B"))};
        Config cfg;
        Rule r1;
        r1.pattern = QStringLiteral("ex");
        r1.scope = MatchScope::Any;
        r1.targetId = QStringLiteral("a");
        Rule r2 = r1;
        r2.targetId = QStringLiteral("b");
        cfg.rules = {r1, r2};
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Pick);
        QCOMPARE(d.reason, QStringLiteral("conflict"));
    }

    void pickerRanksPwaFirst()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true),
                              makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/"))};
        Config cfg;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/x");
        const auto ranked = rankForPicker(c, targets, cfg);
        QCOMPARE(ranked.first().id, QStringLiteral("pwa:gh"));
    }

    void pickerSkipsIncognito()
    {
        Target priv = makeBrowser(QStringLiteral("browser:zen:def:private"), QStringLiteral("Default (Private)"), false);
        priv.incognito = true;
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true), priv};
        Config cfg;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        const auto ranked = rankForPicker(c, targets, cfg);
        QCOMPARE(ranked.size(), 1);
        QCOMPARE(ranked.first().id, QStringLiteral("browser:zen:def"));
    }
};

QTEST_MAIN(RouterTest)
#include "test_router.moc"
