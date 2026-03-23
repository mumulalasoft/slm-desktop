#include <QtTest>

#include "../src/core/actions/framework/acl/slmacllexer.h"
#include "../src/core/actions/framework/acl/slmaclparser.h"
#include "../src/core/actions/framework/slmgrouptreebuilder.h"
#include "../src/core/actions/slmactionregistry.h"
#include "../src/core/actions/slmdesktopactionparser.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace Slm::Actions;
using namespace Slm::Actions::Framework;
namespace SFAcl = Slm::Actions::Framework::Acl;

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

} // namespace

class SlmCapabilityFrameworkTest : public QObject
{
    Q_OBJECT

private slots:
    void desktop_parsing();
    void acl_parser_and_evaluator();
    void capability_filtering();
    void grouping_builder();
    void search_ranking();
};

void SlmCapabilityFrameworkTest::desktop_parsing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString desktopPath = dir.path() + QStringLiteral("/cap.desktop");
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=Cap\n"
        "Exec=cap %F\n"
        "MimeType=image/png;text/plain;\n"
        "Actions=ShareImage;\n"
        "\n"
        "[Desktop Action ShareImage]\n"
        "Name=Share Image\n"
        "Exec=cap share %F\n"
        "X-SLM-Capabilities=Share\n"
        "X-SLM-Share-Targets=file\n"
        "X-SLM-Share-Modes=send;link\n"
        "X-SLM-Group=Share/Image\n");
    QVERIFY(!writeTextFile(desktopPath, content).isEmpty());

    DesktopActionParser parser;
    const DesktopParseResult result = parser.parseFile(desktopPath);
    QVERIFY(result.ok);
    QCOMPARE(result.actions.size(), 1);
    const FileAction action = result.actions.first();
    QVERIFY(action.capabilities.contains(Capability::Share));
    QCOMPARE(action.group, QStringLiteral("Share/Image"));
}

void SlmCapabilityFrameworkTest::acl_parser_and_evaluator()
{
    SFAcl::Parser parser;
    const SFAcl::ParseResult parsed = parser.parse(QStringLiteral("(count >= 1 AND writable) OR target == \"file\""));
    QVERIFY(parsed.ok);
    QVERIFY(parsed.root != nullptr);

    Slm::Actions::Framework::ActionContext ctx;
    ctx.selectionCount = 1;
    ctx.writable = true;
    ctx.target = QStringLiteral("file");

    SFAcl::Evaluator evaluator;
    QVERIFY(evaluator.evaluate(parsed.root, ctx));
}

void SlmCapabilityFrameworkTest::capability_filtering()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString desktopPath = dir.path() + QStringLiteral("/share.desktop");
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=Share\n"
        "Exec=share %F\n"
        "MimeType=text/plain;\n"
        "Actions=ShareSend;Quick;\n"
        "\n"
        "[Desktop Action ShareSend]\n"
        "Name=Share Send\n"
        "Exec=share send %F\n"
        "X-SLM-Capabilities=Share\n"
        "X-SLM-Share-Targets=file\n"
        "X-SLM-FileAction-Targets=file\n"
        "\n"
        "[Desktop Action Quick]\n"
        "Name=Quick\n"
        "Exec=share quick %F\n"
        "X-SLM-Capabilities=QuickAction\n"
        "X-SLM-QuickAction-Scopes=launcher\n");
    QVERIFY(!writeTextFile(desktopPath, content).isEmpty());

    ActionRegistry registry;
    registry.setScanDirectories(QStringList{dir.path()});
    registry.reload();

    QVariantMap ctx{
        {QStringLiteral("uris"), QStringList{QStringLiteral("file:///tmp/a.txt")}},
        {QStringLiteral("target"), QStringLiteral("file")},
        {QStringLiteral("selection_count"), 1},
    };
    const QVariantList share = registry.listActionsWithContext(QStringLiteral("Share"), ctx);
    QCOMPARE(share.size(), 1);
    QCOMPARE(share.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Share Send"));
}

void SlmCapabilityFrameworkTest::grouping_builder()
{
    QList<ResolvedAction> rows;
    ResolvedAction a;
    a.actionId = QStringLiteral("1");
    a.name = QStringLiteral("Convert to WebP");
    a.groupPath = {QStringLiteral("Convert"), QStringLiteral("Image")};
    a.score = 10.0;
    rows.push_back(a);

    ResolvedAction b;
    b.actionId = QStringLiteral("2");
    b.name = QStringLiteral("Compress");
    b.groupPath = {QStringLiteral("Archive")};
    b.score = 5.0;
    rows.push_back(b);

    GroupTreeBuilder builder;
    const GroupTreeNode root = builder.build(rows);
    QVERIFY(!root.children.isEmpty());
}

void SlmCapabilityFrameworkTest::search_ranking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString desktopPath = dir.path() + QStringLiteral("/search.desktop");
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Name=Search\n"
        "Exec=search %F\n"
        "Actions=Convert;Resize;\n"
        "\n"
        "[Desktop Action Convert]\n"
        "Name=Convert to WebP\n"
        "Exec=search convert %F\n"
        "X-SLM-Capabilities=SearchAction\n"
        "X-SLM-SearchAction-Scopes=tothespot\n"
        "X-SLM-SearchAction-Intent=convert-image\n"
        "X-SLM-Keywords=convert;webp;image\n"
        "X-SLM-Priority=10\n"
        "\n"
        "[Desktop Action Resize]\n"
        "Name=Resize Image\n"
        "Exec=search resize %F\n"
        "X-SLM-Capabilities=SearchAction\n"
        "X-SLM-SearchAction-Scopes=tothespot\n"
        "X-SLM-SearchAction-Intent=resize-image\n"
        "X-SLM-Keywords=resize;image\n"
        "X-SLM-Priority=50\n");
    QVERIFY(!writeTextFile(desktopPath, content).isEmpty());

    ActionRegistry registry;
    registry.setScanDirectories(QStringList{dir.path()});
    registry.reload();

    const QVariantMap context{
        {QStringLiteral("scope"), QStringLiteral("tothespot")},
    };
    const QVariantList result = registry.searchActions(QStringLiteral("convert"), context);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Convert to WebP"));
}

QTEST_MAIN(SlmCapabilityFrameworkTest)
#include "slmcapability_framework_test.moc"
