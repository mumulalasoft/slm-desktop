#include "envvariablecontroller.h"
#include "envvalidator.h"

EnvVariableController::EnvVariableController(QObject *parent)
    : QObject(parent)
    , m_store(new LocalEnvStore(this))
    , m_model(new EnvVariableModel(m_store, this))
{
    m_store->load();
}

EnvVariableModel *EnvVariableController::model() const  { return m_model; }
bool              EnvVariableController::loading() const { return m_loading; }
QString           EnvVariableController::lastError() const { return m_lastError; }

void EnvVariableController::setLastError(const QString &msg)
{
    if (m_lastError == msg)
        return;
    m_lastError = msg;
    emit lastErrorChanged();
    if (!msg.isEmpty())
        emit operationFailed(msg);
}

bool EnvVariableController::addEntry(const QString &key,
                                     const QString &value,
                                     const QString &comment,
                                     const QString &mergeMode)
{
    const ValidationResult kr = EnvValidator::validateKey(key);
    if (!kr.valid) {
        setLastError(kr.message);
        return false;
    }
    const ValidationResult vr = EnvValidator::validateValue(key, value);
    if (!vr.valid) {
        setLastError(vr.message);
        return false;
    }

    EnvEntry e;
    e.key       = key.trimmed();
    e.value     = value;
    e.comment   = comment;
    e.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;
    e.scope     = QStringLiteral("user");
    e.enabled   = true;

    if (!m_store->addEntry(e)) {
        setLastError(QStringLiteral("A variable named \"%1\" already exists.").arg(key));
        return false;
    }
    setLastError({});
    emit entryAdded(key);
    return true;
}

bool EnvVariableController::updateEntry(const QString &key,
                                        const QString &value,
                                        const QString &comment,
                                        const QString &mergeMode)
{
    const ValidationResult vr = EnvValidator::validateValue(key, value);
    if (!vr.valid) {
        setLastError(vr.message);
        return false;
    }

    EnvEntry updated;
    updated.key       = key;
    updated.value     = value;
    updated.comment   = comment;
    updated.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;

    if (!m_store->updateEntry(key, updated)) {
        setLastError(QStringLiteral("Variable \"%1\" not found.").arg(key));
        return false;
    }
    setLastError({});
    emit entryUpdated(key);
    return true;
}

bool EnvVariableController::deleteEntry(const QString &key)
{
    if (!m_store->removeEntry(key)) {
        setLastError(QStringLiteral("Variable \"%1\" not found.").arg(key));
        return false;
    }
    setLastError({});
    emit entryDeleted(key);
    return true;
}

bool EnvVariableController::setEnabled(const QString &key, bool enabled)
{
    if (!m_store->setEnabled(key, enabled)) {
        setLastError(QStringLiteral("Variable \"%1\" not found.").arg(key));
        return false;
    }
    setLastError({});
    return true;
}

QVariantMap EnvVariableController::validateKey(const QString &key) const
{
    const ValidationResult r = EnvValidator::validateKey(key);
    return {
        {QStringLiteral("valid"),    r.valid},
        {QStringLiteral("severity"), r.severity},
        {QStringLiteral("message"),  r.message},
    };
}

QVariantMap EnvVariableController::validateValue(const QString &key, const QString &value) const
{
    const ValidationResult r = EnvValidator::validateValue(key, value);
    return {
        {QStringLiteral("valid"),    r.valid},
        {QStringLiteral("severity"), r.severity},
        {QStringLiteral("message"),  r.message},
    };
}

bool EnvVariableController::isSensitiveKey(const QString &key) const
{
    return EnvValidator::isSensitiveKey(key);
}
