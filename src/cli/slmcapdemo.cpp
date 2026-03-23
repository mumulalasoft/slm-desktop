#include "../core/actions/slmactionregistry.h"
#include "../core/actions/framework/slmactionframework.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Slm::Actions::ActionRegistry registry;
    registry.reload();

    Slm::Actions::Framework::SlmActionFramework framework(&registry);
    framework.start();

    const QVariantMap context{
        {QStringLiteral("scope"), QStringLiteral("launcher")},
        {QStringLiteral("selection_count"), 0},
    };
    const QVariantList quickActions = framework.listActions(QStringLiteral("QuickAction"), context);
    qInfo().noquote() << "[slmcapdemo] quickActions=" << quickActions.size();
    for (int i = 0; i < quickActions.size() && i < 8; ++i) {
        const QVariantMap row = quickActions.at(i).toMap();
        qInfo().noquote() << " -" << row.value(QStringLiteral("name")).toString()
                          << "(" << row.value(QStringLiteral("id")).toString() << ")";
    }

    QVariantMap searchCtx{
        {QStringLiteral("scope"), QStringLiteral("tothespot")},
        {QStringLiteral("selection_count"), 0},
        {QStringLiteral("text"), QStringLiteral("convert")},
    };
    const QVariantList searchActions = framework.searchActions(QStringLiteral("convert"), searchCtx);
    qInfo().noquote() << "[slmcapdemo] searchActions=" << searchActions.size();
    for (int i = 0; i < searchActions.size() && i < 8; ++i) {
        const QVariantMap row = searchActions.at(i).toMap();
        qInfo().noquote() << " -" << row.value(QStringLiteral("name")).toString()
                          << "(" << row.value(QStringLiteral("id")).toString() << ")";
    }

    const QString home = QDir::homePath();
    QVariantMap dirCtx{
        {QStringLiteral("target"), QStringLiteral("directory")},
        {QStringLiteral("uris"), QStringList{QUrl::fromLocalFile(home).toString(QUrl::FullyEncoded)}},
        {QStringLiteral("selection_count"), 1},
    };
    const QVariantList dirMenu = framework.listActions(QStringLiteral("ContextMenu"), dirCtx);
    qInfo().noquote() << "[slmcapdemo] contextMenu(directory)=" << dirMenu.size();
    for (int i = 0; i < dirMenu.size() && i < 8; ++i) {
        const QVariantMap row = dirMenu.at(i).toMap();
        qInfo().noquote() << " -" << row.value(QStringLiteral("name")).toString()
                          << "(" << row.value(QStringLiteral("id")).toString() << ")";
    }

    QString sampleImagePath;
    QDirIterator it(home,
                    QStringList{QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.webp")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString candidate = it.next();
        if (QFileInfo(candidate).isReadable()) {
            sampleImagePath = candidate;
            break;
        }
    }
    if (!sampleImagePath.isEmpty()) {
        QVariantMap fileCtx{
            {QStringLiteral("target"), QStringLiteral("file")},
            {QStringLiteral("uris"), QStringList{QUrl::fromLocalFile(sampleImagePath).toString(QUrl::FullyEncoded)}},
            {QStringLiteral("selection_count"), 1},
        };
        const QVariantList fileMenu = framework.listActions(QStringLiteral("ContextMenu"), fileCtx);
        qInfo().noquote() << "[slmcapdemo] contextMenu(file)=" << fileMenu.size()
                          << "path=" << sampleImagePath;
        for (int i = 0; i < fileMenu.size() && i < 8; ++i) {
            const QVariantMap row = fileMenu.at(i).toMap();
            qInfo().noquote() << " -" << row.value(QStringLiteral("name")).toString()
                              << "(" << row.value(QStringLiteral("id")).toString() << ")";
        }
    } else {
        qInfo().noquote() << "[slmcapdemo] contextMenu(file)=skipped (no sample image found)";
    }
    return 0;
}
