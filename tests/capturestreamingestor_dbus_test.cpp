#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/desktopd/captureservice.h"
#include "../src/daemon/desktopd/capturestreamingestor.h"

namespace {
constexpr const char kCaptureService[] = "org.slm.Desktop.Capture";
constexpr const char kCapturePath[] = "/org/slm/Desktop/Capture";
constexpr const char kCaptureIface[] = "org.slm.Desktop.Capture";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
}

class FakeScreencastEmitter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Screencast")

signals:
    void SessionStreamsChanged(const QString &sessionPath, const QVariantList &streams);
    void SessionEnded(const QString &sessionPath);
    void SessionRevoked(const QString &sessionPath, const QString &reason);

public slots:
    void EmitStreams(const QString &sessionPath, const QVariantList &streams)
    {
        emit SessionStreamsChanged(sessionPath, streams);
    }
    void EmitEnded(const QString &sessionPath)
    {
        emit SessionEnded(sessionPath);
    }
    void EmitRevoked(const QString &sessionPath, const QString &reason)
    {
        emit SessionRevoked(sessionPath, reason);
    }
};

class CaptureStreamIngestorDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void ingestStreams_andClearOnEnd();

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();

    bool registerFakeScreencast(FakeScreencastEmitter &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kScreencastService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kScreencastPath));
            m_bus.unregisterService(QString::fromLatin1(kScreencastService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kScreencastService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kScreencastPath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
            m_bus.unregisterService(QString::fromLatin1(kScreencastService));
            return false;
        }
        return true;
    }

    void unregisterFakeScreencast()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kScreencastPath));
        m_bus.unregisterService(QString::fromLatin1(kScreencastService));
    }
};

void CaptureStreamIngestorDbusTest::ingestStreams_andClearOnEnd()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    FakeScreencastEmitter fake;
    if (!registerFakeScreencast(fake)) {
        QSKIP("cannot register fake org.slm.Desktop.Screencast");
    }

    CaptureService captureService;
    QVERIFY(captureService.serviceRegistered());
    CaptureStreamIngestor ingestor(&captureService);
    Q_UNUSED(ingestor);

    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                m_bus);
    QVERIFY(captureIface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/ingest1");
    const QVariantList streams{
        QVariantMap{
            {QStringLiteral("stream_id"), 555},
            {QStringLiteral("node_id"), 777},
            {QStringLiteral("source_type"), QStringLiteral("window")},
            {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
        },
    };

    QDBusInterface screencastIface(QString::fromLatin1(kScreencastService),
                                   QString::fromLatin1(kScreencastPath),
                                   QString::fromLatin1(kScreencastIface),
                                   m_bus);
    QVERIFY(screencastIface.isValid());
    QDBusReply<void> emitReply =
        screencastIface.call(QStringLiteral("EmitStreams"), session, streams);
    QVERIFY(emitReply.isValid());
    QTest::qWait(20);

    QDBusReply<QVariantMap> getReply =
        captureIface.call(QStringLiteral("GetScreencastStreams"),
                          session,
                          QStringLiteral("org.example.App"),
                          QVariantMap{});
    QVERIFY(getReply.isValid());
    const QVariantMap getOut = getReply.value();
    QVERIFY(getOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap getResults = getOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(getResults.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("cache"));
    const QVariantMap first = getResults.value(QStringLiteral("streams")).toList().constFirst().toMap();
    QCOMPARE(first.value(QStringLiteral("stream_id")).toInt(), 555);
    QCOMPARE(first.value(QStringLiteral("node_id")).toInt(), 777);

    QDBusReply<void> endReply =
        screencastIface.call(QStringLiteral("EmitEnded"), session);
    QVERIFY(endReply.isValid());
    QTest::qWait(20);

    QDBusReply<QVariantMap> afterEndReply =
        captureIface.call(QStringLiteral("GetScreencastStreams"),
                          session,
                          QStringLiteral("org.example.App"),
                          QVariantMap{});
    QVERIFY(afterEndReply.isValid());
    const QVariantMap afterEndOut = afterEndReply.value();
    QVERIFY(afterEndOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap afterEndResults = afterEndOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(afterEndResults.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("none"));
    QVERIFY(afterEndResults.value(QStringLiteral("streams")).toList().isEmpty());

    unregisterFakeScreencast();
}

QTEST_MAIN(CaptureStreamIngestorDbusTest)
#include "capturestreamingestor_dbus_test.moc"
