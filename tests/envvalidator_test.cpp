#include "../src/apps/settings/modules/developer/envvalidator.h"

#include <QtTest>

class EnvValidatorTest : public QObject
{
    Q_OBJECT

private slots:
    // ── validateKey ──────────────────────────────────────────────────────
    void validKeys();
    void invalidKey_empty();
    void invalidKey_lowercase();
    void invalidKey_spaces();
    void invalidKey_leadingDigit();
    void invalidKey_hyphen();
    void validKey_leadingUnderscore();

    // ── sensitive key detection ──────────────────────────────────────────
    void sensitiveSeverity_critical();
    void sensitiveSeverity_high();
    void sensitiveSeverity_medium();
    void sensitiveSeverity_unknown();
    void sensitiveDescription_known();
    void sensitiveDescription_unknown();

    // ── validateValue ────────────────────────────────────────────────────
    void value_emptyPathBlocked();
    void value_emptyNonCriticalAllowed();
    void value_normalValueAlwaysValid();
};

// ── validateKey ──────────────────────────────────────────────────────────────

void EnvValidatorTest::validKeys()
{
    const auto r1 = EnvValidator::validateKey(QStringLiteral("MY_VAR"));
    QVERIFY(r1.valid);

    const auto r2 = EnvValidator::validateKey(QStringLiteral("VAR123"));
    QVERIFY(r2.valid);

    const auto r3 = EnvValidator::validateKey(QStringLiteral("_PRIVATE"));
    QVERIFY(r3.valid);

    const auto r4 = EnvValidator::validateKey(QStringLiteral("A"));
    QVERIFY(r4.valid);
}

void EnvValidatorTest::invalidKey_empty()
{
    const auto r = EnvValidator::validateKey(QString{});
    QVERIFY(!r.valid);
    QVERIFY(!r.message.isEmpty());
}

void EnvValidatorTest::invalidKey_lowercase()
{
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("my_var")).valid);
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("MyVar")).valid);
}

void EnvValidatorTest::invalidKey_spaces()
{
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("MY VAR")).valid);
    QVERIFY(!EnvValidator::validateKey(QStringLiteral(" MYVAR")).valid);
}

void EnvValidatorTest::invalidKey_leadingDigit()
{
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("1VAR")).valid);
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("0_VAR")).valid);
}

void EnvValidatorTest::invalidKey_hyphen()
{
    QVERIFY(!EnvValidator::validateKey(QStringLiteral("MY-VAR")).valid);
}

void EnvValidatorTest::validKey_leadingUnderscore()
{
    const auto r = EnvValidator::validateKey(QStringLiteral("_MY_VAR"));
    QVERIFY(r.valid);
}

// ── sensitive keys ───────────────────────────────────────────────────────────

void EnvValidatorTest::sensitiveSeverity_critical()
{
    const QStringList critical = {"PATH", "LD_LIBRARY_PATH", "DBUS_SESSION_BUS_ADDRESS"};
    for (const QString &k : critical) {
        const auto r = EnvValidator::validateKey(k);
        QVERIFY2(r.valid,    qPrintable(k + " should be valid"));
        QCOMPARE(r.severity, QStringLiteral("critical"));
        QVERIFY2(EnvValidator::isSensitiveKey(k), qPrintable(k + " should be sensitive"));
    }
}

void EnvValidatorTest::sensitiveSeverity_high()
{
    const QStringList high = {"WAYLAND_DISPLAY", "DISPLAY", "QT_PLUGIN_PATH"};
    for (const QString &k : high) {
        const auto r = EnvValidator::validateKey(k);
        QVERIFY2(r.valid,    qPrintable(k + " should be valid"));
        QCOMPARE(r.severity, QStringLiteral("high"));
    }
}

void EnvValidatorTest::sensitiveSeverity_medium()
{
    const auto r = EnvValidator::validateKey(QStringLiteral("LD_PRELOAD"));
    QVERIFY(r.valid);
    QCOMPARE(r.severity, QStringLiteral("medium"));
}

void EnvValidatorTest::sensitiveSeverity_unknown()
{
    const auto r = EnvValidator::validateKey(QStringLiteral("MY_CUSTOM_VAR"));
    QVERIFY(r.valid);
    QCOMPARE(r.severity, QStringLiteral("none"));
    QVERIFY(!EnvValidator::isSensitiveKey(QStringLiteral("MY_CUSTOM_VAR")));
}

void EnvValidatorTest::sensitiveDescription_known()
{
    const QString desc = EnvValidator::sensitiveKeyDescription(QStringLiteral("PATH"));
    QVERIFY(!desc.isEmpty());
    QVERIFY(desc.contains(QStringLiteral("PATH"), Qt::CaseInsensitive)
         || desc.contains(QStringLiteral("executable"), Qt::CaseInsensitive));
}

void EnvValidatorTest::sensitiveDescription_unknown()
{
    const QString desc = EnvValidator::sensitiveKeyDescription(QStringLiteral("TOTALLY_CUSTOM"));
    QVERIFY(!desc.isEmpty()); // fallback message
}

// ── validateValue ────────────────────────────────────────────────────────────

void EnvValidatorTest::value_emptyPathBlocked()
{
    const auto r = EnvValidator::validateValue(QStringLiteral("PATH"), QString{});
    QVERIFY(!r.valid);
    QCOMPARE(r.severity, QStringLiteral("critical"));
}

void EnvValidatorTest::value_emptyNonCriticalAllowed()
{
    const auto r = EnvValidator::validateValue(QStringLiteral("MY_VAR"), QString{});
    QVERIFY(r.valid);
}

void EnvValidatorTest::value_normalValueAlwaysValid()
{
    QVERIFY(EnvValidator::validateValue(QStringLiteral("PATH"),
                                        QStringLiteral("/usr/bin:/bin")).valid);
    QVERIFY(EnvValidator::validateValue(QStringLiteral("MY_VAR"),
                                        QStringLiteral("hello")).valid);
    QVERIFY(EnvValidator::validateValue(QStringLiteral("MY_VAR"),
                                        QStringLiteral("value with spaces")).valid);
}

QTEST_MAIN(EnvValidatorTest)
#include "envvalidator_test.moc"
