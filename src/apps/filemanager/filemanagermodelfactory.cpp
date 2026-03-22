#include "filemanagermodelfactory.h"

#include "filemanagermodel.h"
#include "filemanagerapi.h"
#include "metadataindexserver.h"

FileManagerModelFactory::FileManagerModelFactory(FileManagerApi *api,
                                                 MetadataIndexServer *indexServer,
                                                 QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_indexServer(indexServer)
{
}

QObject *FileManagerModelFactory::createModel(QObject *parent) const
{
    QObject *effectiveParent = parent ? parent : const_cast<FileManagerModelFactory *>(this);
    return new FileManagerModel(m_api, m_indexServer, effectiveParent);
}

void FileManagerModelFactory::destroyModel(QObject *model) const
{
    if (!model) {
        return;
    }
    model->deleteLater();
}

