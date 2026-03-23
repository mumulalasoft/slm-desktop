#pragma once

#include <QObject>
#include <QVariantList>

class CaptureService;

class CaptureStreamIngestor : public QObject
{
    Q_OBJECT

public:
    explicit CaptureStreamIngestor(CaptureService *captureService, QObject *parent = nullptr);

private:
    void connectSignals();

private slots:
    void onSessionStreamsChanged(const QString &sessionPath, const QVariantList &streams);
    void onSessionEnded(const QString &sessionPath);
    void onSessionRevoked(const QString &sessionPath, const QString &reason);

private:
    CaptureService *m_captureService = nullptr;
};
