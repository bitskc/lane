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

static Target makeChromiumBrowser(const QString &id, const QString &name, const QString &brand = QStringLiteral("Brave"))
{
    Target t;
    t.id = id;
    t.kind = Kind::BrowserProfile;
    t.engine = Engine::Chromium;
    t.name = name;
    t.browserName = brand;
    return t;
}

static Target makeContainer(const QString &baseId, int containerId, const QString &name)
{
    Target t;
    t.id = baseId + QStringLiteral(":container:") + QString::number(containerId);
    t.kind = Kind::Container;
    t.engine = Engine::Gecko;
    t.name = name;
    t.browserName = QStringLiteral("Zen");
    t.containerId = containerId;
    t.containerName = name;
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

    // Controller::pruneStaleRemembered() (Controller.cpp) is coupled to
    // Qt's event loop and QML window lifecycle, so it is not practical to
    // instantiate a full Controller here; this instead covers the
    // extracted grace-period decision it is built on. A remembered host
    // whose target is missing from one discovery pass must not be treated
    // as gone for good (a transient discovery false negative - Flatpak
    // export dir mid-update, a bare systemd unit with no
    // XDG_DATA_DIRS - looks identical to the target actually being
    // uninstalled on any single pass); only a miss count that has reached
    // the grace-period threshold should be pruned.
    void shouldPruneRememberedRequiresConsecutiveMisses()
    {
        QVERIFY(!shouldPruneRemembered(1, 3));
        QVERIFY(!shouldPruneRemembered(2, 3));
        QVERIFY(shouldPruneRemembered(3, 3));
        QVERIFY(shouldPruneRemembered(4, 3));
    }

    void privateCounterpartGeckoMatchesPrivateSibling()
    {
        Target def = makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true);
        Target priv = makeBrowser(QStringLiteral("browser:zen:def:private"), QStringLiteral("Default (Private)"));
        priv.incognito = true;
        QList<Target> targets{def, priv};
        const Target result = privateCounterpart(targets, def);
        QCOMPARE(result.id, priv.id);
    }

    void privateCounterpartChromiumMatchesIncognitoSibling()
    {
        Target def = makeChromiumBrowser(QStringLiteral("browser:brave:Default"), QStringLiteral("Default"));
        Target inc = makeChromiumBrowser(QStringLiteral("browser:brave:Default:incognito"), QStringLiteral("Default (Incognito)"));
        inc.incognito = true;
        QList<Target> targets{def, inc};
        const Target result = privateCounterpart(targets, def);
        QCOMPARE(result.id, inc.id);
    }

    // Brave ships a separate Tor profile alongside the regular incognito
    // sibling; Alt+P on the normal Brave profile must land on
    // ":incognito", never accidentally on ":tor" just because it is also
    // present and also marked incognito.
    void privateCounterpartBraveDoesNotMatchTorSibling()
    {
        Target def = makeChromiumBrowser(QStringLiteral("browser:brave:Default"), QStringLiteral("Default"));
        Target tor = makeChromiumBrowser(QStringLiteral("browser:brave:tor"), QStringLiteral("Tor"));
        tor.incognito = true;
        QList<Target> targets{def, tor};
        const Target result = privateCounterpart(targets, def);
        QVERIFY(result.id.isEmpty());
    }

    void privateCounterpartContainerStripsToBaseAndFindsPrivate()
    {
        Target base = makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true);
        Target container = makeContainer(QStringLiteral("browser:zen:def"), 1, QStringLiteral("Work"));
        Target priv = makeBrowser(QStringLiteral("browser:zen:def:private"), QStringLiteral("Default (Private)"));
        priv.incognito = true;
        QList<Target> targets{base, container, priv};
        const Target result = privateCounterpart(targets, container);
        QCOMPARE(result.id, priv.id);
    }

    void privateCounterpartAlreadyIncognitoReturnsSelf()
    {
        Target priv = makeBrowser(QStringLiteral("browser:zen:def:private"), QStringLiteral("Default (Private)"));
        priv.incognito = true;
        QList<Target> targets{priv};
        const Target result = privateCounterpart(targets, priv);
        QCOMPARE(result.id, priv.id);
    }

    void privateCounterpartFailsClosedForGenericPwaAndAction()
    {
        Target pwa = makePwa(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"), QStringLiteral("https://github.com/"));
        Target action = makeAction(QStringLiteral("action:email"), QStringLiteral("Email link"));
        Target custom;
        custom.id = QStringLiteral("custom:script");
        custom.kind = Kind::Custom;
        custom.engine = Engine::Generic;
        custom.name = QStringLiteral("Script");
        QList<Target> targets{pwa, action, custom};
        QVERIFY(privateCounterpart(targets, pwa).id.isEmpty());
        QVERIFY(privateCounterpart(targets, action).id.isEmpty());
        QVERIFY(privateCounterpart(targets, custom).id.isEmpty());
    }

    void privateCounterpartNoSiblingFailsClosed()
    {
        Target lone = makeBrowser(QStringLiteral("browser:zen:def"), QStringLiteral("Default"), true);
        QList<Target> targets{lone};
        QVERIFY(privateCounterpart(targets, lone).id.isEmpty());
    }

    void activityDefaultWinsOverGlobalDefault()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("browser:zen:global"), QStringLiteral("Global"), true),
                              makeBrowser(QStringLiteral("browser:brave:work"), QStringLiteral("Work"))};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        cfg.defaultTargetId = QStringLiteral("browser:zen:global");
        cfg.activityDefaults.insert(QStringLiteral("act-work"), QStringLiteral("browser:brave:work"));

        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        // No Activity context: the global default still applies.
        const Decision d0 = route(c, targets, cfg);
        QCOMPARE(d0.action, Decision::Action::Launch);
        QCOMPARE(d0.target.id, QStringLiteral("browser:zen:global"));

        // Under the configured Activity the per-Activity fallback wins.
        c.activityId = QStringLiteral("act-work");
        const Decision d1 = route(c, targets, cfg);
        QCOMPARE(d1.action, Decision::Action::Launch);
        QCOMPARE(d1.target.id, QStringLiteral("browser:brave:work"));
        QCOMPARE(d1.reason, QStringLiteral("default"));

        // An Activity with no entry in activityDefaults falls back to
        // the global default.
        c.activityId = QStringLiteral("act-fun");
        const Decision d2 = route(c, targets, cfg);
        QCOMPARE(d2.target.id, QStringLiteral("browser:zen:global"));
    }

    void activityScopedRuleWinsOverUnscoped()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("a"), QStringLiteral("A")),
                              makeBrowser(QStringLiteral("b"), QStringLiteral("B"))};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        Rule general;
        general.pattern = QStringLiteral("ex");
        general.scope = MatchScope::Any;
        general.targetId = QStringLiteral("a");
        Rule scoped = general;
        scoped.targetId = QStringLiteral("b");
        scoped.activity = QStringLiteral("act-work");
        cfg.rules = {general, scoped};

        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        c.activityId = QStringLiteral("act-work");
        const Decision d = route(c, targets, cfg);
        QCOMPARE(d.action, Decision::Action::Launch);
        QCOMPARE(d.target.id, QStringLiteral("b"));
        QCOMPARE(d.reason, QStringLiteral("rule"));
    }

    void activityScopedRuleFallsThroughElsewhere()
    {
        QList<Target> targets{makeBrowser(QStringLiteral("a"), QStringLiteral("A")),
                              makeBrowser(QStringLiteral("b"), QStringLiteral("B"))};
        Config cfg;
        cfg.pickerPolicy = PickerPolicy::Never;
        Rule general;
        general.pattern = QStringLiteral("ex");
        general.scope = MatchScope::Any;
        general.targetId = QStringLiteral("a");
        Rule scoped = general;
        scoped.targetId = QStringLiteral("b");
        scoped.activity = QStringLiteral("act-work");
        cfg.rules = {general, scoped};

        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        // A different Activity: the scoped rule is gated out and the
        // unscoped rule applies.
        c.activityId = QStringLiteral("act-fun");
        const Decision d1 = route(c, targets, cfg);
        QCOMPARE(d1.target.id, QStringLiteral("a"));

        // No Activity at all: same outcome, matching pre-feature Lane.
        c.activityId.clear();
        const Decision d2 = route(c, targets, cfg);
        QCOMPARE(d2.target.id, QStringLiteral("a"));
    }
};

QTEST_MAIN(RouterTest)
#include "test_router.moc"
