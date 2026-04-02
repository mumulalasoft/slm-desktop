#pragma once

#include <QVariantMap>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

namespace Slm::Archive {

class ArchiveBackend
{
public:
    using ProgressCallback = std::function<void(int)>;

    static QVariantMap listArchive(const QString &archivePath, int maxEntries = 4000);
    static QVariantMap testArchive(const QString &archivePath);
    static QVariantMap extractArchive(const QString &archivePath,
                                      const QString &destinationDir,
                                      const QString &mode,
                                      const QString &conflictPolicy,
                                      std::atomic_bool *cancelFlag = nullptr,
                                      const ProgressCallback &progress = {});
    static QVariantMap compressPaths(const QStringList &paths,
                                     const QString &destinationPath,
                                     const QString &format,
                                     std::atomic_bool *cancelFlag = nullptr,
                                     const ProgressCallback &progress = {});

private:
    static QString normalizeArchiveEntryPath(const QString &rawPath);
    static bool isUnsafeArchiveEntryPath(const QString &relPath);
    static QString safeExtractTargetPath(const QString &destRoot, const QString &relPath);
    static QString uniqueDestinationPath(const QString &basePath);
};

} // namespace Slm::Archive
