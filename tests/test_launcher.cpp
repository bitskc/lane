#include "core/launcher.h"

#include <QTest>

using namespace Tern;

class LauncherTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void expandsUrlPlaceholder()
    {
        Target t;
        t.args = {QStringLiteral("-P"), QStringLiteral("Work"), QStringLiteral("--new-tab"), QStringLiteral("$url")};
        const auto args = expandArgs(t, QStringLiteral("https://example.com"));
        QCOMPARE(args.last(), QStringLiteral("https://example.com"));
        QCOMPARE(args.size(), 4);
    }

    void appendsWhenMissing()
    {
        Target t;
        t.args = {QStringLiteral("--incognito")};
        const auto args = expandArgs(t, QStringLiteral("https://x.test"));
        QCOMPARE(args, QStringList({QStringLiteral("--incognito"), QStringLiteral("https://x.test")}));
    }

    void pwaUrlFlag()
    {
        Target t;
        t.kind = Kind::Pwa;
        t.args = {QStringLiteral("site"), QStringLiteral("launch"), QStringLiteral("ULID"), QStringLiteral("--url"), QStringLiteral("$url")};
        const auto args = expandArgs(t, QStringLiteral("https://claude.ai/new"));
        QCOMPARE(args.last(), QStringLiteral("https://claude.ai/new"));
    }
};

QTEST_MAIN(LauncherTest)
#include "test_launcher.moc"
