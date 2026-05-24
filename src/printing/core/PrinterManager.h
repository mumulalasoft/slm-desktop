#pragma once

#include "PrinterCapabilityProvider.h"

#include <QFutureWatcher>
#include <QObject>
#include <QVariantList>

namespace Slm::Print {

class PrinterManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList printers READ printers NOTIFY printersChanged)
    Q_PROPERTY(QString defaultPrinterId READ defaultPrinterId NOTIFY printersChanged)
    Q_PROPERTY(bool printingAvailable READ printingAvailable NOTIFY printingAvailableChanged)

public:
    explicit PrinterManager(QObject *parent = nullptr);

    QVariantList printers() const { return m_printers; }
    QString defaultPrinterId() const { return m_defaultPrinterId; }
    bool printingAvailable() const { return m_cupsAvailable; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantMap printerById(const QString &printerId) const;
    Q_INVOKABLE QVariantMap capabilities(const QString &printerId) const;

    static QString parseDefaultPrinter(const QString &lpstatDefaultOutput);
    static QVariantList parsePrinterList(const QString &lpstatEOutput,
                                         const QString &lpstatPOutput,
                                         const QString &defaultPrinter);

signals:
    void printersChanged();
    void printingAvailableChanged();

private:
    static bool probeScheduler();
    static QString runCommand(const QString &program, const QStringList &arguments);

    struct ReloadResult {
        bool cupsAvailable = false;
        QString defaultPrinter;
        QVariantList printers;
    };

    PrinterCapabilityProvider m_capabilityProvider;
    QVariantList m_printers;
    QString m_defaultPrinterId;
    bool m_cupsAvailable = true;
    bool m_reloading = false;
    QFutureWatcher<ReloadResult> *m_reloadWatcher = nullptr;
};

} // namespace Slm::Print
