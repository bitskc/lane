// The ordering rules for one in-flight click: queueing, deferred commands,
// and abandoning work at quit. ClickGate has no event loop, so these run
// without a Controller. Whether Controller applies the answers correctly
// (the nested unshorten loop, the SIGTERM pipe) is covered by the real-app
// smoke in tasks 6.3, not here.

#include <QTest>

#include "../src/app/ClickGate.h"

using namespace Lane;
using Entry = ClickGate::Entry;

namespace
{
ClickGate::Pending click(const QString &url, const QString &token = {})
{
    ClickGate::Pending p;
    p.url = url;
    p.activationToken = token;
    return p;
}
}

class ClickGateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstClickRunsAndSecondWaits()
    {
        ClickGate gate;
        QCOMPARE(gate.enter(click(QStringLiteral("https://a.example/"))), Entry::Run);
        QVERIFY(gate.inFlight());
        QCOMPARE(gate.enter(click(QStringLiteral("https://b.example/"))), Entry::Queued);
        QCOMPARE(gate.pendingCount(), 1);

        gate.finish();
        QVERIFY(!gate.inFlight());
        ClickGate::Pending next;
        QVERIFY(gate.takeNext(&next));
        QCOMPARE(next.url, QStringLiteral("https://b.example/"));
    }

    void queuedLinksKeepTheirOwnActivationToken()
    {
        ClickGate gate;
        gate.enter(click(QStringLiteral("https://a.example/"), QStringLiteral("tok-a")));
        gate.enter(click(QStringLiteral("https://b.example/"), QStringLiteral("tok-b")));
        gate.enter(click(QStringLiteral("https://c.example/"), QStringLiteral("tok-c")));
        gate.finish();

        ClickGate::Pending next;
        QVERIFY(gate.takeNext(&next));
        QCOMPARE(next.activationToken, QStringLiteral("tok-b"));
        QVERIFY(gate.takeNext(&next));
        QCOMPARE(next.activationToken, QStringLiteral("tok-c"));
        QVERIFY(!gate.takeNext(&next));
    }

    void overflowDropsTheOldestAndReportsIt()
    {
        ClickGate gate;
        gate.enter(click(QStringLiteral("https://first.example/"))); // in flight
        QString dropped;
        for (int i = 0; i < ClickGate::kMaxPending; ++i) {
            gate.enter(click(QStringLiteral("https://q%1.example/").arg(i)), &dropped);
            QVERIFY2(dropped.isEmpty(), "nothing is dropped while the queue has room");
        }
        gate.enter(click(QStringLiteral("https://newest.example/")), &dropped);
        QCOMPARE(dropped, QStringLiteral("https://q0.example/"));
        QCOMPARE(gate.pendingCount(), ClickGate::kMaxPending);

        gate.finish();
        ClickGate::Pending next;
        QVERIFY(gate.takeNext(&next));
        QCOMPARE(next.url, QStringLiteral("https://q1.example/"));
    }

    void settingsWaitsForTheClickThenRuns()
    {
        ClickGate gate;
        QVERIFY2(gate.requestSettings(), "with nothing in flight the command runs now");
        gate.enter(click(QStringLiteral("https://a.example/")));
        QVERIFY2(!gate.requestSettings(), "during a click the command is deferred");
        gate.finish();
        const auto f = gate.takeFollowup();
        QVERIFY(f.settings);
        QVERIFY(!f.rediscover);
        const auto again = gate.takeFollowup();
        QVERIFY2(!again.settings && !again.rediscover, "a follow-up runs once");
    }

    void settingsCoversARediscoverThatArrivedWithIt()
    {
        ClickGate gate;
        gate.enter(click(QStringLiteral("https://a.example/")));
        QVERIFY(!gate.requestRediscover());
        QVERIFY(!gate.requestSettings());
        gate.finish();
        const auto f = gate.takeFollowup();
        QVERIFY(f.settings);
        QVERIFY2(!f.rediscover, "Settings already reloads discovery");
    }

    void rediscoverAloneRunsAfterTheClick()
    {
        ClickGate gate;
        gate.enter(click(QStringLiteral("https://a.example/")));
        QVERIFY(!gate.requestRediscover());
        gate.finish();
        const auto f = gate.takeFollowup();
        QVERIFY(f.rediscover);
        QVERIFY(!f.settings);
    }

    void quitDuringAClickAbandonsEverythingWaiting()
    {
        ClickGate gate;
        gate.enter(click(QStringLiteral("https://a.example/")));
        gate.enter(click(QStringLiteral("https://b.example/")));
        gate.requestSettings();
        gate.requestRediscover();

        gate.quit();
        gate.finish();
        QVERIFY(gate.quitting());
        QCOMPARE(gate.pendingCount(), 0);
        ClickGate::Pending next;
        QVERIFY2(!gate.takeNext(&next), "no queued link runs after quit");
        const auto f = gate.takeFollowup();
        QVERIFY2(!f.settings && !f.rediscover, "no deferred command runs after quit");
    }

    void aClickArrivingAfterQuitNeverStartsAResolve()
    {
        // Nothing is in flight here. Without the quit check a fresh click
        // would enter Run and start a network round trip just to be thrown
        // away; that is the case a check placed after the resolve misses.
        ClickGate gate;
        gate.quit();
        QVERIFY(!gate.inFlight());
        QCOMPARE(gate.enter(click(QStringLiteral("https://late.example/"))), Entry::Quit);
        QVERIFY2(!gate.inFlight(), "a refused click must not look like one in flight");
        QCOMPARE(gate.pendingCount(), 0);
    }

    void commandsAfterQuitAreRefused()
    {
        ClickGate gate;
        gate.quit();
        QVERIFY2(!gate.requestSettings(), "Settings must not open while quitting");
        QVERIFY2(!gate.requestRediscover(), "discovery must not run while quitting");
        const auto f = gate.takeFollowup();
        QVERIFY(!f.settings && !f.rediscover);
    }
};

QTEST_MAIN(ClickGateTest)
#include "test_clickgate.moc"
