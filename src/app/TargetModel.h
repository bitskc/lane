#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Tern
{

class TargetModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        IconRole,
        KindRole,
        HiddenRole,
        DefaultRole,
        IncognitoRole,
    };
    explicit TargetModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setTargets(QList<Target> targets);
    QList<Target> targets() const { return m_targets; }
    Q_INVOKABLE QString idAt(int row) const;

private:
    QList<Target> m_targets;
};

} // namespace Tern
