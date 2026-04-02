// tests/filemanager/filemanager_shell_contract_test.cpp
//
// Contract tests untuk integrasi FileManagerShellBridge dengan desktop shell.
//
// Test ini TIDAK mock komponen filemanager — ia verifikasi bahwa contract
// interface (DBus, CMake target, C++ public API) terpenuhi dengan
// implementasi nyata.
//
// Prasyarat:
//   - slm-fileopsd harus running (di-start oleh initTestCase)
//   - SlmFileManager::Core harus available (dikompilasi sebagai dependency)

#include <QtTest/QtTest>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QStandardPaths>

// Public API yang DIPERBOLEHKAN dari shell
#include "src/apps/filemanager/include/filemanagerapi.h"
#include "src/filemanager/FileManagerShellBridge.h"

// ── Konstanta DBus ────────────────────────────────────────────────────────────

static constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
static constexpr const char kFileOpsPath[]    = "/org/slm/Desktop/FileOperations";
static constexpr const char kFileOpsIface[]   = "org.slm.Desktop.FileOperations";

// ── Test class ────────────────────────────────────────────────────────────────

class FileManagerShellContractTest : public QObject
{
    Q_OBJECT

private:
    QProcess *m_fileopsd = nullptr;
    FileManagerApi *m_api = nullptr;
    FileManagerShellBridge *m_bridge = nullptr;

private slots:
    // ── Setup / Teardown ──────────────────────────────────────────────────

    void initTestCase()
    {
        // Resolve binary fileopsd dari env override atau PATH
        QString fileopsdBin = qEnvironmentVariable("SLM_FILEOPSD_BINARY");
        if (fileopsdBin.isEmpty())
            fileopsdBin = QStandardPaths::findExecutable(QStringLiteral("slm-fileopsd"));

        QVERIFY2(!fileopsdBin.isEmpty(),
            "slm-fileopsd tidak ditemukan. Set SLM_FILEOPSD_BINARY atau install.");

        // Jika sudah running, skip start
        QDBusInterface probe(QString::fromLatin1(kFileOpsService),
                             QString::fromLatin1(kFileOpsPath),
                             QString::fromLatin1(kFileOpsIface));
        if (!probe.isValid()) {
            m_fileopsd = new QProcess(this);
            m_fileopsd->start(fileopsdBin, {});
            QVERIFY2(m_fileopsd->waitForStarted(3000), "fileopsd gagal start");
            QTest::qWait(600); // tunggu DBus registration
        }

        m_api = new FileManagerApi(this);
        m_bridge = new FileManagerShellBridge(m_api, this);
    }

    void cleanupTestCase()
    {
        if (m_fileopsd) {
            m_fileopsd->terminate();
            m_fileopsd->waitForFinished(2000);
        }
    }

    // ── Contract: DBus interface tersedia ─────────────────────────────────

