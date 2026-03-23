#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class WorkspacePreviewManager : public QObject
{
    Q_OBJECT

public:
    explicit WorkspacePreviewManager(QObject *parent = nullptr);

    Q_INVOKABLE QString capturePreviewSource(const QString &viewId, int x, int y, int width, int height);

private:
    bool refreshFrameSnapshot();
    QString cachePathForView(const QString &viewId) const;
    static QString sanitizeId(const QString &value);

    QString m_framePath;
    qint64 m_frameCapturedAtMs = 0;
    QImage m_frameImage;
    quint64 m_revision = 0;
};
