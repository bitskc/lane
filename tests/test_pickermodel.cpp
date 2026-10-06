#include "app/PickerModel.h"

#include <QTest>

using namespace Lane;

static Target makeTarget(const QString &id, Kind kind, const QString &name, bool suggested = false)
{
    Target t;
    t.id = id;
    t.kind = kind;
    t.name = name;
    t.browserName = name;
    t.suggested = suggested;
    return t;
}

class PickerModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // rankForPicker() hands the model a list in the user's own order
    // (config.targetOrder, then discovery order). The model must not
    // regroup it: a web app can sit below a browser, a container above.
    void orderPreserved()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("GitHub"), true),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Firefox"))});

        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(m.targetAt(0).id, QStringLiteral("browser:zen:def"));
        QCOMPARE(m.targetAt(1).id, QStringLiteral("pwa:gh"));
        QCOMPARE(m.targetAt(2).id, QStringLiteral("browser:ff:work"));
    }

    // The suggestion travels with its target wherever the user's order
    // put it: SuggestedRole on that row only, and suggestedIndex points
    // at it so Enter and the initial selection open the suggestion.
    void suggestedIndexTracksFlag()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("GitHub"), true),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Firefox"))});

        QCOMPARE(m.suggestedIndex(), 1);
        QCOMPARE(m.data(m.index(0, 0), PickerModel::SuggestedRole).toBool(), false);
        QCOMPARE(m.data(m.index(1, 0), PickerModel::SuggestedRole).toBool(), true);
        QCOMPARE(m.data(m.index(2, 0), PickerModel::SuggestedRole).toBool(), false);
    }

    // Filtering removes rows but never reorders them, and the suggested
    // index follows its target through the filtered list.
    void filterPreservesOrderAndSuggestion()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Work Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("Work GitHub"), true),
                 makeTarget(QStringLiteral("browser:ff:work"), Kind::BrowserProfile, QStringLiteral("Work Firefox"))});
        m.setFilter(QStringLiteral("work"));

        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(m.targetAt(0).id, QStringLiteral("browser:zen:def"));
        QCOMPARE(m.targetAt(1).id, QStringLiteral("pwa:gh"));
        QCOMPARE(m.targetAt(2).id, QStringLiteral("browser:ff:work"));
        QCOMPARE(m.suggestedIndex(), 1);

        m.setFilter(QStringLiteral("work git"));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.targetAt(0).id, QStringLiteral("pwa:gh"));
        QCOMPARE(m.suggestedIndex(), 0);
    }
    void filteredOutSuggestionClearsIndex()
    {
        PickerModel m;
        m.reset({makeTarget(QStringLiteral("browser:zen:def"), Kind::BrowserProfile, QStringLiteral("Zen")),
                 makeTarget(QStringLiteral("pwa:gh"), Kind::Pwa, QStringLiteral("GitHub"), true)});
        m.setFilter(QStringLiteral("zen"));

        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.suggestedIndex(), -1);
    }

    void emptyModel()
    {
        PickerModel m;
        m.reset({});
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.suggestedIndex(), -1);
    }
};

QTEST_MAIN(PickerModelTest)
#include "test_pickermodel.moc"