    void test_FileOpsDBusInterfacePresent()
    {
        QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                             QString::fromLatin1(kFileOpsPath),
                             QString::fromLatin1(kFileOpsIface));
        QVERIFY2(iface.isValid(),
            "org.slm.Desktop.FileOperations DBus interface harus tersedia");
    }

    void test_FileOpsPingReturnsValidMap()
    {
        QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                             QString::fromLatin1(kFileOpsPath),
                             QString::fromLatin1(kFileOpsIface));
        auto reply = iface.call(QStringLiteral("Ping"));
        QVERIFY2(reply.type() != QDBusMessage::ErrorMessage,
                 qPrintable(reply.errorMessage()));
        QVERIFY(!reply.arguments().isEmpty());
    }

    void test_FileOpsServiceRegisteredProperty()
    {
        QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                             QString::fromLatin1(kFileOpsPath),
                             QString::fromLatin1(kFileOpsIface));
        const bool registered = iface.property("serviceRegistered").toBool();
        QVERIFY2(registered,
            "serviceRegistered property harus true ketika fileopsd running");
    }

    void test_FileOpsApiVersionProperty()
    {
        QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                             QString::fromLatin1(kFileOpsPath),
                             QString::fromLatin1(kFileOpsIface));
        const QString version = iface.property("apiVersion").toString();
        QVERIFY2(!version.isEmpty(),
            "apiVersion property tidak boleh kosong");
    }

    // ── Contract: SlmFileManager::Core API tersedia ───────────────────────

    void test_FileManagerApiListDirectory()
    {
        QVERIFY(m_api);
        const auto result = m_api->listDirectory(QStringLiteral("/tmp"),
                                                  false, true);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
            "listDirectory /tmp harus berhasil");
        QVERIFY(result.contains(QStringLiteral("entries")));
    }

    void test_FileManagerApiStatPath()
    {
        QVERIFY(m_api);
        const auto result = m_api->statPath(QStringLiteral("/tmp"));
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
            "statPath /tmp harus berhasil");
        QVERIFY(result.value(QStringLiteral("isDir")).toBool());
    }

    void test_FileManagerApiSearchDirectory()
    {
        QVERIFY(m_api);
        // search dengan query kosong harus return hasil (semua file di /tmp)
        const auto result = m_api->searchDirectory(
            QStringLiteral("/tmp"), QStringLiteral(""), false, true, 10, 0);
        QVERIFY(result.contains(QStringLiteral("entries")));
        // tidak harus ada hasil, tapi tidak boleh crash atau missing key
    }

    // ── Contract: FileManagerShellBridge methods tersedia ─────────────────

    void test_BridgeSidebarPlaces_NoError()
    {
        QVERIFY(m_bridge);
        // sidebarPlaces() boleh return list kosong, tapi tidak boleh crash
        const auto places = m_bridge->sidebarPlaces();
        QVERIFY(places.isEmpty() || !places.isEmpty()); // selalu true, tapi exercised
    }

    void test_BridgeTrashItemCount_NonNegative()
    {
        QVERIFY(m_bridge);
        QVERIFY(m_bridge->trashItemCount() >= 0);
    }

    // ── Contract: file copy via bridge → fileopsd ─────────────────────────

    void test_BridgeStartCopy_ReturnsOperationId()
    {
        QVERIFY(m_bridge);
        QVERIFY2(m_bridge->fileOpsAvailable(),
            "fileOpsAvailable harus true — fileopsd harus running");

        QTemporaryDir srcDir, dstDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(dstDir.isValid());

        // Buat file sumber
        QFile f(srcDir.filePath(QStringLiteral("contract_test.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("slm-filemanager contract test\n");
        f.close();

        // Panggil copy via bridge
        const QString opId = m_bridge->startCopy(
            QStringList{f.fileName()},
            dstDir.path());

        QVERIFY2(!opId.isEmpty(),
            "startCopy harus return operation ID non-kosong");

        // Tunggu operasi selesai (max 5 detik)
        QSignalSpy finished(m_bridge,
            &FileManagerShellBridge::operationFinished);
        QVERIFY(finished.wait(5000));

        const auto args = finished.first();
        QCOMPARE(args[0].toString(), opId);
        QCOMPARE(args[1].toBool(), true); // success

        // Verifikasi file ter-copy
        QVERIFY(QFile::exists(dstDir.filePath(
            QStringLiteral("contract_test.txt"))));
    }

    // ── Contract: search via bridge (in-process, non-blocking) ───────────

    void test_BridgeSearchFiles_EmitsResults()
    {
        QVERIFY(m_bridge);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Buat beberapa file
        for (const auto &name : {
                 "alpha.txt", "beta.txt", "gamma.conf"}) {
            QFile f(dir.filePath(QLatin1String(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("test");
            f.close();
        }

        QSignalSpy results(m_bridge,
            &FileManagerShellBridge::searchResultsAvailable);
        QSignalSpy finished(m_bridge,
            &FileManagerShellBridge::searchFinished);

        const QString sessionId = QStringLiteral("test-search-001");
        m_bridge->searchFiles(dir.path(), QStringLiteral("*.txt"), sessionId);

        // Tunggu selesai (max 3 detik)
        QVERIFY(finished.wait(3000));

        // Harus ada signal results
        QVERIFY2(!results.isEmpty(),
            "searchResultsAvailable harus dipancarkan");

        // sessionId harus match
        QCOMPARE(finished.first().first().toString(), sessionId);
    }

    // ── Contract: trash empty via bridge → fileopsd ───────────────────────

    void test_BridgeEmptyTrash_NoError()
    {
        QVERIFY(m_bridge);
        if (!m_bridge->fileOpsAvailable()) QSKIP("fileopsd tidak tersedia");

        // emptyTrash() adalah fire-and-forget, tidak boleh crash
        m_bridge->emptyTrash();
        QTest::qWait(200); // beri waktu dispatch DBus
    }
};

QTEST_GUILESS_MAIN(FileManagerShellContractTest)
#include "filemanager_shell_contract_test.moc"
