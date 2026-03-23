#include "capturestreamingestor.h"
#include "captureservice.h"

#include <QDBusConnection>

namespace {
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
}

CaptureStreamIngestor::CaptureStreamIngestor(CaptureService *captureService, QObject *parent)
    : QObject(parent)
    , m_captureService(captureService)
{
    connectSignals();
}

void CaptureStreamIngestor::connectSignals()
{
    if (!m_captureService) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    bus.connect(QString::fromLatin1(kScreencastService),
                QString::fromLatin1(kScreencastPath),
                QString::fromLatin1(kScreencastIface),
                QStringLiteral("SessionStreamsChanged"),
                this,
                SLOT(onSessionStreamsChanged(QString,QVariantList)));
    bus.connect(QString::fromLatin1(kScreencastService),
                QString::fromLatin1(kScreencastPath),
                QString::fromLatin1(kScreencastIface),
                QStringLiteral("SessionEnded"),
                this,
                SLOT(onSessionEnded(QString)));
    bus.connect(QString::fromLatin1(kScreencastService),
                QString::fromLatin1(kScreencastPath),
                QString::fromLatin1(kScreencastIface),
                QStringLiteral("SessionRevoked"),
                this,
                SLOT(onSessionRevoked(QString,QString)));
}

void CaptureStreamIngestor::onSessionStreamsChanged(const QString &sessionPath,
                                                    const QVariantList &streams)
{
    if (!m_captureService) {
        return;
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty() || streams.isEmpty()) {
        return;
    }
    m_captureService->SetScreencastSessionStreams(session,
                                                  streams,
                                                  {{QStringLiteral("source"),
                                                    QStringLiteral("screencast-signal")}});
}

void CaptureStreamIngestor::onSessionEnded(const QString &sessionPath)
{
    if (!m_captureService) {
        return;
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    m_captureService->ClearScreencastSession(session);
}

void CaptureStreamIngestor::onSessionRevoked(const QString &sessionPath, const QString &reason)
{
    if (!m_captureService) {
        return;
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    m_captureService->RevokeScreencastSession(session, reason);
}

