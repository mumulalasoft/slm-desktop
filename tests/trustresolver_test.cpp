#include <QtTest>

#include "../src/core/permissions/CallerIdentity.h"
#include "../src/core/permissions/TrustResolver.h"

using namespace Slm::Permissions;

class TrustResolverTest : public QObject
{
    Q_OBJECT

private slots:
    // CoreDesktopComponent classification
    void classifiesOfficialAppIdAsCore();
    void classifiesOfficialDesktopFileIdAsCore();
    void classifiesExePathContainingDesktopShellAsCore();

    // PrivilegedDesktopService classification
    void classifiesPrivilegedBusNameAsPrivileged();
    void classifiesBusNameWithOrgSlmDesktopPrefixAsPrivileged();

    // ThirdPartyApplication classification (default)
    void classifiesUnknownCallerAsThirdParty();
    void classifiesEmptyIdentityAsThirdParty();

    // isOfficialComponent flag
    void setsIsOfficialComponentForCoreComponent();
    void doesNotSetIsOfficialComponentForPrivileged();
    void doesNotSetIsOfficialComponentForThirdParty();

    // Normalization: case-insensitive matching
    void matchesAppIdCaseInsensitively();
    void matchesBusNameCaseInsensitively();

    // Priority: core wins over privileged
    void coreTrumpsPotentialPrivilegedBus();

    // Custom lists
    void customOfficialAppIdClassifiedAsCore();
    void customPrivilegedServiceClassifiedAsPrivileged();
};

// ── CoreDesktopComponent ──────────────────────────────────────────────────────

void TrustResolverTest::classifiesOfficialAppIdAsCore()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("com.example.App");
    id.appId   = QStringLiteral("desktopd");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
    QVERIFY(out.isOfficialComponent);
}

void TrustResolverTest::classifiesOfficialDesktopFileIdAsCore()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName      = QStringLiteral("com.example.App");
    id.desktopFileId = QStringLiteral("appdesktop_shell.desktop");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
    QVERIFY(out.isOfficialComponent);
}

void TrustResolverTest::classifiesExePathContainingDesktopShellAsCore()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName       = QStringLiteral("com.example.Random");
    id.executablePath = QStringLiteral("/usr/bin/desktop_shell");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
    QVERIFY(out.isOfficialComponent);
}

// ── PrivilegedDesktopService ──────────────────────────────────────────────────

void TrustResolverTest::classifiesPrivilegedBusNameAsPrivileged()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("org.slm.desktopd");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::PrivilegedDesktopService);
    QVERIFY(!out.isOfficialComponent);
}

void TrustResolverTest::classifiesBusNameWithOrgSlmDesktopPrefixAsPrivileged()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("org.slm.desktop.myservice");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::PrivilegedDesktopService);
    QVERIFY(!out.isOfficialComponent);
}

// ── ThirdPartyApplication ─────────────────────────────────────────────────────

void TrustResolverTest::classifiesUnknownCallerAsThirdParty()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("com.unknown.ThirdParty");
    id.appId   = QStringLiteral("unknown-app");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::ThirdPartyApplication);
    QVERIFY(!out.isOfficialComponent);
}

void TrustResolverTest::classifiesEmptyIdentityAsThirdParty()
{
    TrustResolver resolver;
    CallerIdentity id; // all fields empty

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::ThirdPartyApplication);
    QVERIFY(!out.isOfficialComponent);
}

// ── isOfficialComponent flag ──────────────────────────────────────────────────

void TrustResolverTest::setsIsOfficialComponentForCoreComponent()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.appId = QStringLiteral("appdesktop_shell");

    const CallerIdentity out = resolver.resolveTrust(id);
    QVERIFY(out.isOfficialComponent);
}

void TrustResolverTest::doesNotSetIsOfficialComponentForPrivileged()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("org.slm.desktop.portal");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::PrivilegedDesktopService);
    QVERIFY(!out.isOfficialComponent);
}

void TrustResolverTest::doesNotSetIsOfficialComponentForThirdParty()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("com.third.party");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::ThirdPartyApplication);
    QVERIFY(!out.isOfficialComponent);
}

// ── Case-insensitive matching ─────────────────────────────────────────────────

void TrustResolverTest::matchesAppIdCaseInsensitively()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.appId = QStringLiteral("DESKTOPD"); // uppercase

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
}

void TrustResolverTest::matchesBusNameCaseInsensitively()
{
    TrustResolver resolver;
    CallerIdentity id;
    id.busName = QStringLiteral("ORG.SLM.DESKTOPD"); // uppercase

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::PrivilegedDesktopService);
}

// ── Priority ──────────────────────────────────────────────────────────────────

void TrustResolverTest::coreTrumpsPotentialPrivilegedBus()
{
    TrustResolver resolver;
    CallerIdentity id;
    // appId classifies as core, bus also matches privileged prefix — core wins.
    id.appId   = QStringLiteral("desktopd");
    id.busName = QStringLiteral("org.slm.desktop.something");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
}

// ── Custom lists ──────────────────────────────────────────────────────────────

void TrustResolverTest::customOfficialAppIdClassifiedAsCore()
{
    TrustResolver resolver;
    resolver.setOfficialAppIds({QStringLiteral("my.special.app")});

    CallerIdentity id;
    id.busName = QStringLiteral("com.example.Random");
    id.appId   = QStringLiteral("my.special.app");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::CoreDesktopComponent);
    QVERIFY(out.isOfficialComponent);
}

void TrustResolverTest::customPrivilegedServiceClassifiedAsPrivileged()
{
    TrustResolver resolver;
    resolver.setPrivilegedServiceNames({QStringLiteral("com.corp.internal")});

    CallerIdentity id;
    id.busName = QStringLiteral("com.corp.internal");

    const CallerIdentity out = resolver.resolveTrust(id);
    QCOMPARE(out.trustLevel, TrustLevel::PrivilegedDesktopService);
    QVERIFY(!out.isOfficialComponent);
}

QTEST_MAIN(TrustResolverTest)
#include "trustresolver_test.moc"
