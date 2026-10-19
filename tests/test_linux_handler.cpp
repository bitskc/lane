#include "app/platform/linux_handler_state.h"

#include <QTest>

using namespace Lane;
using Status = Platform::DefaultHandlerState::Status;

class LinuxHandlerStateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bothSchemesLaneIsRegistered()
    {
        const auto state = linuxDefaultHandlerState(true, QStringLiteral("app.lane.Lane.desktop"), true,
                                                    QStringLiteral("app.lane.Lane.desktop"));
        QVERIFY(state.isDefault);
        QCOMPARE(state.status, Status::Registered);
        QCOMPARE(state.currentHandler, QStringLiteral("Lane"));
    }

    void oneSchemeLaneIsPartial()
    {
        const auto state = linuxDefaultHandlerState(true, QStringLiteral("app.lane.Lane.desktop"), true,
                                                    QStringLiteral("firefox.desktop"));
        QVERIFY(!state.isDefault);
        QCOMPARE(state.status, Status::Partial);
        QCOMPARE(state.currentHandler, QStringLiteral("firefox.desktop"));
    }

    void neitherSchemeLaneIsUnknown()
    {
        const auto state = linuxDefaultHandlerState(true, QStringLiteral("firefox.desktop"), true,
                                                    QStringLiteral("firefox.desktop"));
        QVERIFY(!state.isDefault);
        QCOMPARE(state.status, Status::Unknown);
        QCOMPARE(state.currentHandler, QStringLiteral("firefox.desktop"));
    }

    void failedQueryIsNotPartial()
    {
        const auto state = linuxDefaultHandlerState(false, QString(), false, QString());
        QVERIFY(!state.isDefault);
        QCOMPARE(state.status, Status::Unknown);
        QVERIFY(state.currentHandler.isEmpty());

        const auto oneKnown = linuxDefaultHandlerState(true, QStringLiteral("app.lane.Lane.desktop"), false, QString());
        QVERIFY(!oneKnown.isDefault);
        QCOMPARE(oneKnown.status, Status::Unknown);
    }
};

QTEST_MAIN(LinuxHandlerStateTest)
#include "test_linux_handler.moc"
