#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QCoreApplication>
#include <QTemporaryDir>

#include "../src/daemon/portald/portal-adapter/PortalBackendService.h"
#include "../src/daemon/portald/portalmanager.h"
#include "../src/daemon/portald/portalservice.h"
#include "../src/core/permissions/Capability.h"
#include "../src/core/permissions/PermissionBroker.h"
#include "../src/core/permissions/PermissionStore.h"
#include "../src/core/permissions/PolicyEngine.h"
#include "../src/core/permissions/TrustResolver.h"
#include "../src/services/secret/secretservice.h"

namespace {
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
constexpr const char kSecretService[] = "org.slm.Secret1";
constexpr const char kSecretPath[] = "/org/slm/Secret1";
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";

bool describeSecretState(QDBusInterface &portal, const QVariantMap &query, bool expectedOk)
{
    QDBusReply<QVariantMap> reply = portal.call(QStringLiteral("DescribeSecret"), query);
    return reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool() == expectedOk;
}

class FakePortalUiService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")

public:
    int callCount = 0;
    QVariantMap lastPayload;
    QVariantMap consentResponse{
        {QStringLiteral("decision"), QStringLiteral("allow_always")},
        {QStringLiteral("persist"), true},
        {QStringLiteral("scope"), QStringLiteral("persistent")},
        {QStringLiteral("reason"), QStringLiteral("test-consent")},
    };

public slots:
    QVariantMap ConsentRequest(const QVariantMap &payload)
    {
        ++callCount;
        lastPayload = payload;
        return consentResponse;
    }
};
}

