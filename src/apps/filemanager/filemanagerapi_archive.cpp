#include "filemanagerapi.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QSet>
#include <QThread>
#include <QVariant>

#include <thread>

namespace {

static QStringList uniqueExpandedPathsArchive(const QVariantList &paths)
{
    QStringList out;
    QSet<QString> seen;
    for (const QVariant &v : paths) {
        QString p = v.toString().trimmed();
        if (p == QStringLiteral("~")) {
            p = QDir::homePath();
        } else if (p.startsWith(QStringLiteral("~/"))) {
            p = QDir::homePath() + p.mid(1);
        }
        if (p.isEmpty()) {
            continue;
        }
        QFileInfo fi(p);
        const QString abs = fi.absoluteFilePath();
        if (seen.contains(abs)) {
            continue;
        }
        seen.insert(abs);
        out.push_back(abs);
    }
    return out;
}

static QVariantMap runFileRollerExtract(const QString &archive, const QString &dst)
{
    const QString fileRoller = QStandardPaths::findExecutable(QStringLiteral("file-roller"));
    if (fileRoller.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("file-roller-not-found")}
        };
    }

    QProcess p;
    const QStringList args{
        QStringLiteral("--extract-to=%1").arg(dst),
        archive
    };
    p.start(fileRoller, args);
    p.waitForFinished(-1);

    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? QStringLiteral("extract-failed") : err}
        };
    }

    return QVariantMap{
        {QStringLiteral("ok"), true},
        {QStringLiteral("output"), out}
    };
}

static QVariantMap runFileRollerCompress(const QStringList &srcs, const QString &archive)
{
    const QString fileRoller = QStandardPaths::findExecutable(QStringLiteral("file-roller"));
    if (fileRoller.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("file-roller-not-found")}
        };
    }

    QStringList args;
    args << QStringLiteral("--add-to=%1").arg(archive);
    args << srcs;

    QProcess p;
    p.start(fileRoller, args);
    p.waitForFinished(-1);

    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? QStringLiteral("compress-failed") : err}
        };
    }

    return QVariantMap{
        {QStringLiteral("ok"), true}
    };
}

} // namespace

QVariantMap FileManagerApi::extractArchive(const QString &archivePath, const QString &destinationDir) const
{
    const QString archive = expandPath(archivePath);
    if (!QFileInfo::exists(archive)) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    const QString dst = destinationDir.trimmed().isEmpty() ? QFileInfo(archive).absolutePath() : expandPath(destinationDir);
    QDir().mkpath(dst);

    const QVariantMap fileRollerResult = runFileRollerExtract(archive, dst);
    if (!fileRollerResult.value(QStringLiteral("ok")).toBool()) {
        // Backward-compatible fallback for environments without file-roller.
        QProcess p;
        p.start(QStringLiteral("bsdtar"), QStringList{QStringLiteral("-xf"), archive, QStringLiteral("-C"), dst});
        p.waitForFinished(-1);
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            return makeResult(false, err.isEmpty() ? fileRollerResult.value(QStringLiteral("error")).toString() : err);
        }
        return makeResult(true, QString(), {{QStringLiteral("output"), out}, {QStringLiteral("to"), dst}});
    }

    return makeResult(true, QString(),
                      {{QStringLiteral("output"), fileRollerResult.value(QStringLiteral("output")).toString()},
                       {QStringLiteral("to"), dst}});
}

QVariantMap FileManagerApi::compressArchive(const QVariantList &sourcePaths,
                                            const QString &archivePath,
                                            const QString &format) const
{
    Q_UNUSED(format)
    const QString archive = expandPath(archivePath);
    const QStringList srcs = uniqueExpandedPathsArchive(sourcePaths);
    if (archive.isEmpty() || srcs.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }

    const QVariantMap fileRollerResult = runFileRollerCompress(srcs, archive);
    if (!fileRollerResult.value(QStringLiteral("ok")).toBool()) {
        // Backward-compatible fallback for environments without file-roller.
        QStringList args;
        args << QStringLiteral("-cf") << archive;
        for (const QString &p : srcs) {
            args << p;
        }
        QProcess p;
        p.start(QStringLiteral("bsdtar"), args);
        p.waitForFinished(-1);
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
            const QString fallbackErr = fileRollerResult.value(QStringLiteral("error")).toString();
            return makeResult(false, err.isEmpty() ? fallbackErr : err);
        }
        return makeResult(true, QString(), {{QStringLiteral("path"), archive}});
    }

    return makeResult(true, QString(), {{QStringLiteral("path"), archive}});
}

QVariantMap FileManagerApi::startExtractArchive(const QString &archivePath,
                                                const QString &destinationDir)
{
    const QString archive = expandPath(archivePath);
    const QString destination = destinationDir.trimmed();
    if (archive.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    std::thread([this, archive, destination]() {
        const QVariantMap result = extractArchive(archive, destination);
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString to = result.value(QStringLiteral("to")).toString();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, archive, ok, to, error, result]() {
            emit archiveExtractFinished(archive, ok, to, error, result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("archive"), archive},
                       {QStringLiteral("destinationDir"), destination},
                       {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startCompressArchive(const QVariantList &sourcePaths,
                                                 const QString &archivePath,
                                                 const QString &format)
{
    const QString archive = expandPath(archivePath);
    if (archive.isEmpty() || sourcePaths.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    const QVariantList sources = sourcePaths;
    const QString fmt = format;
    std::thread([this, sources, archive, fmt]() {
        const QVariantMap result = compressArchive(sources, archive, fmt);
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, archive, ok, error, result]() {
            emit archiveCompressFinished(archive, ok, error, result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("archive"), archive},
                       {QStringLiteral("async"), true}});
}
