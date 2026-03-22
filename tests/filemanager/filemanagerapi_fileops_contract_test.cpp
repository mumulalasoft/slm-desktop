#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "../../src/apps/filemanager/include/filemanagerapi.h"

class FileManagerApiFileOpsContractTest : public QObject
{
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &content)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        if (f.write(content) != content.size()) {
            return false;
        }
        f.close();
        return true;
    }

    static QByteArray readFile(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        return f.readAll();
    }

private slots:
    void overwriteCancel_keepsExistingTargetAndSource()
    {
        FileManagerApi api;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("shot-new.png"));
        const QString targetPath = dir.filePath(QStringLiteral("shot.png"));
        QVERIFY(writeFile(sourcePath, QByteArray("new-content")));
        QVERIFY(writeFile(targetPath, QByteArray("old-content")));

        // Contract: without explicit overwrite remove step, rename must fail when target exists.
        const QVariantMap rename = api.renamePath(sourcePath, targetPath);
        QVERIFY(!rename.value(QStringLiteral("ok")).toBool());
        QCOMPARE(rename.value(QStringLiteral("error")).toString(), QStringLiteral("rename-failed"));

        const QVariantMap srcStat = api.statPath(sourcePath);
        const QVariantMap dstStat = api.statPath(targetPath);
        QVERIFY(srcStat.value(QStringLiteral("ok")).toBool());
        QVERIFY(dstStat.value(QStringLiteral("ok")).toBool());
        QCOMPARE(readFile(targetPath), QByteArray("old-content"));
        QCOMPARE(readFile(sourcePath), QByteArray("new-content"));
    }

    void overwriteReplace_removesTargetThenReplaces()
    {
        FileManagerApi api;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("shot-new.png"));
        const QString targetPath = dir.filePath(QStringLiteral("shot.png"));
        QVERIFY(writeFile(sourcePath, QByteArray("new-content")));
        QVERIFY(writeFile(targetPath, QByteArray("old-content")));

        const QVariantMap removed = api.removePath(targetPath, false);
        QVERIFY(removed.value(QStringLiteral("ok")).toBool());

        const QVariantMap rename = api.renamePath(sourcePath, targetPath);
        QVERIFY(rename.value(QStringLiteral("ok")).toBool());

        const QVariantMap srcStat = api.statPath(sourcePath);
        const QVariantMap dstStat = api.statPath(targetPath);
        QVERIFY(!srcStat.value(QStringLiteral("ok")).toBool());
        QVERIFY(dstStat.value(QStringLiteral("ok")).toBool());
        QCOMPARE(readFile(targetPath), QByteArray("new-content"));
    }
};

QTEST_MAIN(FileManagerApiFileOpsContractTest)
#include "filemanagerapi_fileops_contract_test.moc"
