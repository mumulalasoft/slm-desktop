#include "../src/services/envd/mergeresolver.h"
#include "../src/apps/settings/modules/developer/enventry.h"

#include <QtTest>

static EnvEntry makeEntry(const QString &key, const QString &value,
                           const QString &mergeMode = QStringLiteral("replace"),
                           bool enabled = true)
{
    EnvEntry e;
    e.key       = key;
    e.value     = value;
    e.mergeMode = mergeMode;
    e.enabled   = enabled;
    return e;
}

class MergeResolverTest : public QObject
{
    Q_OBJECT

private slots:
    // ── basic resolution ──────────────────────────────────────────────────
    void empty_layers_produces_empty_env();
    void single_layer_single_entry();
    void higher_layer_wins_replace();
    void disabled_entry_is_skipped();

    // ── merge modes ───────────────────────────────────────────────────────
    void prepend_concatenates_with_colon();
    void append_concatenates_with_colon();
    void prepend_no_base_is_value_alone();
    void append_no_base_is_value_alone();

    // ── multi-layer ordering ──────────────────────────────────────────────
    void layers_sorted_automatically();
    void multiple_entries_different_keys();

    // ── applyEntry ────────────────────────────────────────────────────────
    void applyEntry_replace_overwrites();
    void applyEntry_skips_disabled();
};

// ── basic resolution ─────────────────────────────────────────────────────────

void MergeResolverTest::empty_layers_produces_empty_env()
{
    const ResolvedEnv env = MergeResolver::resolve({});
    QVERIFY(env.isEmpty());
}

void MergeResolverTest::single_layer_single_entry()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("MY_VAR"), QStringLiteral("hello"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.size(), 1);
    QCOMPARE(env.value(QStringLiteral("MY_VAR")).value, QStringLiteral("hello"));
    QCOMPARE(env.value(QStringLiteral("MY_VAR")).sourceLayer, EnvLayer::UserPersistent);
}

void MergeResolverTest::higher_layer_wins_replace()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("FOO"), QStringLiteral("user"))}},
        {EnvLayer::Session,        {makeEntry(QStringLiteral("FOO"), QStringLiteral("session"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("FOO")).value, QStringLiteral("session"));
    QCOMPARE(env.value(QStringLiteral("FOO")).sourceLayer, EnvLayer::Session);
}

void MergeResolverTest::disabled_entry_is_skipped()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("GHOST"), QStringLiteral("v"), QStringLiteral("replace"), false)}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QVERIFY(!env.contains(QStringLiteral("GHOST")));
}

// ── merge modes ───────────────────────────────────────────────────────────────

void MergeResolverTest::prepend_concatenates_with_colon()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("PATH"), QStringLiteral("/usr/bin"), QStringLiteral("replace"))}},
        {EnvLayer::Session,        {makeEntry(QStringLiteral("PATH"), QStringLiteral("/custom/bin"), QStringLiteral("prepend"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("PATH")).value, QStringLiteral("/custom/bin:/usr/bin"));
}

void MergeResolverTest::append_concatenates_with_colon()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("PATH"), QStringLiteral("/usr/bin"), QStringLiteral("replace"))}},
        {EnvLayer::Session,        {makeEntry(QStringLiteral("PATH"), QStringLiteral("/custom/bin"), QStringLiteral("append"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("PATH")).value, QStringLiteral("/usr/bin:/custom/bin"));
}

void MergeResolverTest::prepend_no_base_is_value_alone()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::Session, {makeEntry(QStringLiteral("NEW_PATH"), QStringLiteral("/only"), QStringLiteral("prepend"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("NEW_PATH")).value, QStringLiteral("/only"));
}

void MergeResolverTest::append_no_base_is_value_alone()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::Session, {makeEntry(QStringLiteral("NEW_PATH"), QStringLiteral("/only"), QStringLiteral("append"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("NEW_PATH")).value, QStringLiteral("/only"));
}

// ── multi-layer ordering ──────────────────────────────────────────────────────

void MergeResolverTest::layers_sorted_automatically()
{
    // Submit layers in reverse order; resolver must still process low-priority first.
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::Session,        {makeEntry(QStringLiteral("X"), QStringLiteral("session"))}},
        {EnvLayer::UserPersistent, {makeEntry(QStringLiteral("X"), QStringLiteral("user"))}}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.value(QStringLiteral("X")).value, QStringLiteral("session"));
}

void MergeResolverTest::multiple_entries_different_keys()
{
    const QList<MergeResolver::Layer> layers = {
        {EnvLayer::UserPersistent, {
             makeEntry(QStringLiteral("A"), QStringLiteral("1")),
             makeEntry(QStringLiteral("B"), QStringLiteral("2")),
         }}
    };
    const ResolvedEnv env = MergeResolver::resolve(layers);
    QCOMPARE(env.size(), 2);
    QCOMPARE(env.value(QStringLiteral("A")).value, QStringLiteral("1"));
    QCOMPARE(env.value(QStringLiteral("B")).value, QStringLiteral("2"));
}

// ── applyEntry ────────────────────────────────────────────────────────────────

void MergeResolverTest::applyEntry_replace_overwrites()
{
    ResolvedEnv env;
    env.insert(QStringLiteral("K"), {QStringLiteral("old"), EnvLayer::UserPersistent});
    MergeResolver::applyEntry(env, makeEntry(QStringLiteral("K"), QStringLiteral("new")),
                               EnvLayer::Session);
    QCOMPARE(env.value(QStringLiteral("K")).value, QStringLiteral("new"));
}

void MergeResolverTest::applyEntry_skips_disabled()
{
    ResolvedEnv env;
    MergeResolver::applyEntry(env,
                               makeEntry(QStringLiteral("K"), QStringLiteral("v"),
                                         QStringLiteral("replace"), false),
                               EnvLayer::Session);
    QVERIFY(!env.contains(QStringLiteral("K")));
}

QTEST_MAIN(MergeResolverTest)
#include "mergeresolver_test.moc"
