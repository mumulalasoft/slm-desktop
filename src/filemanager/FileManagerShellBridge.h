#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QDBusInterface>
#include <QFuture>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logFmBridge)

class FileManagerApi;

/**
 * FileManagerShellBridge
 *
 * Satu-satunya titik kontak antara desktop shell dan filemanager.
 * Menyatukan dua jalur integrasi:
 *
 *   1. In-process (via FileManagerApi):
 *      - getSidebarPlaces()      → GIO bookmarks + mount points
 *      - trashItemCount()        → GIO trash:// item count
 *      - searchFiles()           → FileManagerApi::searchDirectory()
 *
 *   2. Cross-process (via DBus ke fileopsd dan filemanager app):
 *      - openPath / revealItem   → org.slm.Desktop.FileManager1
 *      - startCopy / startMove   → org.slm.Desktop.FileOperations
 *      - emptyTrash              → org.slm.Desktop.FileOperations
 *      - mountVolume             → GIO via FileManagerApi (in-process)
 *
 * Shell TIDAK boleh menggunakan FileManagerApi atau FileOperationsService
 * langsung — semua akses melalui bridge ini agar contract terisolasi.
 *
 * Graceful degradation: semua metode ini aman dipanggil meski filemanager
 * tidak running. Fallback: log warning, return empty/false.
 */
class FileManagerShellBridge : public QObject
{
    Q_OBJECT

    // Trash item count — dapat di-bind langsung dari QML
    Q_PROPERTY(int trashItemCount READ trashItemCount NOTIFY trashCountChanged)

    // Apakah filemanager app sedang tersedia via DBus
    Q_PROPERTY(bool fileManagerAvailable READ fileManagerAvailable
               NOTIFY fileManagerAvailableChanged)

    // Apakah fileopsd sedang tersedia via DBus
    Q_PROPERTY(bool fileOpsAvailable READ fileOpsAvailable
               NOTIFY fileOpsAvailableChanged)

public:
    explicit FileManagerShellBridge(FileManagerApi *api, QObject *parent = nullptr);
    ~FileManagerShellBridge() override;

    // ── In-process queries ────────────────────────────────────────────────

    /** Daftar sidebar places dari GIO (bookmarks + mounts + trash). */
    Q_INVOKABLE QVariantList sidebarPlaces() const;

    /** Jumlah item di trash. Di-update otomatis via GIO watch. */
    int trashItemCount() const;

    /** Apakah filemanager app (DBus activation) tersedia. */
    bool fileManagerAvailable() const;

    /** Apakah fileopsd tersedia. */
    bool fileOpsAvailable() const;

    // ── Navigation (cross-process via DBus) ──────────────────────────────

    /**
     * Buka folder di filemanager. Jika filemanager belum running,
     * DBus activation akan start-nya otomatis.
     * @param path  Path filesystem absolut.
     */
    Q_INVOKABLE void openPath(const QString &path);

    /**
     * Buka folder yang berisi path, lalu highlight item tersebut.
     */
    Q_INVOKABLE void revealItem(const QString &path);

    /** Buka jendela filemanager baru. path boleh kosong untuk home dir. */
    Q_INVOKABLE void newWindow(const QString &path = QString());

    // ── Volume management (in-process via GIO) ───────────────────────────

    Q_INVOKABLE void mountVolume(const QString &deviceUri);
    Q_INVOKABLE void unmountVolume(const QString &mountPath);
    Q_INVOKABLE void ejectVolume(const QString &deviceUri);

    // ── Trash ────────────────────────────────────────────────────────────

    /**
     * Kosongkan trash via fileopsd. Fire-and-forget — status via signal
     * operationFinished dengan type "empty-trash".
     */
    Q_INVOKABLE void emptyTrash();

    // ── File operations (cross-process via fileopsd) ──────────────────────

    /**
     * Mulai operasi copy. Return: operation ID string, atau "" bila gagal.
     * Status operasi dilaporkan via signal operationProgress/operationFinished.
     */
    Q_INVOKABLE QString startCopy(const QStringList &sourcePaths,
                                   const QString &destPath);

    Q_INVOKABLE QString startMove(const QStringList &sourcePaths,
                                   const QString &destPath);

    Q_INVOKABLE QString startDelete(const QStringList &paths);
    Q_INVOKABLE QString trashFiles(const QStringList &paths);

    /** Cancel operasi berdasarkan ID yang dikembalikan startCopy/startMove/dll. */
    Q_INVOKABLE void cancelOperation(const QString &operationId);

    // ── Search (in-process, incremental) ─────────────────────────────────

    /**
     * Mulai pencarian file.
     * Hasil dikirim via signal searchResultsAvailable() secara incremental.
     * Panggil cancelSearch() dengan sessionId yang sama untuk stop.
     *
     * @param rootPath    Direktori awal pencarian.
     * @param query       Query string (nama file, bisa berisi wildcard).
     * @param sessionId   ID unik untuk sesi ini (string bebas, misal QUuid).
     */
    Q_INVOKABLE void searchFiles(const QString &rootPath,
                                  const QString &query,
                                  const QString &sessionId);

    Q_INVOKABLE void cancelSearch(const QString &sessionId);

signals:
    // ── State changes ─────────────────────────────────────────────────────
    void sidebarPlacesChanged();
    void trashCountChanged(int count);
    void fileManagerAvailableChanged();
    void fileOpsAvailableChanged();

    // ── Mount events ──────────────────────────────────────────────────────
    void mountAdded(const QString &mountPath, const QString &label,
                    const QString &iconName, bool ejectable);
    void mountRemoved(const QString &mountPath);
    void volumeAdded(const QString &deviceUri, const QString &label,
                     const QString &iconName);

    // ── File operations progress ──────────────────────────────────────────
    /**
     * Dipancarkan secara periodik selama operasi berjalan.
     * @param fraction 0.0–1.0
     */
    void operationProgress(const QString &operationId, double fraction,
                           const QString &statusText);

    void operationFinished(const QString &operationId, bool success,
                           const QString &error);

    // ── Search results ────────────────────────────────────────────────────
    /**
     * Hasil pencarian incremental.
     * Tiap entry: { path: s, name: s, mimeType: s, size: t, modified: t }
     */
    void searchResultsAvailable(const QString &sessionId,
                                 const QVariantList &entries);

    void searchFinished(const QString &sessionId);

private slots:
    void onFileOpsJobsChanged();
    void onFileOpsProgress(const QString &id, int percent);
    void onFileOpsProgressDetail(const QString &id, qulonglong current,
                                  qulonglong total);
    void onFileOpsFinished(const QString &id);
    void onFileOpsError(const QString &id);
    void onTrashCountRefresh();
    void onFmServiceAppeared(const QString &service);
    void onFmServiceVanished(const QString &service);
    void onFileOpsServiceAppeared(const QString &service);
    void onFileOpsServiceVanished(const QString &service);

private:
    void connectFileOpsDbus();
    void connectFmDbus();
    void startTrashWatch();
    QDBusInterface *fileOpsIface() const;
    QDBusInterface *fmIface() const;

    // Buat request ID pendek untuk tracing lintas komponen
    static QString newReqId();

    FileManagerApi *m_api = nullptr;
    QDBusInterface *m_fileOpsIface = nullptr;
    QDBusInterface *m_fmIface = nullptr;
    bool m_fmAvailable = false;
    bool m_fileOpsAvailable = false;
    int m_trashItemCount = 0;

    // sessionId → future untuk search yang sedang berjalan
    QHash<QString, QFuture<void>> m_activeSearchSessions;
};
