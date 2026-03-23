#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../src/core/workspace/windowingbackendmanager.h"
#include "../src/core/workspace/kwinwaylandipcclient.h"

class WindowingBackendManagerTest : public QObject {
    Q_OBJECT

private slots:
    void availableBackends_kwinOnly()
    {
        WindowingBackendManager manager;
        const QStringList backends = manager.availableBackends();
        QCOMPARE(backends.size(), 1);
        QCOMPARE(backends.first(), QStringLiteral("kwin-wayland"));
    }

    void canonicalizeBackend_mapsAllToKwin()
    {
        WindowingBackendManager manager;
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("kwin")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("kde")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("kwin-wayland")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("wlroots")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("slm-desktop")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QStringLiteral("unknown")), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.canonicalizeBackend(QString()), QStringLiteral("kwin-wayland"));
    }

    void backendDefaultsToKwin()
    {
        WindowingBackendManager manager;
        QCOMPARE(manager.backend(), QStringLiteral("kwin-wayland"));
        QCOMPARE(manager.protocolVersion(), -1);
        QCOMPARE(manager.eventSchemaVersion(), -1);
    }

    void capabilityParity_kwinOnly()
    {
        WindowingBackendManager manager;
        const QVariantMap expected =
                WindowingBackendManager::baseCapabilitiesForBackend(QStringLiteral("kwin-wayland"));
        const QVariantMap caps = manager.capabilities();
        const QStringList keys = {
            QStringLiteral("window-list"),
            QStringLiteral("spaces"),
            QStringLiteral("switcher"),
            QStringLiteral("overview"),
            QStringLiteral("progress-overlay"),
            QStringLiteral("command.switcher-next"),
            QStringLiteral("command.switcher-prev"),
            QStringLiteral("command.progress-hide"),
        };
        for (const QString &key : keys) {
            QVERIFY2(expected.contains(key), qPrintable(QStringLiteral("missing expected key: %1").arg(key)));
            QCOMPARE(caps.value(key).toBool(), expected.value(key).toBool());
            QCOMPARE(manager.hasCapability(key), expected.value(key).toBool());
        }
        QVERIFY(!manager.hasCapability(QStringLiteral("unknown-capability")));
    }

    void sendCommand_noCrash()
    {
        WindowingBackendManager manager;
        const bool ok = manager.sendCommand(QStringLiteral("switcher-next"));
        Q_UNUSED(ok)
        QVERIFY(true);
    }

    void eventRelay_kwin()
    {
        WindowingBackendManager manager;
        KWinWaylandIpcClient *kwinIpc = manager.findChild<KWinWaylandIpcClient *>();
        QVERIFY(kwinIpc != nullptr);

        QSignalSpy relaySpy(&manager, SIGNAL(eventReceived(QString,QVariantMap)));
        QVariantMap payload;
        payload.insert(QStringLiteral("backend"), QStringLiteral("kwin-wayland"));
        QVERIFY(QMetaObject::invokeMethod(kwinIpc, "eventReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("command")),
                                          Q_ARG(QVariantMap, payload)));
        QCOMPARE(relaySpy.count(), 1);
    }
};

QTEST_MAIN(WindowingBackendManagerTest)
#include "windowingbackendmanager_test.moc"
