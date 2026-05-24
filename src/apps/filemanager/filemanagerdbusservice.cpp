#include "filemanagerdbusservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QVariantMap>

namespace {
constexpr const char kService[] = "org.slm.Desktop.FileManager1";
constexpr const char kPath[] = "/org/slm/Desktop/FileManager1";
}

FileManagerDbusService::FileManagerDbusService(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    if (!bus.registerObject(QString::fromLatin1(kPath),
                            this,
                            QDBusConnection::ExportAllSlots)) {
        return;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        bus.unregisterObject(QString::fromLatin1(kPath));
        return;
    }
    m_serviceRegistered = true;
}

FileManagerDbusService::~FileManagerDbusService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QString::fromLatin1(kPath));
    bus.unregisterService(QString::fromLatin1(kService));
}

bool FileManagerDbusService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QVariantMap FileManagerDbusService::OpenPath(const QString &path, const QString &requestId)
{
    Q_UNUSED(requestId)
    QObject *window = resolveFileManagerWindow();
    if (!window) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("window-unavailable")}};
    }
    const QString p = normalizedPath(path);
    const bool ok = QMetaObject::invokeMethod(window,
                                               "openPath",
                                               Q_ARG(QString, p));
    return {{QStringLiteral("ok"), ok},
            {QStringLiteral("path"), p},
            {QStringLiteral("error"), ok ? QString() : QStringLiteral("open-invoke-failed")}};
}

QVariantMap FileManagerDbusService::RevealItem(const QString &path, const QString &requestId)
{
    Q_UNUSED(requestId)
    QObject *window = resolveFileManagerWindow();
    if (!window) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("window-unavailable")}};
    }

    const QString p = normalizedPath(path);
    const QFileInfo info(p);
    const QString parentPath = info.isDir() ? info.absoluteFilePath()
                                            : info.absolutePath();

    const bool opened = QMetaObject::invokeMethod(window,
                                                   "openPath",
                                                   Q_ARG(QString, parentPath));
    if (!opened) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("open-parent-failed")},
                {QStringLiteral("path"), p}};
    }

    if (!info.isDir()) {
        QMetaObject::invokeMethod(window,
                                  "selectEntryByPath",
                                  Q_ARG(QString, p));
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("path"), p},
            {QStringLiteral("parent"), parentPath}};
}

QVariantMap FileManagerDbusService::NewWindow(const QString &path)
{
    QStringList args;
    const QString p = normalizedPath(path);
    if (!p.trimmed().isEmpty()) {
        args << p;
    }

    const QString exe = QCoreApplication::applicationFilePath();
    const bool started = QProcess::startDetached(exe, args);
    return {{QStringLiteral("ok"), started},
            {QStringLiteral("path"), p},
            {QStringLiteral("error"), started ? QString() : QStringLiteral("spawn-failed")}};
}

QObject *FileManagerDbusService::resolveFileManagerWindow() const
{
    if (!m_engine) {
        return nullptr;
    }
    const auto roots = m_engine->rootObjects();
    for (QObject *root : roots) {
        if (!root) {
            continue;
        }
        QObject *window = root->findChild<QObject *>(QStringLiteral("fileManagerWindowRoot"));
        if (window) {
            return window;
        }
    }
    return roots.isEmpty() ? nullptr : roots.first();
}

QString FileManagerDbusService::normalizedPath(const QString &path)
{
    QString p = path.trimmed();
    if (p.isEmpty()) {
        return QDir::homePath();
    }
    if (p == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (p.startsWith(QStringLiteral("~/"))) {
        p = QDir::homePath() + p.mid(1);
    }
    if (QFileInfo(p).isRelative()) {
        p = QDir::current().absoluteFilePath(p);
    }
    return p;
}
