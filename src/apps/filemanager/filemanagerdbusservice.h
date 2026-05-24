#pragma once

#include <QObject>
#include <QVariantMap>

class QQmlApplicationEngine;

class FileManagerDbusService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.FileManager1")

public:
    explicit FileManagerDbusService(QQmlApplicationEngine *engine, QObject *parent = nullptr);
    ~FileManagerDbusService() override;

    bool serviceRegistered() const;

public slots:
    QVariantMap OpenPath(const QString &path, const QString &requestId = QString());
    QVariantMap RevealItem(const QString &path, const QString &requestId = QString());
    QVariantMap NewWindow(const QString &path = QString());

private:
    QObject *resolveFileManagerWindow() const;
    static QString normalizedPath(const QString &path);

    QQmlApplicationEngine *m_engine = nullptr;
    bool m_serviceRegistered = false;
};
