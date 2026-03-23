#pragma once

#include "PrintTypes.h"

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

class PrintSettingsModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString printerId READ printerId WRITE setPrinterId NOTIFY settingsChanged)
    Q_PROPERTY(int copies READ copies WRITE setCopies NOTIFY settingsChanged)
    Q_PROPERTY(QString pageRange READ pageRange WRITE setPageRange NOTIFY settingsChanged)
    Q_PROPERTY(QString paperSize READ paperSize WRITE setPaperSize NOTIFY settingsChanged)
    Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY settingsChanged)
    Q_PROPERTY(QString duplex READ duplex WRITE setDuplex NOTIFY settingsChanged)
    Q_PROPERTY(QString colorMode READ colorMode WRITE setColorMode NOTIFY settingsChanged)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY settingsChanged)
    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap pluginFeatures READ pluginFeatures WRITE setPluginFeatures NOTIFY settingsChanged)

public:
    explicit PrintSettingsModel(QObject *parent = nullptr);

    QString printerId() const { return m_ticket.printerId; }
    int copies() const { return m_ticket.copies; }
    QString pageRange() const { return m_ticket.pageRange; }
    QString paperSize() const { return m_ticket.paperSize; }
    QString orientation() const { return toString(m_ticket.orientation); }
    QString duplex() const { return toString(m_ticket.duplex); }
    QString colorMode() const { return toString(m_ticket.colorMode); }
    QString quality() const { return m_ticket.quality; }
    double scale() const { return m_ticket.scale; }
    QVariantMap pluginFeatures() const { return m_ticket.pluginFeatures; }

    void setPrinterId(const QString &value);
    void setCopies(int value);
    void setPageRange(const QString &value);
    void setPaperSize(const QString &value);
    void setOrientation(const QString &value);
    void setDuplex(const QString &value);
    void setColorMode(const QString &value);
    void setQuality(const QString &value);
    void setScale(double value);
    void setPluginFeatures(const QVariantMap &value);

    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE QVariantMap serialize() const;
    Q_INVOKABLE void deserialize(const QVariantMap &payload);
    Q_INVOKABLE void applyCapability(const PrinterCapability &capability);

    PrintTicket ticket() const { return m_ticket; }
    void setTicket(const PrintTicket &ticket);

signals:
    void settingsChanged();

private:
    void emitIfChanged(const PrintTicket &before);
    static double clampScale(double value);

    PrintTicket m_ticket;
};

} // namespace Slm::Print
