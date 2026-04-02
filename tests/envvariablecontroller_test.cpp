#include "../src/apps/settings/modules/developer/envvariablecontroller.h"
#include "../src/apps/settings/modules/developer/envvariablemodel.h"

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

static QString testFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/slm/environment.d/user.json");
}

class EnvVariableControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init()         { QFile::remove(testFilePath()); }
    void cleanup()      { QFile::remove(testFilePath()); }

    // ── addEntry ──────────────────────────────────────────────────────────
    void addEntry_validKey_appearsInModel();
    void addEntry_invalidKey_rejected();
    void addEntry_duplicateKey_rejected();
    void addEntry_emptyPathValue_rejected();

    // ── updateEntry ───────────────────────────────────────────────────────
    void updateEntry_changesValue();
    void updateEntry_missingKey_rejected();

    // ── deleteEntry ───────────────────────────────────────────────────────
    void deleteEntry_removesFromModel();
    void deleteEntry_missingKey_rejected();

    // ── setEnabled ────────────────────────────────────────────────────────
    void setEnabled_reflectedInModel();

    // ── signals ───────────────────────────────────────────────────────────
    void signal_entryAdded_emitted();
    void signal_entryDeleted_emitted();
    void signal_operationFailed_emitted_onInvalidKey();

    // ── validateKey / isSensitiveKey ──────────────────────────────────────
    void validateKey_returnsExpectedSeverity();
    void isSensitiveKey_pathIsTrue();
    void isSensitiveKey_customVarIsFalse();
};

// ── addEntry ──────────────────────────────────────────────────────────────────

void EnvVariableControllerTest::addEntry_validKey_appearsInModel()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.addEntry(QStringLiteral("MY_VAR"), QStringLiteral("hello"), QString{}));
    QCOMPARE(ctrl.model()->rowCount(), 1);
    QCOMPARE(ctrl.model()->data(ctrl.model()->index(0), EnvVariableModel::KeyRole).toString(),
             QStringLiteral("MY_VAR"));
    QCOMPARE(ctrl.model()->data(ctrl.model()->index(0), EnvVariableModel::ValueRole).toString(),
             QStringLiteral("hello"));
    QVERIFY(ctrl.model()->data(ctrl.model()->index(0), EnvVariableModel::EnabledRole).toBool());
}

void EnvVariableControllerTest::addEntry_invalidKey_rejected()
{
    EnvVariableController ctrl;
    QVERIFY(!ctrl.addEntry(QStringLiteral("my_var"), QStringLiteral("hello"), QString{}));
    QVERIFY(!ctrl.lastError().isEmpty());
    QCOMPARE(ctrl.model()->rowCount(), 0);
}

void EnvVariableControllerTest::addEntry_duplicateKey_rejected()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.addEntry(QStringLiteral("DUP"), QStringLiteral("v1"), QString{}));
    QVERIFY(!ctrl.addEntry(QStringLiteral("DUP"), QStringLiteral("v2"), QString{}));
    QCOMPARE(ctrl.model()->rowCount(), 1);
    // First value is preserved
    QCOMPARE(ctrl.model()->data(ctrl.model()->index(0), EnvVariableModel::ValueRole).toString(),
             QStringLiteral("v1"));
}

void EnvVariableControllerTest::addEntry_emptyPathValue_rejected()
{
    EnvVariableController ctrl;
    QVERIFY(!ctrl.addEntry(QStringLiteral("PATH"), QString{}, QString{}));
    QVERIFY(!ctrl.lastError().isEmpty());
    QCOMPARE(ctrl.model()->rowCount(), 0);
}

// ── updateEntry ───────────────────────────────────────────────────────────────

void EnvVariableControllerTest::updateEntry_changesValue()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.addEntry(QStringLiteral("UPD_VAR"), QStringLiteral("old"), QString{}));
    QVERIFY(ctrl.updateEntry(QStringLiteral("UPD_VAR"), QStringLiteral("new"), QString{},
                             QStringLiteral("replace")));
    QCOMPARE(ctrl.model()->data(ctrl.model()->index(0), EnvVariableModel::ValueRole).toString(),
             QStringLiteral("new"));
}

void EnvVariableControllerTest::updateEntry_missingKey_rejected()
{
    EnvVariableController ctrl;
    QVERIFY(!ctrl.updateEntry(QStringLiteral("GHOST"), QStringLiteral("x"), QString{},
                              QStringLiteral("replace")));
}

