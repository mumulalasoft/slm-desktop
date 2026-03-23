#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

class ScreenshotSaveHelper : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshotSaveHelper(QObject *parent = nullptr);

    Q_INVOKABLE QString validateFileName(const QString &name) const;
    Q_INVOKABLE QString ensurePngFileName(const QString &name) const;
    Q_INVOKABLE QString ensureFileNameForExtension(const QString &name, const QString &extension) const;
    Q_INVOKABLE QString buildTargetPath(const QString &folder, const QString &name) const;
    Q_INVOKABLE QString buildTargetPathWithExtension(const QString &folder,
                                                     const QString &name,
                                                     const QString &extension) const;
    Q_INVOKABLE bool convertImageFile(const QString &sourcePath,
                                      const QString &targetPath,
                                      const QString &format,
                                      int quality = -1) const;
    Q_INVOKABLE QVariantMap saveImageFile(const QString &sourcePath,
                                          const QString &targetPath,
                                          const QString &format,
                                          int quality = -1,
                                          bool overwrite = false) const;
    Q_INVOKABLE QVariantMap storageInfoForPath(const QString &path) const;
    Q_INVOKABLE bool shouldPromptOverwrite(bool targetExists, bool forceOverwrite) const;
    Q_INVOKABLE QString resolveChosenFolder(const QString &currentFolder,
                                            const QVariantList &selectedPaths,
                                            bool chooserAccepted) const;
    Q_INVOKABLE bool shouldResumeSaveDialog(const QString &sourcePath,
                                            bool chooserAccepted) const;
};
