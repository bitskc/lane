#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Tern
{

class PickerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        IconRole,
        KindRole,
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
    QList<Target> all() const { return m_all; }

Q_SIGNALS:
    void countChanged();

private:
    void applyFilter();

    QList<Target> m_all;
    QList<Target> m_shown;
    QString m_filter;
};

} // namespace Tern
