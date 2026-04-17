#include "src/daemon/portald/portal-adapter/PortalAppContextResolver.h"
#include "src/core/permissions/CallerIdentity.h"

#include <QtTest>

class PortalAppContextResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void explicitDesktopAppIdIsKept()
    {
        Slm::Permissions::CallerIdentity caller;
        caller.appId = QStringLiteral("fallback-app");
        caller.desktopFileId = QStringLiteral("fallback-app.desktop");

        const auto out = Slm::PortalAdapter::PortalAppContextResolver::resolve(
            caller,
            QStringLiteral("org.example.MyApp.desktop"),
            QStringLiteral("wayland:1"));

        QCOMPARE(out.appId, QStringLiteral("org.example.MyApp.desktop"));
        QCOMPARE(out.parentWindowType, QStringLiteral("wayland"));
        QCOMPARE(out.parentWindowId, QStringLiteral("1"));
        QVERIFY(out.parentWindowValid);
        QVERIFY(!out.appIdFromCaller);
    }

    void appIdWithoutDesktopSuffixIsNormalized()
    {
        Slm::Permissions::CallerIdentity caller;

        const auto out = Slm::PortalAdapter::PortalAppContextResolver::resolve(
            caller,
            QStringLiteral("org.example.App"),
            QStringLiteral("x11:0x1234"));

        QCOMPARE(out.appId, QStringLiteral("org.example.App.desktop"));
        QCOMPARE(out.parentWindowType, QStringLiteral("x11"));
        QCOMPARE(out.parentWindowId, QStringLiteral("0x1234"));
        QVERIFY(out.parentWindowValid);
    }

    void fallbackToCallerIdentityWhenAppIdMissing()
    {
        Slm::Permissions::CallerIdentity caller;
        caller.appId = QStringLiteral("kdenlive");
        caller.desktopFileId = QStringLiteral("org.kde.kdenlive.desktop");

        const auto out = Slm::PortalAdapter::PortalAppContextResolver::resolve(
            caller,
            QString(),
            QString());

        QCOMPARE(out.appId, QStringLiteral("org.kde.kdenlive.desktop"));
        QCOMPARE(out.parentWindowType, QStringLiteral("none"));
        QVERIFY(out.parentWindowValid);
        QVERIFY(out.appIdFromCaller);
    }

    void invalidParentWindowIsMarked()
    {
        Slm::Permissions::CallerIdentity caller;
        const auto out = Slm::PortalAdapter::PortalAppContextResolver::resolve(
            caller,
            QStringLiteral("org.example.App"),
            QStringLiteral("gtk:abc"));

        QCOMPARE(out.appId, QStringLiteral("org.example.App.desktop"));
        QCOMPARE(out.parentWindowType, QStringLiteral("invalid"));
        QCOMPARE(out.parentWindowId, QStringLiteral("gtk:abc"));
        QVERIFY(!out.parentWindowValid);
    }
};

QTEST_MAIN(PortalAppContextResolverTest)
#include "portal_app_context_resolver_test.moc"

