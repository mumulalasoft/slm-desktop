#pragma once

#include <QObject>

class FileManagerApi;
class MetadataIndexServer;

class FileManagerModelFactory : public QObject
{
    Q_OBJECT
public:
    explicit FileManagerModelFactory(FileManagerApi *api,
                                     MetadataIndexServer *indexServer,
                                     QObject *parent = nullptr);

    Q_INVOKABLE QObject *createModel(QObject *parent = nullptr) const;
    Q_INVOKABLE void destroyModel(QObject *model) const;

private:
    FileManagerApi *m_api = nullptr;
    MetadataIndexServer *m_indexServer = nullptr;
};

