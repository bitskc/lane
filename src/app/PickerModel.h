#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Lane
{

class PickerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Number of distinct section headers list.qml will actually render for
    // the currently shown rows (see sectionFor() in PickerModel.cpp: up to
    // five -- Containers, Web apps, Actions, Apps, Browsers). Exists so the
    // picker window can size itself for however many headers are really
    // present instead of a hardcoded guess.
    Q_PROPERTY(int sectionCount READ sectionCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        IconRole,
        KindRole,
        SectionRole,
        ColorRole,
        ShortcutRole,
        SuggestedRole,
        IncognitoRole,
    };

    explicit PickerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset(QList<Target> targets, const QString &filter = {});
    Q_INVOKABLE void setFilter(const QString &filter);
    Target targetAt(int row) const;
    int sectionCount() const { return m_sectionCount; }
    QList<Target> all() const { return m_all; }

Q_SIGNALS:
    void countChanged();

private:
    void applyFilter();

    QList<Target> m_all;
    QList<Target> m_shown;
    QString m_filter;
    int m_sectionCount = 0;
};

} // namespace Lane
