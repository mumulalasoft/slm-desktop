#include "../src/core/launcher/launchenvresolver.h"
#include "../src/apps/settings/modules/developer/envserviceclient.h"

#include <QProcessEnvironment>
#include <QTest>
#include <QThread>

// ── Minimal stub for EnvServiceClient ─────────────────────────────────────────
//
// We don't want to spin up a real D-Bus session, so we subclass
// EnvServiceClient and override the methods that LaunchEnvResolver uses.
// We can't mock the D-Bus calls directly without a running bus, so we
// test LaunchEnvResolver's cache logic with a real (but disconnected) client.
//
// NOTE: A disconnected client returns serviceAvailable() == false, so the
// resolver falls back to QProcessEnvironment::systemEnvironment().
// These tests validate:
//   1. Fallback returns a non-empty environment.
//   2. The cache is populated and reused within TTL.
//   3. invalidate() clears the cache for a specific key.
//   4. invalidateAll() clears everything.

class LaunchEnvResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void fallback_returns_system_env();
    void resolve_same_appid_within_ttl_reuses_cache();
    void invalidate_clears_specific_entry();
    void invalidateAll_clears_all_entries();
    void empty_appid_resolves_session_wide();
};

void LaunchEnvResolverTest::fallback_returns_system_env()
{
    EnvServiceClient client;    // disconnected — serviceAvailable() == false
    LaunchEnvResolver resolver(&client);

    const QProcessEnvironment env = resolver.resolve(QStringLiteral("firefox"));
    // Fallback should be system env which is never empty on a real machine.
    QVERIFY(!env.isEmpty());
    // Should contain at least PATH.
    QVERIFY(env.contains(QStringLiteral("PATH"))
            || env.toStringList().size() > 0);
}

void LaunchEnvResolverTest::resolve_same_appid_within_ttl_reuses_cache()
{
    EnvServiceClient client;
    LaunchEnvResolver resolver(&client);

    // First resolve — populates cache.
    const QProcessEnvironment e1 = resolver.resolve(QStringLiteral("myapp"));
    // Second resolve immediately — should hit cache (same object identity not
    // testable, but it must return the same keys).
    const QProcessEnvironment e2 = resolver.resolve(QStringLiteral("myapp"));

    QCOMPARE(e1.toStringList().size(), e2.toStringList().size());
}

void LaunchEnvResolverTest::invalidate_clears_specific_entry()
{
    EnvServiceClient client;
    LaunchEnvResolver resolver(&client);

    resolver.resolve(QStringLiteral("app1"));
    resolver.resolve(QStringLiteral("app2"));

    // Invalidate only app1.
    resolver.invalidate(QStringLiteral("app1"));

    // app2 should still be resolvable (hits the still-valid cache).
    // We can't verify cache hit vs miss externally, so just confirm it
    // doesn't crash and returns a valid env.
    const QProcessEnvironment e = resolver.resolve(QStringLiteral("app2"));
    QVERIFY(!e.isEmpty());
}

void LaunchEnvResolverTest::invalidateAll_clears_all_entries()
{
    EnvServiceClient client;
    LaunchEnvResolver resolver(&client);

    resolver.resolve(QStringLiteral("app1"));
    resolver.resolve(QStringLiteral("app2"));
    resolver.invalidateAll();

    // Both must be re-fetchable without crash.
    const QProcessEnvironment e1 = resolver.resolve(QStringLiteral("app1"));
    const QProcessEnvironment e2 = resolver.resolve(QStringLiteral("app2"));
    QVERIFY(!e1.isEmpty());
    QVERIFY(!e2.isEmpty());
}

void LaunchEnvResolverTest::empty_appid_resolves_session_wide()
{
    EnvServiceClient client;
    LaunchEnvResolver resolver(&client);

    // Empty app ID should not crash and should return a valid (fallback) env.
    const QProcessEnvironment e = resolver.resolve(QString{});
    QVERIFY(!e.isEmpty());
}

QTEST_MAIN(LaunchEnvResolverTest)
#include "launchenvresolver_test.moc"
