#pragma once

#include "enventry.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>

class LocalEnvStore;

// EnvVariableModel — QAbstractListModel over LocalEnvStore.
// QML accesses entries through the role names below; all mutations
// are delegated to the store (model is read-only from QML's perspective).
class EnvVariableModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        KeyRole      = Qt::UserRole + 1,
        ValueRole,
        EnabledRole,
        CommentRole,
        MergeModeRole,
        ModifiedAtRole,   // ISO 8601 string
        SensitiveRole,    // bool — key is in the risky-var list
        SeverityRole,     // string: "none" | "medium" | "high" | "critical"
    };
    Q_ENUM(Roles)

    explicit EnvVariableModel(LocalEnvStore *store, QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private slots:
    void onStoreChanged();

private:
    LocalEnvStore    *m_store;
    QList<EnvEntry>   m_snapshot; // local copy so beginRemoveRows etc. work correctly
};
