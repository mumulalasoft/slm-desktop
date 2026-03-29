#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/services/archive/archivebackend.h"

extern "C" {
#include <archive.h>
#include <archive_entry.h>
}

namespace {

struct TarEntry {
    QString path;
    QByteArray payload;
    mode_t mode = 0644;
    int fileType = AE_IFREG;
    QString symlinkTarget;
};

static bool createTarArchive(const QString &archivePath, const QList<TarEntry> &entries)
{
    archive *writer = archive_write_new();
    if (!writer) {
        return false;
    }
    archive_write_set_format_pax_restricted(writer);
    if (archive_write_open_filename(writer, archivePath.toUtf8().constData()) != ARCHIVE_OK) {
        archive_write_free(writer);
        return false;
    }

    for (const TarEntry &row : entries) {
        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, row.path.toUtf8().constData());
        archive_entry_set_filetype(entry, row.fileType);
        archive_entry_set_perm(entry, row.mode);
        archive_entry_set_size(entry, row.fileType == AE_IFREG ? row.payload.size() : 0);
        if (row.fileType == AE_IFLNK && !row.symlinkTarget.isEmpty()) {
            archive_entry_set_symlink(entry, row.symlinkTarget.toUtf8().constData());
        }

        if (archive_write_header(writer, entry) != ARCHIVE_OK) {
            archive_entry_free(entry);
            archive_write_close(writer);
            archive_write_free(writer);
            return false;
        }
        if (row.fileType == AE_IFREG && !row.payload.isEmpty()) {
            if (archive_write_data(writer, row.payload.constData(), row.payload.size()) < 0) {
                archive_entry_free(entry);
                archive_write_close(writer);
                archive_write_free(writer);
                return false;
            }
        }
        archive_entry_free(entry);
    }

    archive_write_close(writer);
    archive_write_free(writer);
    return true;
}

} // namespace

class ArchiveBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void listArchive_detectsSingleRootFolder();
    void extractArchive_blocksTraversalEntry();
    void extractArchive_skipsSymlinkEntry();
    void extractArchive_blocksSuspiciousExpansionRatio();
    void compressArchive_enforcesEntryLimit();
};

void ArchiveBackendTest::listArchive_detectsSingleRootFolder()
{
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), "temporary directory must be valid");

    const QString rootDir = QDir(tmp.path()).filePath(QStringLiteral("Payload"));
    QVERIFY(QDir().mkpath(QDir(rootDir).filePath(QStringLiteral("Documents"))));

    QFile f(QDir(rootDir).filePath(QStringLiteral("Documents/notes.txt")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.write("hello") == 5);
    f.close();

    const QString archivePath = QDir(tmp.path()).filePath(QStringLiteral("payload.zip"));
    const QVariantMap compressed = Slm::Archive::ArchiveBackend::compressPaths(
        QStringList{rootDir}, archivePath, QStringLiteral("zip"));
    QVERIFY2(compressed.value(QStringLiteral("ok")).toBool(), "compressPaths should succeed");

    const QVariantMap listed = Slm::Archive::ArchiveBackend::listArchive(archivePath, 2048);
    QVERIFY2(listed.value(QStringLiteral("ok")).toBool(), "listArchive should succeed");
    QCOMPARE(listed.value(QStringLiteral("layout")).toString(), QStringLiteral("single_root_folder"));
}

void ArchiveBackendTest::extractArchive_blocksTraversalEntry()
{
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), "temporary directory must be valid");

    const QString archivePath = QDir(tmp.path()).filePath(QStringLiteral("payload.tar"));
    QVERIFY(createTarArchive(archivePath, {
        TarEntry{QStringLiteral("../evil.txt"), QByteArray("pwnd")},
        TarEntry{QStringLiteral("safe.txt"), QByteArray("safe")}
    }));

    const QString extractRoot = QDir(tmp.path()).filePath(QStringLiteral("out"));
    const QVariantMap result = Slm::Archive::ArchiveBackend::extractArchive(
        archivePath,
        extractRoot,
        QStringLiteral("extract_sibling_default"),
        QStringLiteral("auto_rename"));
    QVERIFY2(result.value(QStringLiteral("ok")).toBool(), "extractArchive should succeed");

    const QString extractedFolder = result.value(QStringLiteral("destination")).toString();
    QVERIFY(!extractedFolder.isEmpty());
    QVERIFY(QFileInfo::exists(QDir(extractedFolder).filePath(QStringLiteral("safe.txt"))));
    QVERIFY(!QFileInfo::exists(QDir(extractRoot).filePath(QStringLiteral("evil.txt"))));
}

