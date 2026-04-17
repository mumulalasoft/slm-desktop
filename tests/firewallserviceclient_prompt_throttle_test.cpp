#include <QtTest/QtTest>

#include "../src/apps/settings/modules/network/firewallserviceclient.h"

#include <QMetaObject>

class FirewallServiceClientPromptThrottleTest : public QObject
{
    Q_OBJECT

private:
    static QVariantMap makePrompt(const QString &appId,
                                  const QString &direction,
                                  const QString &ip,
                                  int port)
    {
        QVariantMap request{
            {QStringLiteral("direction"), direction},
            {QStringLiteral("destinationIp"), ip},
            {QStringLiteral("destinationPort"), port},
        };
        QVariantMap evaluation{
            {QStringLiteral("direction"), direction},
            {QStringLiteral("source"), QStringLiteral("policy")},
            {QStringLiteral("actor"), QVariantMap{
                 {QStringLiteral("appName"), QStringLiteral("Test App")},
                 {QStringLiteral("executable"), QStringLiteral("/usr/bin/test-app")},
            }},
            {QStringLiteral("identity"), QVariantMap{
                 {QStringLiteral("app_id"), appId},
            }},
            {QStringLiteral("target"), QVariantMap{
                 {QStringLiteral("ip"), ip},
                 {QStringLiteral("port"), port},
            }},
        };
        return QVariantMap{
            {QStringLiteral("request"), request},
            {QStringLiteral("evaluation"), evaluation},
        };
    }

    static int findPromptIndexByIp(const QVariantList &rows, const QString &ip)
    {
        for (int i = 0; i < rows.size(); ++i) {
            const QVariantMap row = rows.at(i).toMap();
            const QVariantMap request = row.value(QStringLiteral("request")).toMap();
            if (request.value(QStringLiteral("destinationIp")).toString() == ip) {
                return i;
            }
        }
        return -1;
    }

private slots:
    void duplicateBurstSuppressedByThrottle()
    {
        FirewallServiceClient client;

        const QVariantMap first = makePrompt(QStringLiteral("cli.curl"),
                                             QStringLiteral("outgoing"),
                                             QStringLiteral("203.0.113.10"),
                                             443);
        QVERIFY(QMetaObject::invokeMethod(&client,
                                          "onConnectionPromptRequested",
                                          Qt::DirectConnection,
                                          Q_ARG(QVariantMap, first)));

        QVariantList rows = client.pendingPrompts();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("duplicateCount")).toInt(), 0);

        QVERIFY(QMetaObject::invokeMethod(&client,
                                          "onConnectionPromptRequested",
                                          Qt::DirectConnection,
                                          Q_ARG(QVariantMap, first)));

        rows = client.pendingPrompts();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("duplicateCount")).toInt(), 1);
    }

    void throttleKeyIncludesDirectionAndEndpoint()
    {
        FirewallServiceClient client;

        const QVariantMap outgoingA = makePrompt(QStringLiteral("cli.curl"),
                                                 QStringLiteral("outgoing"),
                                                 QStringLiteral("203.0.113.10"),
                                                 443);
        const QVariantMap outgoingB = makePrompt(QStringLiteral("cli.curl"),
                                                 QStringLiteral("outgoing"),
                                                 QStringLiteral("203.0.113.11"),
                                                 443);
        const QVariantMap incomingA = makePrompt(QStringLiteral("cli.curl"),
                                                 QStringLiteral("incoming"),
                                                 QStringLiteral("203.0.113.10"),
                                                 443);

        QVERIFY(QMetaObject::invokeMethod(&client, "onConnectionPromptRequested",
                                          Qt::DirectConnection, Q_ARG(QVariantMap, outgoingA)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onConnectionPromptRequested",
                                          Qt::DirectConnection, Q_ARG(QVariantMap, outgoingB)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onConnectionPromptRequested",
                                          Qt::DirectConnection, Q_ARG(QVariantMap, incomingA)));

        const QVariantList rows = client.pendingPrompts();
        QCOMPARE(rows.size(), 3);
        QVERIFY(findPromptIndexByIp(rows, QStringLiteral("203.0.113.10")) >= 0);
        QVERIFY(findPromptIndexByIp(rows, QStringLiteral("203.0.113.11")) >= 0);
    }
};

QTEST_GUILESS_MAIN(FirewallServiceClientPromptThrottleTest)
#include "firewallserviceclient_prompt_throttle_test.moc"
