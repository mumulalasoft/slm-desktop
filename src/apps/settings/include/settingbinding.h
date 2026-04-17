#pragma once

#include <QObject>
#include <QVariant>

/**
 * @brief Abstract interface for connecting QML controls to system backends (D-Bus, GSettings, etc.)
 */
class SettingBinding : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit SettingBinding(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~SettingBinding() = default;

    virtual QVariant value() const = 0;
    virtual void setValue(const QVariant &newValue) = 0;

    virtual bool isLoading() const { return m_loading; }
    QString lastError() const { return m_lastError; }

signals:
    void valueChanged();
    void loadingChanged();
    void lastErrorChanged();
    void errorOccurred(const QString &message);
    void operationFinished(const QString &operation, bool ok, const QString &message);

protected:
    void setLoading(bool loading) {
        if (m_loading != loading) {
            m_loading = loading;
            emit loadingChanged();
        }
    }
    void clearError()
    {
        if (m_lastError.isEmpty()) {
            return;
        }
        m_lastError.clear();
        emit lastErrorChanged();
    }
    void reportError(const QString &message)
    {
        const QString normalized = message.trimmed().isEmpty()
                                       ? QStringLiteral("operation-failed")
                                       : message.trimmed();
        if (m_lastError != normalized) {
            m_lastError = normalized;
            emit lastErrorChanged();
        }
        emit errorOccurred(normalized);
    }
    void beginOperation()
    {
        clearError();
        setLoading(true);
    }
    void endOperation(const QString &operation, bool ok, const QString &message = QString())
    {
        setLoading(false);
        if (ok) {
            clearError();
            emit operationFinished(operation, true, QString());
            return;
        }
        reportError(message);
        emit operationFinished(operation, false, lastError());
    }

private:
    bool m_loading = false;
    QString m_lastError;
};
