#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Lane
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
        DiscoveredNameRole,
    };
    explicit TargetModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setTargets(QList<Target> targets);
    Q_INVOKABLE QVariantList orderableTargets() const;
    Q_INVOKABLE QVariantList incognitoTargets() const;

private:
    QList<Target> m_targets;
};

} // namespace Lane
