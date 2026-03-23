#include <QtTest>

#include "../src/core/actions/slmactioncondition.h"
#include "../src/core/actions/slmactionregistry.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace Slm::Actions;

namespace {

QString writeTextFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&file);
    out << content;
    file.close();
    return path;
}

QVariantMap findNodeByLabel(const QVariantList &nodes, const QString &label)
{
    for (const QVariant &entry : nodes) {
        const QVariantMap node = entry.toMap();
        if (node.value(QStringLiteral("label")).toString() == label) {
            return node;
        }
    }
    return {};
}

bool containsActionLabel(const QVariantList &nodes, const QString &label)
{
    for (const QVariant &entry : nodes) {
        const QVariantMap node = entry.toMap();
        if (node.value(QStringLiteral("type")).toString() == QStringLiteral("action")) {
            const QVariantMap action = node.value(QStringLiteral("action")).toMap();
            if (action.value(QStringLiteral("name")).toString() == label
                || node.value(QStringLiteral("label")).toString() == label) {
                return true;
            }
        }
        const QVariantList children = node.value(QStringLiteral("children")).toList();
        if (!children.isEmpty() && containsActionLabel(children, label)) {
            return true;
        }
    }
    return false;
}

} // namespace

class SlmActionRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void acl_precedence();
    void acl_any_all_like_match_in();
    void acl_invalid_expression();
    void registry_filter_target_condition_and_sort();
    void registry_menu_tree_grouping();
    void registry_capability_native_openwith_and_search();
};

void SlmActionRegistryTest::acl_precedence()
{
    ActionConditionParser parser;
    ActionConditionEvaluator eval;
    const ConditionParseResult parsed = parser.parse(QStringLiteral("true OR false AND false"));
    QVERIFY2(parsed.ok, qPrintable(parsed.error));

    ActionEvalContext ctx;
    QVERIFY(eval.evaluate(parsed.root, ctx));
}

void SlmActionRegistryTest::acl_any_all_like_match_in()
{
    ActionConditionParser parser;
    ActionConditionEvaluator eval;
    const ConditionParseResult parsed = parser.parse(
        QStringLiteral("all(mime LIKE \"image/*\") AND any(ext IN (\"png\",\"jpg\")) AND count >= 2"));
    QVERIFY2(parsed.ok, qPrintable(parsed.error));

    ActionEvalContext ctx;
    ctx.count = 2;
    ActionItemMeta a;
    a.mime = QStringLiteral("image/png");
    a.ext = QStringLiteral("png");
    a.target = QStringLiteral("file");
    ActionItemMeta b;
    b.mime = QStringLiteral("image/jpeg");
    b.ext = QStringLiteral("jpg");
    b.target = QStringLiteral("file");
    ctx.items = {a, b};
    QVERIFY(eval.evaluate(parsed.root, ctx));

    const ConditionParseResult matchExpr = parser.parse(QStringLiteral("ext MATCH \"j.*g\""));
    QVERIFY2(matchExpr.ok, qPrintable(matchExpr.error));
    ActionEvalContext matchCtx;
    matchCtx.count = 1;
    ActionItemMeta c;
    c.ext = QStringLiteral("jpg");
    c.target = QStringLiteral("file");
    matchCtx.items = {c};
    QVERIFY(eval.evaluate(matchExpr.root, matchCtx));
}

void SlmActionRegistryTest::acl_invalid_expression()
{
    ActionConditionParser parser;
    const ConditionParseResult parsed = parser.parse(QStringLiteral("mime LIKE \"image/*\" AND (count == 1"));
    QVERIFY(!parsed.ok);
    QVERIFY(!parsed.error.isEmpty());
}

void SlmActionRegistryTest::registry_filter_target_condition_and_sort()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString txtPath = dir.path() + QStringLiteral("/note.txt");
    QVERIFY(!writeTextFile(txtPath, QStringLiteral("hello")).isEmpty());

    const QString desktopPath = dir.path() + QStringLiteral("/slm-test.desktop");
    const QString desktopContent = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=SLM Test\n"
        "Exec=slm-test %F\n"
        "MimeType=text/plain;image/png;\n"
        "Actions=OpenText;OpenImage;\n"
        "\n"
        "[Desktop Action OpenText]\n"
        "Name=Open Text Fast\n"
        "Exec=slm-test open-text %F\n"
        "X-SLM-Capabilities=ContextMenu;QuickAction\n"
        "X-SLM-ContextMenu=true\n"
        "X-SLM-FileAction-Targets=file\n"
        "X-SLM-Priority=5\n"
        "X-SLM-FileAction-Conditions=ext == \"txt\" AND count == 1\n"
        "\n"
        "[Desktop Action OpenImage]\n"
        "Name=Open Image Slow\n"
        "Exec=slm-test open-image %F\n"
        "X-SLM-Capabilities=ContextMenu\n"
        "X-SLM-ContextMenu=true\n"
        "X-SLM-FileAction-Targets=file\n"
        "X-SLM-Priority=50\n"
        "X-SLM-FileAction-Conditions=ext == \"png\"\n");
    QVERIFY(!writeTextFile(desktopPath, desktopContent).isEmpty());

    ActionRegistry registry;
    registry.setScanDirectories(QStringList{dir.path()});
    registry.reload();

    const QVariantList out = registry.listActions(
        QStringLiteral("ContextMenu"),
        QStringList{QStringLiteral("file://") + txtPath},
        QVariantMap{{QStringLiteral("target"), QStringLiteral("file")}});

    QVERIFY(out.size() >= 1);
    const QVariantMap row = out.first().toMap();
    QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("Open Text Fast"));
    QCOMPARE(row.value(QStringLiteral("priority")).toInt(), 5);
}