// ── deleteEntry ───────────────────────────────────────────────────────────────

void EnvVariableControllerTest::deleteEntry_removesFromModel()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.addEntry(QStringLiteral("DEL_ME"), QStringLiteral("x"), QString{}));
    QCOMPARE(ctrl.model()->rowCount(), 1);
    QVERIFY(ctrl.deleteEntry(QStringLiteral("DEL_ME")));
    QCOMPARE(ctrl.model()->rowCount(), 0);
}

void EnvVariableControllerTest::deleteEntry_missingKey_rejected()
{
    EnvVariableController ctrl;
    QVERIFY(!ctrl.deleteEntry(QStringLiteral("GHOST")));
    QVERIFY(!ctrl.lastError().isEmpty());
}

// ── setEnabled ────────────────────────────────────────────────────────────────

void EnvVariableControllerTest::setEnabled_reflectedInModel()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.addEntry(QStringLiteral("EN_VAR"), QStringLiteral("x"), QString{}));

    const QModelIndex idx = ctrl.model()->index(0);
    QVERIFY(ctrl.model()->data(idx, EnvVariableModel::EnabledRole).toBool());

    QVERIFY(ctrl.setEnabled(QStringLiteral("EN_VAR"), false));
    QVERIFY(!ctrl.model()->data(idx, EnvVariableModel::EnabledRole).toBool());

    QVERIFY(ctrl.setEnabled(QStringLiteral("EN_VAR"), true));
    QVERIFY(ctrl.model()->data(idx, EnvVariableModel::EnabledRole).toBool());
}

// ── signals ───────────────────────────────────────────────────────────────────

void EnvVariableControllerTest::signal_entryAdded_emitted()
{
    EnvVariableController ctrl;
    QSignalSpy spy(&ctrl, &EnvVariableController::entryAdded);
    ctrl.addEntry(QStringLiteral("SIG_ADD"), QStringLiteral("v"), QString{});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("SIG_ADD"));
}

void EnvVariableControllerTest::signal_entryDeleted_emitted()
{
    EnvVariableController ctrl;
    ctrl.addEntry(QStringLiteral("SIG_DEL"), QStringLiteral("v"), QString{});
    QSignalSpy spy(&ctrl, &EnvVariableController::entryDeleted);
    ctrl.deleteEntry(QStringLiteral("SIG_DEL"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("SIG_DEL"));
}

void EnvVariableControllerTest::signal_operationFailed_emitted_onInvalidKey()
{
    EnvVariableController ctrl;
    QSignalSpy spy(&ctrl, &EnvVariableController::operationFailed);
    ctrl.addEntry(QStringLiteral("bad key"), QStringLiteral("v"), QString{});
    QCOMPARE(spy.count(), 1);
}

// ── validateKey / isSensitiveKey ──────────────────────────────────────────────

void EnvVariableControllerTest::validateKey_returnsExpectedSeverity()
{
    EnvVariableController ctrl;

    const QVariantMap r1 = ctrl.validateKey(QStringLiteral("PATH"));
    QVERIFY(r1.value(QStringLiteral("valid")).toBool());
    QCOMPARE(r1.value(QStringLiteral("severity")).toString(), QStringLiteral("critical"));

    const QVariantMap r2 = ctrl.validateKey(QStringLiteral("MY_CUSTOM"));
    QVERIFY(r2.value(QStringLiteral("valid")).toBool());
    QCOMPARE(r2.value(QStringLiteral("severity")).toString(), QStringLiteral("none"));

    const QVariantMap r3 = ctrl.validateKey(QStringLiteral("bad key"));
    QVERIFY(!r3.value(QStringLiteral("valid")).toBool());
}

void EnvVariableControllerTest::isSensitiveKey_pathIsTrue()
{
    EnvVariableController ctrl;
    QVERIFY(ctrl.isSensitiveKey(QStringLiteral("PATH")));
    QVERIFY(ctrl.isSensitiveKey(QStringLiteral("LD_LIBRARY_PATH")));
}

void EnvVariableControllerTest::isSensitiveKey_customVarIsFalse()
{
    EnvVariableController ctrl;
    QVERIFY(!ctrl.isSensitiveKey(QStringLiteral("MY_CUSTOM_VAR")));
}

QTEST_MAIN(EnvVariableControllerTest)
#include "envvariablecontroller_test.moc"
