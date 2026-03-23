#pragma once

#include "PrintSettingsModel.h"
#include "PrintTicketSerializer.h"

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

class PrintSession : public QObject
{
    Q_OBJECT
public:
    enum class SessionStatus {
        Idle,
        Prepared,
        Submitting,
        Completed,
        Failed,
        Cancelled
    };
    Q_ENUM(SessionStatus)

private:
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString documentUri READ documentUri WRITE setDocumentUri NOTIFY sessionChanged)
    Q_PROPERTY(QString documentTitle READ documentTitle WRITE setDocumentTitle NOTIFY sessionChanged)
    Q_PROPERTY(SessionStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap printerCapability READ printerCapability WRITE setPrinterCapability NOTIFY printerCapabilityChanged)
    Q_PROPERTY(PrintSettingsModel *settingsModel READ settingsModel CONSTANT)

public:
    explicit PrintSession(QObject *parent = nullptr);

    QString sessionId() const { return m_sessionId; }
    QString documentUri() const { return m_documentUri; }
    QString documentTitle() const { return m_documentTitle; }
    SessionStatus status() const { return m_status; }
    QVariantMap printerCapability() const { return m_printerCapabilityRaw; }
    PrintSettingsModel *settingsModel() const { return m_settingsModel; }

    void setDocumentUri(const QString &uri);
    void setDocumentTitle(const QString &title);
    void setStatus(SessionStatus status);
    void setPrinterCapability(const QVariantMap &capability);

    Q_INVOKABLE void begin(const QString &documentUri, const QString &documentTitle);
    Q_INVOKABLE void reset();
    Q_INVOKABLE QVariantMap buildTicketPayload() const;
    Q_INVOKABLE QVariantMap buildIppAttributes() const;
    Q_INVOKABLE QVariantMap buildJobPayload() const;
    Q_INVOKABLE void updateSettingsFromPayload(const QVariantMap &payload);

signals:
    void sessionChanged();
    void statusChanged();
    void printerCapabilityChanged();

private:
    static PrinterCapability parseCapabilityMap(const QVariantMap &capabilityMap);

    QString m_sessionId;
    QString m_documentUri;
    QString m_documentTitle;
    SessionStatus m_status = SessionStatus::Idle;
    QVariantMap m_printerCapabilityRaw;
    PrintSettingsModel *m_settingsModel = nullptr;
};

} // namespace Slm::Print
