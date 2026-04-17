#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "../src/services/context/contextclient.h"

namespace {
constexpr const char kContextService[] = "org.desktop.Context";
constexpr const char kContextPath[] = "/org/desktop/Context";
}

class FakeContextService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Context")

public:
    explicit FakeContextService(QObject *parent = nullptr)
        : QObject(parent)
    {
        setPower(QStringLiteral("ac"), 80, false);
    }

    bool registerOnBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.registerService(QString::fromLatin1(kContextService))) {
            return false;
        }
        if (!bus.registerObject(QString::fromLatin1(kContextPath),
                                this,
                                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
            bus.unregisterService(QString::fromLatin1(kContextService));
            return false;
        }
        return true;
    }

    void unregisterFromBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(kContextPath));
        bus.unregisterService(QString::fromLatin1(kContextService));
    }

    void setPower(const QString &mode, int level, bool lowPower)
    {
        QVariantMap power{
            {QStringLiteral("mode"), mode},
            {QStringLiteral("level"), level},
            {QStringLiteral("lowPower"), lowPower},
        };
        QVariantMap rules{
            {QStringLiteral("actions"), QVariantMap{
                 {QStringLiteral("ui.reduceAnimation"), lowPower},
                 {QStringLiteral("ui.disableBlur"), lowPower},
                 {QStringLiteral("ui.disableHeavyEffects"), lowPower},
             }},
        };
        m_context.insert(QStringLiteral("power"), power);
        m_context.insert(QStringLiteral("rules"), rules);
    }

    int getContextCalls() const
    {
        return m_getContextCalls;
    }

    QVariantMap currentPower() const
    {
        return m_context.value(QStringLiteral("power")).toMap();
    }

public slots:
    QVariantMap GetContext()
    {
        ++m_getContextCalls;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("context"), m_context},
        };
    }

signals:
    void ContextChanged(const QVariantMap &context);
    void PowerChanged(const QVariantMap &power);
    void PowerStateChanged(const QVariantMap &power);

private:
    int m_getContextCalls = 0;
    QVariantMap m_context;
};

class ContextClientTest : public QObject
{
    Q_OBJECT

private slots:
    void power_signal_compatibility_refresh_guard()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip fake service test");
        }

        FakeContextService fake;
        QVERIFY(fake.registerOnBus());

        Slm::Context::ContextClient client;
        QTRY_VERIFY(client.serviceAvailable());
        QTRY_VERIFY(!client.context().isEmpty());

        const int initCalls = fake.getContextCalls();
        QVERIFY(initCalls >= 1);

        // Same-power payload: client should not refresh again.
        const QVariantMap samePower = client.context().value(QStringLiteral("power")).toMap();
        emit fake.PowerChanged(samePower);
        QTest::qWait(60);
        QCOMPARE(fake.getContextCalls(), initCalls);

        // New power payload via canonical signal should trigger refresh.
        fake.setPower(QStringLiteral("battery"), 25, true);
        const QVariantMap nextPower = fake.currentPower();
        emit fake.PowerChanged(nextPower);
        QTRY_VERIFY(fake.getContextCalls() > initCalls);
        QTRY_COMPARE(client.context().value(QStringLiteral("power")).toMap()
                         .value(QStringLiteral("mode")).toString(),
                     QStringLiteral("battery"));
        QTRY_VERIFY(client.reduceAnimation());

        const int afterCanonical = fake.getContextCalls();

        // Legacy signal still supported and should also refresh when stale.
        fake.setPower(QStringLiteral("ac"), 95, false);
        const QVariantMap legacyPower = fake.currentPower();
        emit fake.PowerStateChanged(legacyPower);
        QTRY_VERIFY(fake.getContextCalls() > afterCanonical);
        QTRY_COMPARE(client.context().value(QStringLiteral("power")).toMap()
                         .value(QStringLiteral("mode")).toString(),
                     QStringLiteral("ac"));
        QTRY_VERIFY(!client.reduceAnimation());

        fake.unregisterFromBus();
    }
};

QTEST_GUILESS_MAIN(ContextClientTest)
#include "contextclient_test.moc"
