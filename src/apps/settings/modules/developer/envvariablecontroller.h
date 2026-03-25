#pragma once

#include "envvariablemodel.h"
#include "localenvstore.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

// EnvVariableController — QML-facing bridge for Settings > Developer > Environment Variables.
// Exposed to QML as the "EnvVarController" context property.
//
// Owns LocalEnvStore and EnvVariableModel for Phase 1.
// In Phase 2 these will be replaced by a D-Bus service client.
class EnvVariableController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EnvVariableModel *model  READ model  CONSTANT)
    Q_PROPERTY(bool    loading          READ loading          NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError        READ lastError        NOTIFY lastErrorChanged)

public:
    explicit EnvVariableController(QObject *parent = nullptr);

    EnvVariableModel *model() const;
    bool     loading()   const;
    QString  lastError() const;

    // Add a new entry. Returns false (and sets lastError) if the key already exists
    // or fails validation.
    Q_INVOKABLE bool addEntry(const QString &key,
                              const QString &value,
                              const QString &comment,
                              const QString &mergeMode = QStringLiteral("replace"));

    // Update value/comment/mergeMode for an existing key.
    Q_INVOKABLE bool updateEntry(const QString &key,
                                 const QString &value,
                                 const QString &comment,
                                 const QString &mergeMode);

    // Remove the entry for the given key.
    Q_INVOKABLE bool deleteEntry(const QString &key);

    // Toggle the enabled flag.
    Q_INVOKABLE bool setEnabled(const QString &key, bool enabled);

    // Returns { valid: bool, severity: string, message: string }
    Q_INVOKABLE QVariantMap validateKey(const QString &key) const;
    Q_INVOKABLE QVariantMap validateValue(const QString &key, const QString &value) const;

    // Convenience — true if the key triggers a warning dialog.
    Q_INVOKABLE bool isSensitiveKey(const QString &key) const;

signals:
    void loadingChanged();
    void lastErrorChanged();
    void entryAdded(const QString &key);
    void entryDeleted(const QString &key);
    void entryUpdated(const QString &key);
    void operationFailed(const QString &error);

private:
    void setLastError(const QString &msg);

    LocalEnvStore    *m_store;
    EnvVariableModel *m_model;
    bool              m_loading   = false;
    QString           m_lastError;
};
