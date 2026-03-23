#include <QtTest>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include "../screenshotsavehelper.h"

class ScreenshotSaveHelperTest : public QObject
{
    Q_OBJECT

private slots:
    void validateFileName_rejectsInvalid()
    {
        ScreenshotSaveHelper helper;
        QCOMPARE(helper.validateFileName(QStringLiteral("")), QStringLiteral("Filename is empty"));
        QCOMPARE(helper.validateFileName(QStringLiteral("   ")), QStringLiteral("Filename is empty"));
        QCOMPARE(helper.validateFileName(QStringLiteral(".")), QStringLiteral("Invalid filename"));
        QCOMPARE(helper.validateFileName(QStringLiteral("..")), QStringLiteral("Invalid filename"));
        QCOMPARE(helper.validateFileName(QStringLiteral("shot.")), QStringLiteral("Invalid filename"));
        QCOMPARE(helper.validateFileName(QStringLiteral("shot.   ")), QStringLiteral("Invalid filename"));
        QCOMPARE(helper.validateFileName(QStringLiteral("a:b")), QStringLiteral("Filename contains invalid characters"));
        QCOMPARE(helper.validateFileName(QStringLiteral("a/b")), QStringLiteral("Filename contains invalid characters"));
    }

    void validateFileName_acceptsValid()
    {
        ScreenshotSaveHelper helper;
        QCOMPARE(helper.validateFileName(QStringLiteral("Screenshot 2026-03-01")), QString());
        QCOMPARE(helper.validateFileName(QStringLiteral("test_file.png")), QString());
    }

    void ensurePngFileName_appendsSuffix()
    {
        ScreenshotSaveHelper helper;
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("capture")), QStringLiteral("capture.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("capture.PNG")), QStringLiteral("capture.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("   ")), QStringLiteral("Screenshot.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral(".")), QStringLiteral("Screenshot.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("..")), QStringLiteral("Screenshot.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("shot.")), QStringLiteral("Screenshot.png"));
        QCOMPARE(helper.ensurePngFileName(QStringLiteral("shot.   ")), QStringLiteral("Screenshot.png"));
    }

    void ensureFileNameForExtension_normalizesForSelectedType()
    {
        ScreenshotSaveHelper helper;
        QCOMPARE(helper.ensureFileNameForExtension(QStringLiteral("capture.png"), QStringLiteral("jpg")),
                 QStringLiteral("capture.jpg"));
        QCOMPARE(helper.ensureFileNameForExtension(QStringLiteral("capture"), QStringLiteral(".tiff")),
                 QStringLiteral("capture.tiff"));
        QCOMPARE(helper.ensureFileNameForExtension(QStringLiteral("  "), QStringLiteral("bmp")),
                 QStringLiteral("Screenshot.bmp"));
    }

    void buildTargetPath_joinsFolderAndFile()
    {
        ScreenshotSaveHelper helper;
        QCOMPARE(helper.buildTargetPath(QStringLiteral("~/Pictures/Screenshots"), QStringLiteral("shot")),
                 QStringLiteral("~/Pictures/Screenshots/shot.png"));
        QCOMPARE(helper.buildTargetPath(QStringLiteral("~/Pictures/Screenshots/"), QStringLiteral("shot.png")),
                 QStringLiteral("~/Pictures/Screenshots/shot.png"));
        QCOMPARE(helper.buildTargetPath(QStringLiteral("~/Pictures/Screenshots"), QStringLiteral("shot.   ")),
                 QStringLiteral("~/Pictures/Screenshots/Screenshot.png"));
    }

    void overwritePrompt_decisionPath()
    {
        ScreenshotSaveHelper helper;
        QVERIFY(helper.shouldPromptOverwrite(true, false));
        QVERIFY(!helper.shouldPromptOverwrite(false, false));
        QVERIFY(!helper.shouldPromptOverwrite(true, true));
    }

    void chooserRoundtrip_folderAndResumeState()
    {
        ScreenshotSaveHelper helper;

        const QString currentFolder = QStringLiteral("~/Pictures/Screenshots");
        const QVariantList selectedPaths{QStringLiteral("/home/garis/Desktop")};
        QCOMPARE(helper.resolveChosenFolder(currentFolder, selectedPaths, true),
                 QStringLiteral("/home/garis/Desktop"));

        const QVariantList emptySelection;
        QCOMPARE(helper.resolveChosenFolder(currentFolder, emptySelection, true), currentFolder);
        QCOMPARE(helper.resolveChosenFolder(currentFolder, selectedPaths, false), currentFolder);

        QVERIFY(helper.shouldResumeSaveDialog(QStringLiteral("/tmp/shot.png"), true));
        QVERIFY(helper.shouldResumeSaveDialog(QStringLiteral("/tmp/shot.png"), false));
        QVERIFY(!helper.shouldResumeSaveDialog(QStringLiteral("   "), true));
    }

    void saveImageFile_pngRenameRoundtrip()
    {
        ScreenshotSaveHelper helper;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString srcPath = dir.path() + QStringLiteral("/src.png");
        const QString dstPath = dir.path() + QStringLiteral("/nested/out.png");

        QImage img(8, 8, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::red);
        QVERIFY(img.save(srcPath));

        const QVariantMap res = helper.saveImageFile(srcPath, dstPath, QStringLiteral("png"), -1, false);
        QVERIFY(res.value(QStringLiteral("ok")).toBool());
        QCOMPARE(res.value(QStringLiteral("error")).toString(), QString());
        QVERIFY(QFileInfo::exists(dstPath));
        QVERIFY(!QFileInfo::exists(srcPath));
    }

    void saveImageFile_nonPngConvertRoundtrip()
    {
        ScreenshotSaveHelper helper;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString srcPath = dir.path() + QStringLiteral("/src.png");
        const QString dstPath = dir.path() + QStringLiteral("/out.jpg");

        QImage img(12, 12, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::blue);
        QVERIFY(img.save(srcPath));

        const QVariantMap res = helper.saveImageFile(srcPath, dstPath, QStringLiteral("jpg"), 85, true);
        QVERIFY(res.value(QStringLiteral("ok")).toBool());
        QCOMPARE(res.value(QStringLiteral("error")).toString(), QString());
        QVERIFY(QFileInfo::exists(dstPath));
        QVERIFY(!QFileInfo::exists(srcPath));
    }

    void saveImageFile_missingSource()
    {
        ScreenshotSaveHelper helper;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QVariantMap res = helper.saveImageFile(dir.path() + QStringLiteral("/missing.png"),
                                                     dir.path() + QStringLiteral("/out.png"),
                                                     QStringLiteral("png"),
                                                     -1,
                                                     false);
        QVERIFY(!res.value(QStringLiteral("ok")).toBool());
        QCOMPARE(res.value(QStringLiteral("error")).toString(), QStringLiteral("source-missing"));
    }
};

QTEST_MAIN(ScreenshotSaveHelperTest)
#include "screenshotsavehelper_test.moc"
