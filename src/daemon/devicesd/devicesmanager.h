#pragma once

#include <QObject>
#include <QVariantMap>

class DevicesManager : public QObject
{
    Q_OBJECT

public:
    explicit DevicesManager(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap Mount(const QString &target);
    Q_INVOKABLE QVariantMap Eject(const QString &target);
    Q_INVOKABLE QVariantMap Unlock(const QString &target, const QString &passphrase);
    Q_INVOKABLE QVariantMap Format(const QString &target, const QString &filesystem);

private:
    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QString &stdoutText = QString(),
                                  const QString &stderrText = QString());
    static bool isUriTarget(const QString &target);
    static QVariantMap runCommand(const QString &program,
                                  const QStringList &args,
                                  int timeoutMs = 30000);
};

