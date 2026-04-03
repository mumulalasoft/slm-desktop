#include "envvariablemodel.h"
#include "envvalidator.h"
#include "localenvstore.h"

EnvVariableModel::EnvVariableModel(LocalEnvStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    connect(m_store, &LocalEnvStore::entriesChanged,
            this, &EnvVariableModel::onStoreChanged);
    m_snapshot = m_store->entries();
}

int EnvVariableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_snapshot.size();
}

QVariant EnvVariableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_snapshot.size())
        return {};

    const EnvEntry &e = m_snapshot.at(index.row());
    switch (role) {
    case KeyRole:        return e.key;
    case ValueRole:      return e.value;
    case EnabledRole:    return e.enabled;
    case CommentRole:    return e.comment;
    case MergeModeRole:  return e.mergeMode;
    case ModifiedAtRole: return e.modifiedAt.isValid()
                              ? e.modifiedAt.toString(Qt::ISODate)
                              : QString{};
    case SensitiveRole:  return EnvValidator::isSensitiveKey(e.key);
    case SeverityRole: {
        const ValidationResult r = EnvValidator::validateKey(e.key);
        return r.severity;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> EnvVariableModel::roleNames() const
{
    return {
        {KeyRole,        "key"},
        {ValueRole,      "value"},
        {EnabledRole,    "enabled"},
        {CommentRole,    "comment"},
        {MergeModeRole,  "mergeMode"},
        {ModifiedAtRole, "modifiedAt"},
        {SensitiveRole,  "sensitive"},
        {SeverityRole,   "severity"},
    };
}

void EnvVariableModel::onStoreChanged()
{
    beginResetModel();
    m_snapshot = m_store->entries();
    endResetModel();
    emit countChanged();
}