void ArchiveBackendTest::extractArchive_skipsSymlinkEntry()
{
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), "temporary directory must be valid");

    const QString archivePath = QDir(tmp.path()).filePath(QStringLiteral("symlink.tar"));
    QVERIFY(createTarArchive(archivePath, {
        TarEntry{QStringLiteral("regular.txt"), QByteArray("hello")},
        TarEntry{
            QStringLiteral("bad-link"),
            QByteArray(),
            0777,
            AE_IFLNK,
            QStringLiteral("/etc/passwd")
        }
    }));

    const QString extractRoot = QDir(tmp.path()).filePath(QStringLiteral("out"));
    const QVariantMap result = Slm::Archive::ArchiveBackend::extractArchive(
        archivePath,
        extractRoot,
        QStringLiteral("extract_sibling_default"),
        QStringLiteral("auto_rename"));
    QVERIFY2(result.value(QStringLiteral("ok")).toBool(), "extractArchive should succeed");

    const QString extractedFolder = result.value(QStringLiteral("destination")).toString();
    QVERIFY(!extractedFolder.isEmpty());
    QVERIFY(QFileInfo::exists(QDir(extractedFolder).filePath(QStringLiteral("regular.txt"))));
    QVERIFY(!QFileInfo::exists(QDir(extractedFolder).filePath(QStringLiteral("bad-link"))));
}

void ArchiveBackendTest::extractArchive_blocksSuspiciousExpansionRatio()
{
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), "temporary directory must be valid");

    const QString srcFile = QDir(tmp.path()).filePath(QStringLiteral("blob.txt"));
    QFile f(srcFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    const QByteArray largeChunk(512 * 1024, '\0');
    QVERIFY(f.write(largeChunk) == largeChunk.size());
    f.close();

    const QString archivePath = QDir(tmp.path()).filePath(QStringLiteral("blob.zip"));
    const QVariantMap compressed = Slm::Archive::ArchiveBackend::compressPaths(
        QStringList{srcFile}, archivePath, QStringLiteral("zip"));
    QVERIFY2(compressed.value(QStringLiteral("ok")).toBool(), "compressPaths should succeed");

    qputenv("SLM_ARCHIVE_MAX_EXPAND_RATIO", QByteArray("1"));
    const QString extractRoot = QDir(tmp.path()).filePath(QStringLiteral("out"));
    const QVariantMap extracted = Slm::Archive::ArchiveBackend::extractArchive(
        archivePath,
        extractRoot,
        QStringLiteral("extract_sibling_default"),
        QStringLiteral("auto_rename"));
    qunsetenv("SLM_ARCHIVE_MAX_EXPAND_RATIO");

    QVERIFY(!extracted.value(QStringLiteral("ok")).toBool());
    QCOMPARE(extracted.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("ERR_RESOURCE_LIMIT"));
}

void ArchiveBackendTest::compressArchive_enforcesEntryLimit()
{
    QTemporaryDir tmp;
    QVERIFY2(tmp.isValid(), "temporary directory must be valid");

    const QString dirPath = QDir(tmp.path()).filePath(QStringLiteral("payload"));
    QVERIFY(QDir().mkpath(dirPath));
    for (int i = 0; i < 3; ++i) {
        QFile f(QDir(dirPath).filePath(QStringLiteral("f%1.txt").arg(i)));
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("x") == 1);
        f.close();
    }

    qputenv("SLM_ARCHIVE_MAX_ENTRIES", QByteArray("2"));
    const QString archivePath = QDir(tmp.path()).filePath(QStringLiteral("out.zip"));
    const QVariantMap compressed = Slm::Archive::ArchiveBackend::compressPaths(
        QStringList{dirPath}, archivePath, QStringLiteral("zip"));
    qunsetenv("SLM_ARCHIVE_MAX_ENTRIES");

    QVERIFY(!compressed.value(QStringLiteral("ok")).toBool());
    QCOMPARE(compressed.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("ERR_RESOURCE_LIMIT"));
}

QTEST_MAIN(ArchiveBackendTest)

#include "archivebackend_test.moc"
