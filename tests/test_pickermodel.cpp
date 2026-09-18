#include "app/PickerModel.h"

#include <QTest>

using namespace Lane;

static Target makeTarget(const QString &id, Kind kind, const QString &name)
{
    Target t;
    t.id = id;
    t.kind = kind;
    t.name = name;
    t.browserName = name;
    return t;
}

class PickerModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // rankForPicker() hands the model an already-ranked list; the top pick
    // must stay row 0 even when its section would sort later, so Enter and
    // digit-1 open the suggestion instead of the first PWA/container row.
    void leaderPinnedAboveSections()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("GitHub")),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Firefox"))});

        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(m.targetAt(0).id, QStringLiteral("browser:zen:def"));
        QCOMPARE(m.data(m.index(0, 0), PickerModel::SuggestedRole).toBool(), true);
        QCOMPARE(m.data(m.index(1, 0), PickerModel::SuggestedRole).toBool(), false);
        // Remaining rows keep fixed section order: Web apps before Browsers.
        QCOMPARE(m.targetAt(1).id, QStringLiteral("pwa:gh"));
        QCOMPARE(m.targetAt(2).id, QStringLiteral("browser:ff:work"));
    }

    // The leader leaves its own bucket: no duplicate row, no empty section
    // header, and its section header is "Suggested" so the list does not
    // render the same section name twice.
    void leaderExcludedFromItsSection()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Firefox")),
                 makeTarget(QStringLiteral("browser:brave:personal"), Kind::BrowserProfile, QStringLiteral("Brave"))});

        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(m.data(m.index(0, 0), PickerModel::SectionRole).toString(), QStringLiteral("Suggested"));
        QCOMPARE(m.data(m.index(1, 0), PickerModel::SectionRole).toString(), QStringLiteral("Browsers"));
        QCOMPARE(m.data(m.index(2, 0), PickerModel::SectionRole).toString(), QStringLiteral("Browsers"));
        QCOMPARE(m.sectionCount(), 2);
    }

    // An Action leader (e.g. a pinned action target) still pins to row 0.
    void actionLeaderStillPinned()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("action:email"), Kind::Action, QStringLiteral("Email link")),
                 makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen"))});

        QCOMPARE(m.targetAt(0).id, QStringLiteral("action:email"));
        QCOMPARE(m.data(m.index(0, 0), PickerModel::SectionRole).toString(), QStringLiteral("Suggested"));
        QCOMPARE(m.sectionCount(), 2);
    }

    // Filtering re-pins whatever survives at the head of the ranked list.
    void filterKeepsLeaderFirst()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("GitHub")),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Firefox"))});
        m.setFilter(QStringLiteral("firefox"));

        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.targetAt(0).id, QStringLiteral("browser:ff:work"));
        QCOMPARE(m.data(m.index(0, 0), PickerModel::SuggestedRole).toBool(), true);
        QCOMPARE(m.sectionCount(), 1);
    }

    void emptyModel()
    {
        PickerModel m;
        m.reset({});
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.sectionCount(), 0);
    }
};

QTEST_MAIN(PickerModelTest)
#include "test_pickermodel.moc"
