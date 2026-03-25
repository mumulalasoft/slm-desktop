#include "../src/apps/settings/modules/developer/enventry.h"
#include "../src/apps/settings/modules/developer/localenvstore.h"

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

// LocalEnvStore writes to QStandardPaths::ConfigLocation + /slm/environment.d/user.json.
// QStandardPaths::setTestModeEnabled(true) redirects that to ~/.qttest/config/
// so tests never touch the real user config.

static QString testFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/slm/environment.d/user.json");
}

class LocalEnvStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();           // run before each test
    void cleanup();        // run after each test

    // ── load / save ──────────────────────────────────────────────────────
    void load_emptyOnMissingFile();
    void save_createsFile();
    void roundtrip_allFields();

    // ── CRUD ─────────────────────────────────────────────────────────────
    void add_entryAppears();
    void add_duplicateKeyRejected();
    void update_valueChanges();
    void update_missingKeyReturnsFalse();
    void remove_entryDisappears();
    void remove_missingKeyReturnsFalse();
    void setEnabled_togglesFlag();

    // ── signals ──────────────────────────────────────────────────────────
    void entriesChanged_emittedOnAdd();
    void entriesChanged_emittedOnRemove();
    void entriesChanged_emittedOnToggle();
};

void LocalEnvStoreTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void LocalEnvStoreTest::init()
{
    // Start each test with a clean file.
    QFile::remove(testFilePath());
}

void LocalEnvStoreTest::cleanup()
{
    QFile::remove(testFilePath());
}

// ── load / save ───────────────────────────────────────────────────────────────

void LocalEnvStoreTest::load_emptyOnMissingFile()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    QCOMPARE(store.entries().size(), 0);
}

void LocalEnvStoreTest::save_createsFile()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    QVERIFY(store.save());
    QVERIFY(QFile::exists(testFilePath()));
}

void LocalEnvStoreTest::roundtrip_allFields()
{
    EnvEntry e;
    e.key       = QStringLiteral("ROUND_TRIP");
    e.value     = QStringLiteral("/some/path");
    e.enabled   = false;
    e.scope     = QStringLiteral("user");
    e.comment   = QStringLiteral("a comment");
    e.mergeMode = QStringLiteral("prepend");

    {
        LocalEnvStore store;
        QVERIFY(store.load());
        QVERIFY(store.addEntry(e));
    }

    // Re-load from disk
    LocalEnvStore store2;
    QVERIFY(store2.load());
    QCOMPARE(store2.entries().size(), 1);

    const EnvEntry loaded = store2.entries().first();
    QCOMPARE(loaded.key,       e.key);
    QCOMPARE(loaded.value,     e.value);
    QCOMPARE(loaded.enabled,   e.enabled);
    QCOMPARE(loaded.scope,     e.scope);
    QCOMPARE(loaded.comment,   e.comment);
    QCOMPARE(loaded.mergeMode, e.mergeMode);
    QVERIFY(loaded.createdAt.isValid());
    QVERIFY(loaded.modifiedAt.isValid());
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

void LocalEnvStoreTest::add_entryAppears()
{
    LocalEnvStore store;
    QVERIFY(store.load());

    EnvEntry e;
    e.key   = QStringLiteral("NEW_VAR");
    e.value = QStringLiteral("hello");

    QVERIFY(store.addEntry(e));
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().key, QStringLiteral("NEW_VAR"));
}

void LocalEnvStoreTest::add_duplicateKeyRejected()
{
    LocalEnvStore store;
    QVERIFY(store.load());

    EnvEntry e;
    e.key   = QStringLiteral("DUP");
    e.value = QStringLiteral("v1");

    QVERIFY(store.addEntry(e));
    QVERIFY(!store.addEntry(e));          // second add must fail
    QCOMPARE(store.entries().size(), 1);  // only one entry
}

void LocalEnvStoreTest::update_valueChanges()
{
    LocalEnvStore store;
    QVERIFY(store.load());

    EnvEntry e;
    e.key   = QStringLiteral("UPD_VAR");
    e.value = QStringLiteral("old");
    QVERIFY(store.addEntry(e));

    EnvEntry updated = e;
    updated.value = QStringLiteral("new");
    QVERIFY(store.updateEntry(QStringLiteral("UPD_VAR"), updated));
    QCOMPARE(store.entries().first().value, QStringLiteral("new"));
}

void LocalEnvStoreTest::update_missingKeyReturnsFalse()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    QVERIFY(!store.updateEntry(QStringLiteral("GHOST"), EnvEntry{}));
}

void LocalEnvStoreTest::remove_entryDisappears()
{
    LocalEnvStore store;
    QVERIFY(store.load());

    EnvEntry e;
    e.key   = QStringLiteral("DEL_VAR");
    e.value = QStringLiteral("bye");
    QVERIFY(store.addEntry(e));
    QCOMPARE(store.entries().size(), 1);

    QVERIFY(store.removeEntry(QStringLiteral("DEL_VAR")));
    QCOMPARE(store.entries().size(), 0);
}

void LocalEnvStoreTest::remove_missingKeyReturnsFalse()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    QVERIFY(!store.removeEntry(QStringLiteral("GHOST")));
}

void LocalEnvStoreTest::setEnabled_togglesFlag()
{
    LocalEnvStore store;
    QVERIFY(store.load());

    EnvEntry e;
    e.key   = QStringLiteral("TOGGLE_VAR");
    e.value = QStringLiteral("x");
    QVERIFY(store.addEntry(e));
    QVERIFY(store.entries().first().enabled);

    QVERIFY(store.setEnabled(QStringLiteral("TOGGLE_VAR"), false));
    QVERIFY(!store.entries().first().enabled);

    QVERIFY(store.setEnabled(QStringLiteral("TOGGLE_VAR"), true));
    QVERIFY(store.entries().first().enabled);
}

// ── signals ───────────────────────────────────────────────────────────────────

void LocalEnvStoreTest::entriesChanged_emittedOnAdd()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    QSignalSpy spy(&store, &LocalEnvStore::entriesChanged);

    EnvEntry e;
    e.key = QStringLiteral("SIG_ADD");
    store.addEntry(e);

    QCOMPARE(spy.count(), 1);
}

void LocalEnvStoreTest::entriesChanged_emittedOnRemove()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    EnvEntry e;
    e.key = QStringLiteral("SIG_REM");
    store.addEntry(e);

    QSignalSpy spy(&store, &LocalEnvStore::entriesChanged);
    store.removeEntry(QStringLiteral("SIG_REM"));
    QCOMPARE(spy.count(), 1);
}

void LocalEnvStoreTest::entriesChanged_emittedOnToggle()
{
    LocalEnvStore store;
    QVERIFY(store.load());
    EnvEntry e;
    e.key = QStringLiteral("SIG_TOG");
    store.addEntry(e);

    QSignalSpy spy(&store, &LocalEnvStore::entriesChanged);
    store.setEnabled(QStringLiteral("SIG_TOG"), false);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(LocalEnvStoreTest)
#include "localenvstore_test.moc"
