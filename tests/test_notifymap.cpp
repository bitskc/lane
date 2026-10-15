// NotificationActionMap: bounded capacity, LRU eviction, one-shot take.
// Protects the macOS notification delegate; does not claim the real
// notification center ran the action (that's the native gate).

#include <QTest>

#include "../src/app/NotificationActionMap.h"

using namespace Lane;

class NotificationActionMapTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void takesTheRegisteredActionOnce()
    {
        NotificationActionMap map;
        bool ran = false;
        const QString key = map.add([&ran] { ran = true; });
        QVERIFY(!key.isEmpty());
        QVERIFY(map.take(key));
        QVERIFY(ran);
        // One-shot: a second take must not run it again.
        ran = false;
        QVERIFY(!map.take(key));
        QVERIFY(!ran);
    }

    void unknownKeysReturnFalse()
    {
        NotificationActionMap map;
        QVERIFY(!map.take(QStringLiteral("never-registered")));
        QVERIFY(!map.take({}));
    }

    void capacityIsBounded()
    {
        NotificationActionMap map;
        QStringList keys;
        for (int i = 0; i < NotificationActionMap::kMaxEntries + 4; ++i) {
            keys << map.add([] {});
        }
        QCOMPARE(map.size(), NotificationActionMap::kMaxEntries);
        QCOMPARE(map.evictedCount(), 4);
    }

    void evictionDropsOldest()
    {
        NotificationActionMap map;
        QString oldestKey = map.add([] {});
        for (int i = 0; i < NotificationActionMap::kMaxEntries + 2; ++i) {
            map.add([] {});
        }
        QVERIFY(!map.contains(oldestKey));
        // The two newest entries survive.
        QVERIFY(map.contains(QStringLiteral("act-%1").arg(quint64(NotificationActionMap::kMaxEntries + 1))));
        QVERIFY(map.contains(QStringLiteral("act-%1").arg(quint64(NotificationActionMap::kMaxEntries + 2))));
    }
};

QTEST_MAIN(NotificationActionMapTest)
#include "test_notifymap.moc"
