#pragma once

#include <QObject>
#include <QVariantList>

namespace Slm::Print {

// Provides privileged printer management operations (add, remove, set default,
// device discovery). All operations run asynchronously via QProcess to avoid
// blocking the UI thread.
//
// Register as a context property before loading any Settings QML:
//   engine.rootContext()->setContextProperty("PrinterAdmin", &printerAdminService);
//
// QML guards against "undefined" are not needed — this object is always present
// once registered, though individual operations may fail if the print scheduler
// is not running.
class PrinterAdminService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList discoveredDevices READ discoveredDevices NOTIFY discoveredDevicesChanged)

public:
    explicit PrinterAdminService(QObject *parent = nullptr);

    bool         busy()              const { return m_busy; }
    QVariantList discoveredDevices() const { return m_discoveredDevices; }

    // Removes the named printer queue.
    // Emits printerRemoved(success, printerId, errorMessage).
    Q_INVOKABLE void removePrinter(const QString &printerId);

    // Makes the named printer the system default.
    // Emits defaultPrinterSet(success, printerId, errorMessage).
    Q_INVOKABLE void setDefaultPrinter(const QString &printerId);

    // Adds a new printer queue.
    //   name       — queue name (must be alphanumeric/hyphen, no spaces)
    //   deviceUri  — backend URI, e.g. "ipp://192.168.1.10/ipp/print"
    //   ppd        — PPD file path, driver name, or empty for the generic
    //                IPP-Everywhere driver ("everywhere")
    // Emits printerAdded(success, name, errorMessage).
    Q_INVOKABLE void addPrinter(const QString &name,
                                const QString &deviceUri,
                                const QString &ppd = QString());

    // Probes available device URIs via the print backend.
    // Results arrive asynchronously via discoveredDevicesChanged().
    // Each entry: { uri: string, type: string, info: string }
    Q_INVOKABLE void discoverDevices();

signals:
    void busyChanged();
    void discoveredDevicesChanged();

    void printerAdded(bool success, const QString &name, const QString &error);
    void printerRemoved(bool success, const QString &printerId, const QString &error);
    void defaultPrinterSet(bool success, const QString &printerId, const QString &error);

private:
    void setBusy(bool v);

    bool         m_busy = false;
    QVariantList m_discoveredDevices;
};

} // namespace Slm::Print
