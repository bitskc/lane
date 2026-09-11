#pragma once

#include "core/types.h"

#include <QAbstractListModel>

namespace Lane
{

class RuleModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PatternRole,
        ScopeRole,
        LocationRole,
        RegexRole,
        TargetIdRole,
        EnabledRole,
    };
    explicit RuleModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setRules(QList<Rule> rules);
    QList<Rule> rules() const { return m_rules; }
    Q_INVOKABLE void addRule(const QString &pattern, const QString &targetId);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void setPattern(int row, const QString &pattern);
    Q_INVOKABLE void setTargetId(int row, const QString &targetId);
    Q_INVOKABLE void setEnabledAt(int row, bool enabled);
    Q_INVOKABLE void setScope(int row, const QString &scope);
    Q_INVOKABLE void setRegex(int row, bool regex);
Q_SIGNALS:
    void rulesChanged();

private:
    QList<Rule> m_rules;
};

} // namespace Lane
