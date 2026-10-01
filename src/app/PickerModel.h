#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Lane
{

class PickerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Row of the target rankForPicker() flagged `suggested` (a matching
    // web app or remembered destination), or -1 when there is none or the
    // current filter hid it. Picker.qml uses it for the initial selection
    // so Enter still opens the suggestion now that row order belongs to
    // the user's targetOrder instead of pinning row 0.
    Q_PROPERTY(int suggestedIndex READ suggestedIndex NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        IconRole,
        KindRole,
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
    int suggestedIndex() const { return m_suggestedIndex; }
    QList<Target> all() const { return m_all; }

Q_SIGNALS:
    void countChanged();

private:
    void applyFilter();

    QList<Target> m_all;
    QList<Target> m_shown;
    QString m_filter;
    int m_suggestedIndex = -1;
};

} // namespace Lane
