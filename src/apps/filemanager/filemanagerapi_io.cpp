#include "filemanagerapi.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

QVariantMap FileManagerApi::readTextFile(const QString &path, int maxBytes) const
{
    const QString p = expandPath(path);
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) {
        return makeResult(false, f.errorString());
    }
    const QByteArray data = f.read(qMax(0, maxBytes));
    return makeResult(true, QString(), {{QStringLiteral("content"), QString::fromUtf8(data)}});
}

QVariantMap FileManagerApi::writeTextFile(const QString &path, const QString &content, bool append)
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    QDir parent = fi.absoluteDir();
    if (!parent.exists()) {
        parent.mkpath(QStringLiteral("."));
    }

    QFile f(p);
    const QIODevice::OpenMode mode = append ? (QIODevice::WriteOnly | QIODevice::Append) : QIODevice::WriteOnly;
    if (!f.open(mode)) {
        return makeResult(false, f.errorString());
    }
    const QByteArray bytes = content.toUtf8();
    if (f.write(bytes) != bytes.size()) {
        return makeResult(false, f.errorString());
    }
    f.close();
    emit pathChanged(p, QStringLiteral("file"));
    emit pathChanged(fi.absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("path"), p}});
}

QVariantMap FileManagerApi::createDirectory(const QString &path, bool recursive)
{
    const QString p = expandPath(path);
    QDir dir;
    const bool ok = recursive ? dir.mkpath(p) : dir.mkdir(p);
    if (!ok && !QFileInfo(p).exists()) {
        return makeResult(false, QStringLiteral("mkdir-failed"));
    }
    emit pathChanged(p, QStringLiteral("directory"));
    emit pathChanged(QFileInfo(p).absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("path"), p}});
}

QVariantMap FileManagerApi::createEmptyFile(const QString &path)
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    QDir parent = fi.absoluteDir();
    if (!parent.exists()) {
        parent.mkpath(QStringLiteral("."));
    }
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly)) {
        return makeResult(false, f.errorString());
    }
    f.close();
    emit pathChanged(p, QStringLiteral("file"));
    emit pathChanged(fi.absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("path"), p}});
}