class PortalSecretIntegrationTest : public QObject
{
    Q_OBJECT

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();

private slots:
    void listAndRevokeSecretConsentGrants_filtersSecretCapabilities()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        QVERIFY(permissionStore.savePermission(QStringLiteral("org.demo.app"),
                                               Slm::Permissions::Capability::SecretRead,
                                               Slm::Permissions::DecisionType::AllowAlways,
                                               QStringLiteral("persistent"),
                                               QStringLiteral("secret"),
                                               QStringLiteral("token")));
        QVERIFY(permissionStore.savePermission(QStringLiteral("org.demo.app"),
                                               Slm::Permissions::Capability::SecretStore,
                                               Slm::Permissions::DecisionType::AllowAlways,
                                               QStringLiteral("persistent"),
                                               QStringLiteral("secret"),
                                               QStringLiteral("refresh")));
        QVERIFY(permissionStore.savePermission(QStringLiteral("org.demo.app"),
                                               Slm::Permissions::Capability::PortalOpenURI,
                                               Slm::Permissions::DecisionType::AllowAlways,
                                               QStringLiteral("persistent")));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        trustResolver.setOfficialAppIds({
            QStringLiteral("portal_secret_integration_test"),
            QCoreApplication::applicationName(),
        });
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        QDBusReply<QVariantMap> listReply =
            portal.call(QStringLiteral("ListSecretConsentGrants"), QVariantMap{});
        QVERIFY(listReply.isValid());
        QVERIFY(listReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantList beforeItems = listReply.value().value(QStringLiteral("items")).toList();
        QCOMPARE(beforeItems.size(), 2);

        QDBusReply<QVariantMap> revokeReply =
            portal.call(QStringLiteral("RevokeSecretConsentGrants"),
                        QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.demo.app")}});
        QVERIFY(revokeReply.isValid());
        QVERIFY(revokeReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(revokeReply.value().value(QStringLiteral("revokeCount")).toInt(), 2);

        listReply = portal.call(QStringLiteral("ListSecretConsentGrants"),
                                QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.demo.app")}});
        QVERIFY(listReply.isValid());
        QVERIFY(listReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(listReply.value().value(QStringLiteral("items")).toList().isEmpty());

        const Slm::Permissions::StoredPermission nonSecret =
            permissionStore.findPermission(QStringLiteral("org.demo.app"),
                                           Slm::Permissions::Capability::PortalOpenURI);
        QVERIFY(nonSecret.valid);
    }

    void storeGetDescribeDelete_roundtrip()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        trustResolver.setOfficialAppIds({
            QStringLiteral("portal_secret_integration_test"),
        });
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
            {QStringLiteral("sensitivity"), QStringLiteral("high")},
        };
        const QByteArray secret = QByteArrayLiteral("s3cr3t-value");

        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, secret);
        QVERIFY(storeReply.isValid());
        const QVariantMap storeResult = storeReply.value();
        QVERIFY(storeResult.value(QStringLiteral("ok")).toBool());
        QVERIFY(!storeResult.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty());

        QVariantMap storeOptions2 = storeOptions;
        storeOptions2.insert(QStringLiteral("key"), QStringLiteral("refresh-token"));
        const QByteArray secret2 = QByteArrayLiteral("refresh-value");
        storeReply = portal.call(QStringLiteral("StoreSecret"), storeOptions2, secret2);
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());

        QVariantMap query{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
        };

        QTRY_VERIFY_WITH_TIMEOUT(describeSecretState(portal, query, true), 1000);
        QDBusReply<QVariantMap> describeReply = portal.call(QStringLiteral("DescribeSecret"), query);
        QVERIFY(describeReply.isValid());
        const QVariantMap describeResult = describeReply.value();
        QVERIFY(describeResult.value(QStringLiteral("ok")).toBool());
        const QVariantMap metadata = describeResult.value(QStringLiteral("metadata")).toMap();
        QCOMPARE(metadata.value(QStringLiteral("namespace")).toString(), QStringLiteral("session"));
        QCOMPARE(metadata.value(QStringLiteral("key")).toString(), QStringLiteral("token"));

        QDBusReply<QVariantMap> listReply =
            portal.call(QStringLiteral("ListOwnSecretMetadata"),
                        QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
        QVERIFY(listReply.isValid());
        const QVariantMap listResult = listReply.value();
        QVERIFY(listResult.value(QStringLiteral("ok")).toBool());
        const QVariantList items = listResult.value(QStringLiteral("items")).toList();
        QCOMPARE(items.size(), 2);
        const QVariantMap item0 = items.constFirst().toMap();
        QCOMPARE(item0.value(QStringLiteral("namespace")).toString(), QStringLiteral("session"));
        QVERIFY(!item0.value(QStringLiteral("key")).toString().trimmed().isEmpty());

        QDBusReply<QVariantMap> appIdsReply = portal.call(QStringLiteral("ListSecretAppIds"),
                                                          QVariantMap{});
        QVERIFY(appIdsReply.isValid());
        const QVariantMap appIdsResult = appIdsReply.value();
        QVERIFY(appIdsResult.value(QStringLiteral("ok")).toBool());
        const QVariantList appIds = appIdsResult.value(QStringLiteral("apps")).toList();
        QVERIFY(appIds.contains(QStringLiteral("org.slm.tests.secret")));

        QDBusReply<QVariantMap> getReply = portal.call(QStringLiteral("GetSecret"), query);
        QVERIFY(getReply.isValid());
        const QVariantMap getResult = getReply.value();
        QVERIFY(getResult.value(QStringLiteral("ok")).toBool());
        QVERIFY(!getResult.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty());

        QDBusReply<QVariantMap> clearReply = portal.call(
            QStringLiteral("ClearAppSecrets"),
            QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
        QVERIFY(clearReply.isValid());
        const QVariantMap clearResult = clearReply.value();
        QVERIFY(clearResult.value(QStringLiteral("ok")).toBool());
        QVERIFY(!clearResult.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty());

        QTRY_VERIFY_WITH_TIMEOUT(describeSecretState(portal, query, false), 1000);
        describeReply = portal.call(QStringLiteral("DescribeSecret"), query);
        QVERIFY(describeReply.isValid());
        const QVariantMap missing = describeReply.value();
        QVERIFY(!missing.value(QStringLiteral("ok")).toBool());
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("not-found"));

        listReply = portal.call(QStringLiteral("ListOwnSecretMetadata"),
                                QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
        QVERIFY(listReply.isValid());
        const QVariantMap emptyListResult = listReply.value();
        QVERIFY(emptyListResult.value(QStringLiteral("ok")).toBool());
        QVERIFY(emptyListResult.value(QStringLiteral("items")).toList().isEmpty());

        appIdsReply = portal.call(QStringLiteral("ListSecretAppIds"), QVariantMap{});
        QVERIFY(appIdsReply.isValid());
        const QVariantMap appIdsAfterClear = appIdsReply.value();
        QVERIFY(appIdsAfterClear.value(QStringLiteral("ok")).toBool());
        QVERIFY(!appIdsAfterClear.value(QStringLiteral("apps")).toList().contains(
            QStringLiteral("org.slm.tests.secret")));

        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void clearAppSecrets_consentAlwaysAllowDoesNotPersist_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };

        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, QByteArrayLiteral("test-token"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Store"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();

        for (int i = 0; i < 2; ++i) {
            QDBusReply<QVariantMap> clearReply = portal.call(
                QStringLiteral("ClearAppSecrets"),
                QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
            QVERIFY(clearReply.isValid());
            QVERIFY(clearReply.value().value(QStringLiteral("ok")).toBool());
            QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Delete"));
            QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), false);
        }
        QCOMPARE(fakeUi.callCount, 2);

        const Slm::Permissions::StoredPermission persistedDelete = permissionStore.findPermission(
            QStringLiteral("portal_secret_integration_test"),
            Slm::Permissions::Capability::SecretDelete,
            QStringLiteral("secret"),
            QString());
        QVERIFY(!persistedDelete.valid);

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void deleteSecret_consentAlwaysAllowPersists_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("allow_always")},
            {QStringLiteral("persist"), true},
            {QStringLiteral("scope"), QStringLiteral("persistent")},
            {QStringLiteral("reason"), QStringLiteral("allow-always-delete")},
        };
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        const QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };
        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, QByteArrayLiteral("delete-me"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());

        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();

        const QVariantMap deleteOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
        };
        QDBusReply<QVariantMap> deleteReply = portal.call(QStringLiteral("DeleteSecret"), deleteOptions);
        QVERIFY(deleteReply.isValid());
        QVERIFY(deleteReply.value().value(QStringLiteral("ok")).toBool());

        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Delete"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        const Slm::Permissions::StoredPermission persistedDelete = permissionStore.findPermission(
            QStringLiteral("portal_secret_integration_test"),
            Slm::Permissions::Capability::SecretDelete,
            QStringLiteral("secret"),
            QStringLiteral("token"));
        QVERIFY(persistedDelete.valid);
        QCOMPARE(persistedDelete.decision, Slm::Permissions::DecisionType::AllowAlways);
        QCOMPARE(persistedDelete.scope, QStringLiteral("persistent"));

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void getSecret_consentAlwaysAllowPersists_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("allow_always")},
            {QStringLiteral("persist"), true},
            {QStringLiteral("scope"), QStringLiteral("persistent")},
            {QStringLiteral("reason"), QStringLiteral("allow-always-read")},
        };
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        const QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };
        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, QByteArrayLiteral("read-me"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());

        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();

        const QVariantMap getOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
        };
        QDBusReply<QVariantMap> getReply = portal.call(QStringLiteral("GetSecret"), getOptions);
        QVERIFY(getReply.isValid());
        QVERIFY(getReply.value().value(QStringLiteral("ok")).toBool());

        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Read"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        const Slm::Permissions::StoredPermission persistedRead = permissionStore.findPermission(
            QStringLiteral("portal_secret_integration_test"),
            Slm::Permissions::Capability::SecretRead,
            QStringLiteral("secret"),
            QStringLiteral("token"));
        QVERIFY(persistedRead.valid);
        QCOMPARE(persistedRead.decision, Slm::Permissions::DecisionType::AllowAlways);
        QCOMPARE(persistedRead.scope, QStringLiteral("persistent"));

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void storeSecret_consentAlwaysAllowPersists_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("allow_always")},
            {QStringLiteral("persist"), true},
            {QStringLiteral("scope"), QStringLiteral("persistent")},
            {QStringLiteral("reason"), QStringLiteral("allow-always-store")},
        };
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        const QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };
        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, QByteArrayLiteral("store-me"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());

        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Store"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        const Slm::Permissions::StoredPermission persistedStore = permissionStore.findPermission(
            QStringLiteral("portal_secret_integration_test"),
            Slm::Permissions::Capability::SecretStore,
            QStringLiteral("secret"),
            QStringLiteral("token"));
        QVERIFY(persistedStore.valid);
        QCOMPARE(persistedStore.decision, Slm::Permissions::DecisionType::AllowAlways);
        QCOMPARE(persistedStore.scope, QStringLiteral("persistent"));

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void clearAppSecrets_denyAlwaysDoesNotPersistAndReprompts_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("allow_always")},
            {QStringLiteral("persist"), true},
            {QStringLiteral("scope"), QStringLiteral("persistent")},
            {QStringLiteral("reason"), QStringLiteral("seed-store")},
        };
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        const QVariantMap storeOptions{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };
        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), storeOptions, QByteArrayLiteral("seed"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());

        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("deny_always")},
            {QStringLiteral("persist"), true},
            {QStringLiteral("scope"), QStringLiteral("persistent")},
            {QStringLiteral("reason"), QStringLiteral("deny-clear")},
        };

        for (int i = 0; i < 2; ++i) {
            QDBusReply<QVariantMap> clearReply = portal.call(
                QStringLiteral("ClearAppSecrets"),
                QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
            QVERIFY(clearReply.isValid());
            QVERIFY(clearReply.value().value(QStringLiteral("ok")).toBool());
            QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(),
                     QStringLiteral("Secret.Delete"));
            QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), false);
        }
        QCOMPARE(fakeUi.callCount, 2);

        const Slm::Permissions::StoredPermission persistedDelete = permissionStore.findPermission(
            QStringLiteral("portal_secret_integration_test"),
            Slm::Permissions::Capability::SecretDelete,
            QStringLiteral("secret"),
            QString());
        QVERIFY(!persistedDelete.valid);

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }

    void secretConsentPayload_persistentEligible_matrix_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSecretService)).value()) {
            QSKIP("org.slm.Secret1 already owned by another process");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            QSKIP("org.slm.Desktop.PortalUI already owned by another process");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        Slm::Secret::SecretService secretService;
        QVERIFY(m_bus.registerService(QString::fromLatin1(kSecretService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kSecretPath),
                                     &secretService,
                                     QDBusConnection::ExportAllSlots));

        FakePortalUiService fakeUi;
        fakeUi.consentResponse = {
            {QStringLiteral("decision"), QStringLiteral("allow_once")},
            {QStringLiteral("persist"), false},
            {QStringLiteral("scope"), QStringLiteral("session")},
            {QStringLiteral("reason"), QStringLiteral("matrix-check")},
        };
        QVERIFY(m_bus.registerService(QString::fromLatin1(kUiService)));
        QVERIFY(m_bus.registerObject(QString::fromLatin1(kUiPath),
                                     &fakeUi,
                                     QDBusConnection::ExportAllSlots));

        PortalManager manager;
        Slm::PortalAdapter::PortalBackendService backend;
        backend.setBus(m_bus);
        Slm::Permissions::PermissionStore permissionStore;
        QVERIFY(permissionStore.open(temp.filePath(QStringLiteral("permissions.db"))));
        Slm::Permissions::PolicyEngine policyEngine;
        policyEngine.setStore(&permissionStore);
        Slm::Permissions::PermissionBroker permissionBroker;
        permissionBroker.setStore(&permissionStore);
        permissionBroker.setPolicyEngine(&policyEngine);
        Slm::Permissions::TrustResolver trustResolver;
        backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &permissionStore);
        PortalService portalService(&manager, &backend);
        if (!portalService.serviceRegistered()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
            m_bus.unregisterService(QString::fromLatin1(kSecretService));
            QSKIP("cannot register org.slm.Desktop.Portal service");
        }

        QDBusInterface portal(QString::fromLatin1(kPortalService),
                              QString::fromLatin1(kPortalPath),
                              QString::fromLatin1(kPortalIface),
                              m_bus);
        QVERIFY(portal.isValid());

        const QVariantMap base{
            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
            {QStringLiteral("namespace"), QStringLiteral("session")},
            {QStringLiteral("key"), QStringLiteral("token")},
            {QStringLiteral("label"), QStringLiteral("Portal token")},
        };

        // StoreSecret -> Secret.Store, persistentEligible=true
        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();
        QDBusReply<QVariantMap> storeReply =
            portal.call(QStringLiteral("StoreSecret"), base, QByteArrayLiteral("matrix-value"));
        QVERIFY(storeReply.isValid());
        QVERIFY(storeReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Store"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        // GetSecret -> Secret.Read, persistentEligible=true
        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();
        QDBusReply<QVariantMap> getReply = portal.call(QStringLiteral("GetSecret"), base);
        QVERIFY(getReply.isValid());
        QVERIFY(getReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Read"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        // DeleteSecret -> Secret.Delete, persistentEligible=true
        QDBusReply<QVariantMap> storeReply2 =
            portal.call(QStringLiteral("StoreSecret"),
                        QVariantMap{
                            {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
                            {QStringLiteral("namespace"), QStringLiteral("session")},
                            {QStringLiteral("key"), QStringLiteral("token-delete")},
                            {QStringLiteral("label"), QStringLiteral("Portal token")},
                        },
                        QByteArrayLiteral("delete-value"));
        QVERIFY(storeReply2.isValid());
        QVERIFY(storeReply2.value().value(QStringLiteral("ok")).toBool());
        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();
        QDBusReply<QVariantMap> deleteReply = portal.call(
            QStringLiteral("DeleteSecret"),
            QVariantMap{
                {QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")},
                {QStringLiteral("namespace"), QStringLiteral("session")},
                {QStringLiteral("key"), QStringLiteral("token-delete")},
            });
        QVERIFY(deleteReply.isValid());
        QVERIFY(deleteReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Delete"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), true);

        // ClearAppSecrets -> Secret.Delete, persistentEligible=false
        QDBusReply<QVariantMap> storeReply3 =
            portal.call(QStringLiteral("StoreSecret"), base, QByteArrayLiteral("to-clear"));
        QVERIFY(storeReply3.isValid());
        QVERIFY(storeReply3.value().value(QStringLiteral("ok")).toBool());
        fakeUi.callCount = 0;
        fakeUi.lastPayload.clear();
        QDBusReply<QVariantMap> clearReply = portal.call(
            QStringLiteral("ClearAppSecrets"),
            QVariantMap{{QStringLiteral("appId"), QStringLiteral("org.slm.tests.secret")}});
        QVERIFY(clearReply.isValid());
        QVERIFY(clearReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(fakeUi.callCount >= 1);
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Delete"));
        QCOMPARE(fakeUi.lastPayload.value(QStringLiteral("persistentEligible")).toBool(), false);

        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
        m_bus.unregisterObject(QString::fromLatin1(kSecretPath));
        m_bus.unregisterService(QString::fromLatin1(kSecretService));
    }
};

QTEST_MAIN(PortalSecretIntegrationTest)

#include "portal_secret_integration_test.moc"