void SlmActionRegistryTest::registry_menu_tree_grouping()
{
    ActionRegistry registry;
    QVariantList flat;
    flat.push_back(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("a")},
        {QStringLiteral("name"), QStringLiteral("Convert to PNG")},
        {QStringLiteral("group"), QStringLiteral("Convert/Image")},
    });
    flat.push_back(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("b")},
        {QStringLiteral("name"), QStringLiteral("Compress")},
        {QStringLiteral("group"), QStringLiteral("Archive")},
    });

    const QVariantList tree = registry.buildMenuTree(flat);
    QVERIFY(!tree.isEmpty());

    const QVariantMap convert = findNodeByLabel(tree, QStringLiteral("Convert"));
    QVERIFY(!convert.isEmpty());
    const QVariantList convertChildren = convert.value(QStringLiteral("children")).toList();
    QVERIFY(!convertChildren.isEmpty());
    const QVariantMap image = findNodeByLabel(convertChildren, QStringLiteral("Image"));
    QVERIFY(!image.isEmpty());
    const QVariantList imageChildren = image.value(QStringLiteral("children")).toList();
    QVERIFY(!imageChildren.isEmpty());
    QVERIFY(containsActionLabel(imageChildren, QStringLiteral("Convert to PNG")));

    const QVariantMap archive = findNodeByLabel(tree, QStringLiteral("Archive"));
    QVERIFY(!archive.isEmpty());
    const QVariantList archiveChildren = archive.value(QStringLiteral("children")).toList();
    QVERIFY(!archiveChildren.isEmpty());
    QVERIFY(containsActionLabel(archiveChildren, QStringLiteral("Compress")));
}

void SlmActionRegistryTest::registry_capability_native_openwith_and_search()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString desktopPath = dir.path() + QStringLiteral("/slm-cap.desktop");
    const QString desktopContent = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=SLM Cap\n"
        "Exec=slm-cap %F\n"
        "MimeType=image/png;\n"
        "Actions=ConvertWebp;\n"
        "\n"
        "[Desktop Action ConvertWebp]\n"
        "Name=Convert to WebP\n"
        "Exec=slm-cap convert-webp %F\n"
        "X-SLM-Capabilities=SearchAction\n"
        "X-SLM-SearchAction-Scopes=launcher;tothespot\n"
        "X-SLM-SearchAction-Intent=convert-image\n"
        "X-SLM-Keywords=convert;image;webp\n"
        "X-SLM-Priority=30\n");
    QVERIFY(!writeTextFile(desktopPath, desktopContent).isEmpty());

    ActionRegistry registry;
    registry.setScanDirectories(QStringList{dir.path()});
    registry.reload();

    const QStringList uris{QStringLiteral("file:///tmp/image.png")};
    const QVariantMap context{
        {QStringLiteral("uris"), uris},
        {QStringLiteral("target"), QStringLiteral("file")},
        {QStringLiteral("scope"), QStringLiteral("tothespot")},
        {QStringLiteral("selection_count"), 1},
    };

    const QVariantList openWithRows = registry.listActionsWithContext(QStringLiteral("OpenWith"), context);
    QVERIFY(openWithRows.isEmpty());
    const QVariantMap resolved = registry.resolveDefaultAction(QStringLiteral("OpenWith"), context);
    QVERIFY(!resolved.value(QStringLiteral("ok")).toBool());
    QCOMPARE(resolved.value(QStringLiteral("error")).toString(),
             QStringLiteral("unsupported-capability"));

    const QVariantList search = registry.searchActions(QStringLiteral("convert"), context);
    QVERIFY(!search.isEmpty());
    const QVariantMap top = search.first().toMap();
    QCOMPARE(top.value(QStringLiteral("name")).toString(), QStringLiteral("Convert to WebP"));
    QVERIFY(top.value(QStringLiteral("score")).toInt() > 0);
}

QTEST_MAIN(SlmActionRegistryTest)
#include "slmactionregistry_test.moc"
