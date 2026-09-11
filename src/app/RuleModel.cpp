#include "RuleModel.h"
#include "core/config.h"

#include <QUuid>

namespace Lane
{

RuleModel::RuleModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RuleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rules.size();
}

QVariant RuleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rules.size()) {
        return {};
    }
    const auto &r = m_rules.at(index.row());
    switch (role) {
    case IdRole:
        return r.id;
    case PatternRole:
        return r.pattern;
    case ScopeRole:
        return scopeToString(r.scope);
    case LocationRole:
        return locationToString(r.location);
    case RegexRole:
        return r.regex;
    case TargetIdRole:
        return r.targetId;
    case EnabledRole:
        return r.enabled;
    default:
        return {};
    }
}

bool RuleModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_rules.size()) {
        return false;
    }
    auto &r = m_rules[index.row()];
    switch (role) {
    case PatternRole:
        r.pattern = value.toString();
        break;
    case ScopeRole:
        r.scope = scopeFromString(value.toString());
        break;
    case LocationRole:
        r.location = locationFromString(value.toString());
        break;
    case RegexRole:
        r.regex = value.toBool();
        break;
    case TargetIdRole:
        r.targetId = value.toString();
        break;
    case EnabledRole:
        r.enabled = value.toBool();
        break;
    default:
        return false;
    }
    Q_EMIT dataChanged(index, index, {role});
    Q_EMIT rulesChanged();
    return true;
}

Qt::ItemFlags RuleModel::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> RuleModel::roleNames() const
{
    return {
        {IdRole, "ruleId"},
        {PatternRole, "pattern"},
        {ScopeRole, "scope"},
        {LocationRole, "location"},
        {RegexRole, "isRegex"},
        {TargetIdRole, "targetId"},
        {EnabledRole, "enabled"},
    };
}

void RuleModel::setRules(QList<Rule> rules)
{
    beginResetModel();
    m_rules = std::move(rules);
    endResetModel();
}

void RuleModel::addRule(const QString &pattern, const QString &targetId)
{
    beginInsertRows({}, m_rules.size(), m_rules.size());
    Rule r;
    r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.pattern = pattern;
    r.targetId = targetId;
    r.scope = MatchScope::Domain;
    m_rules.append(r);
    endInsertRows();
    Q_EMIT rulesChanged();
}

void RuleModel::removeAt(int row)
{
    if (row < 0 || row >= m_rules.size()) {
        return;
    }
    beginRemoveRows({}, row, row);
    m_rules.removeAt(row);
    endRemoveRows();
    Q_EMIT rulesChanged();
}

void RuleModel::setPattern(int row, const QString &pattern)
{
    setData(index(row, 0), pattern, PatternRole);
}

void RuleModel::setTargetId(int row, const QString &targetId)
{
    setData(index(row, 0), targetId, TargetIdRole);
}

void RuleModel::setEnabledAt(int row, bool enabled)
{
    setData(index(row, 0), enabled, EnabledRole);
}

void RuleModel::setScope(int row, const QString &scope)
{
    setData(index(row, 0), scope, ScopeRole);
}

void RuleModel::setRegex(int row, bool regex)
{
    setData(index(row, 0), regex, RegexRole);
}
} // namespace Lane
