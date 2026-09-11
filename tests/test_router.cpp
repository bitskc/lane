#include "core/router.h"

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

static Target makeAction(const QString &id, const QString &name)
{
    Target t;
    t.id = id;
    t.kind = Kind::Action;
    t.engine = Engine::Action;
    t.name = name;
    t.browserName = QStringLiteral("Lane");
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

    void conflictHonorsFirstRuleWhenPickerNever()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("a"), QStringLiteral("A")),
                              makeBrowser(QStringLiteral("b"), QStringLiteral("B"))};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
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
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("a"));
        QCOMPARE(d.reason, QStringLiteral("rule"));
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

    void pickerHonorsTargetOrder()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true),
                              makeBrowser(QStringLiteral("browser:brave:personal"), QStringLiteral("Personal")),
                              makeBrowser(QStringLiteral("browser:firefox:work"), QStringLiteral("Work"))};
        Config cfg;
        cfg.targetOrder = {QStringLiteral("browser:firefox:work"),
                           QStringLiteral("browser:brave:personal"),
                           QStringLiteral("browser:zen:def")};
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        const auto ranked = rankForPicker(c, targets, cfg);
        QCOMPARE(ranked.size(), 3);
        QCOMPARE(ranked.at(0).id, QStringLiteral("browser:firefox:work"));
        QCOMPARE(ranked.at(1).id, QStringLiteral("browser:brave:personal"));
        QCOMPARE(ranked.at(2).id, QStringLiteral("browser:zen:def"));
    }

    void pickerTargetOrderAfterPwaAndRemembered()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true),
                              makeBrowser(QStringLiteral("browser:brave:personal"), QStringLiteral("Personal")),
                              makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/"))};
        Config cfg;
        cfg.targetOrder = {QStringLiteral("browser:brave:personal"),
                           QStringLiteral("browser:zen:def")};
        cfg.remembered.insert(QStringLiteral("news.ycombinator.com"), QStringLiteral("browser:brave:personal"));
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/x");
        const auto ranked = rankForPicker(c, targets, cfg);
        // PWA first
        QCOMPARE(ranked.at(0).id, QStringLiteral("pwa:gh"));
        // Remembered next
        QCOMPARE(ranked.at(1).id, QStringLiteral("browser:brave:personal"));
        // Then targetOrder
        QCOMPARE(ranked.at(2).id, QStringLiteral("browser:zen:def"));
    }

    // action:copy moved out of the picker list into a footer control
    // (Controller::copyCurrent(), bound to Ctrl+C); it must never appear
    // as a row, whether it would have been reached via the general target
    // list or by explicit targetOrder pinning. Other Kind::Action targets
    // (e.g. action:email) are unaffected by this and still excluded from
    // the default ranking the same way they always were.
    void pickerExcludesCopyAction()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true),
                              makeAction(QStringLiteral("action:copy"), QStringLiteral("Copy link")),
                              makeAction(QStringLiteral("action:email"), QStringLiteral("Email link"))};
        Config cfg;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        const auto ranked = rankForPicker(c, targets, cfg);
        QCOMPARE(ranked.size(), 1);
        QCOMPARE(ranked.first().id, QStringLiteral("browser:zen:def"));
    }

    void pickerExcludesCopyActionEvenWhenPinnedInTargetOrder()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true),
                              makeAction(QStringLiteral("action:copy"), QStringLiteral("Copy link"))};
        Config cfg;
        cfg.targetOrder = {QStringLiteral("action:copy"), QStringLiteral("browser:zen:def")};
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        const auto ranked = rankForPicker(c, targets, cfg);
        QCOMPARE(ranked.size(), 1);
        QCOMPARE(ranked.first().id, QStringLiteral("browser:zen:def"));
    }

    void unsafeUrlNeverAutoLaunches()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        cfg.defaultTargetId = QStringLiteral("browser:zen:def");
        Click c;
        c.matchUrl = QStringLiteral("file:///etc/passwd");
        c.openUrl = QStringLiteral("file:///etc/passwd");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Pick);
        QCOMPARE(d.reason, QStringLiteral("blocked"));
    }

    void danglingRememberedKeysFindsMissingTargetsOnly()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true)};
        Config cfg;
        cfg.remembered.insert(QStringLiteral("news.ycombinator.com"), QStringLiteral("browser:zen:def"));
        cfg.remembered.insert(QStringLiteral("old-site.example"), QStringLiteral("browser:firefox:uninstalled"));
        const QStringList dangling = danglingRememberedKeys(targets, cfg);
        QCOMPARE(dangling.size(), 1);
        QCOMPARE(dangling.first(), QStringLiteral("old-site.example"));
    }
};

QTEST_MAIN(RouterTest)
#include "test_router.moc"
