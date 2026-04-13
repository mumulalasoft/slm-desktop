#include <QtTest/QtTest>

#include <QFile>

namespace {

QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

// Returns true if the source contains a suspicious log call that includes
// password-adjacent field names adjacent to a log macro.
bool containsPasswordLog(const QString &source)
{
    static const QStringList logMacros = {
        QStringLiteral("qInfo()"),
        QStringLiteral("qDebug()"),
        QStringLiteral("qWarning()"),
        QStringLiteral("qCritical()"),
    };
    static const QStringList sensitiveTerms = {
        QStringLiteral("password"),
        QStringLiteral("Password"),
        QStringLiteral("passwd"),
        QStringLiteral("credential"),
        QStringLiteral("secret"),
        QStringLiteral("passphrase"),
    };

    const QStringList lines = source.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        // Skip pure comment lines.
        if (trimmed.startsWith(QStringLiteral("//")) || trimmed.startsWith(QStringLiteral("*"))) {
            continue;
        }
        bool hasLogMacro = false;
        for (const auto &macro : logMacros) {
            if (trimmed.contains(macro)) {
                hasLogMacro = true;
                break;
            }
        }
        if (!hasLogMacro) {
            continue;
        }
        for (const auto &term : sensitiveTerms) {
            if (trimmed.contains(term)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

// Contract test: verifies the polkit agent security hardening and state machine.
// These are "regression guard" tests — they catch unsafe changes before they ship.
class PolkitAgentContractTest : public QObject
{
    Q_OBJECT

private slots:
    // ── Security: no password in logs ────────────────────────────────────────

    void noPasswordInLogs_appFile()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));
        QVERIFY2(!containsPasswordLog(text),
                 "polkitagentapp.cpp contains a log call that may emit password/credential data");
    }

    void noPasswordInLogs_dialogController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/authdialogcontroller.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));
        QVERIFY2(!containsPasswordLog(text),
                 "authdialogcontroller.cpp contains a log call that may emit password/credential data");
    }

    void noPasswordInLogs_authSession()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/authsession.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));
        QVERIFY2(!containsPasswordLog(text),
                 "authsession.cpp contains a log call that may emit password/credential data");
    }

    // ── Security: secure buffer wipe after password use ───────────────────────

    void passwordBuffer_isWipedAfterResponse()
    {
        // authsession.cpp must define a secure wipe function and call it
        // after polkit_agent_session_response.
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/authsession.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        // Secure wipe helper must exist.
        QVERIFY(text.contains(QStringLiteral("clearByteArray")));
        // It must use volatile to prevent compiler elision.
        QVERIFY(text.contains(QStringLiteral("volatile char *")));
        // Must be called after the response is sent (respondPassword calls it).
        const int responseIdx = text.indexOf(QStringLiteral("polkit_agent_session_response("));
        const int wipeIdx     = text.indexOf(QStringLiteral("clearByteArray("));
        QVERIFY2(responseIdx >= 0, "polkit_agent_session_response not found in authsession.cpp");
        QVERIFY2(wipeIdx >= 0,     "clearByteArray not called in authsession.cpp");
        QVERIFY2(wipeIdx > responseIdx,
                 "clearByteArray must be called AFTER polkit_agent_session_response");
    }

    // ── State machine: request queue ─────────────────────────────────────────

    void requestQueue_isImplemented()
    {
        // The agent must queue concurrent auth requests rather than silently
        // failing them with BUSY. Concurrent requests happen when two apps request
        // privilege at the same time.
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.h");
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString header = readTextFile(headerPath);
        const QString source = readTextFile(sourcePath);

        QVERIFY2(!header.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // Queue data structure declared.
        QVERIFY(header.contains(QStringLiteral("QQueue<QueuedRequest>")));
        // processNextRequest declared in the header.
        QVERIFY(header.contains(QStringLiteral("void processNextRequest()")));
        // processNextRequest implemented in the source.
        QVERIFY(source.contains(QStringLiteral("void PolkitAgentApp::processNextRequest()")));
        // Queue is drained on completion (processNextRequest called inside completeAuthenticationRequest).
        QVERIFY(source.contains(QStringLiteral("processNextRequest()")));
        // Concurrent requests are enqueued (m_requestQueue.enqueue).
        QVERIFY(source.contains(QStringLiteral("m_requestQueue.enqueue(")));
        // Queue is drained on shutdown.
        QVERIFY(source.contains(QStringLiteral("m_requestQueue.isEmpty()")));
        QVERIFY(source.contains(QStringLiteral("m_requestQueue.dequeue()")));
    }

    void requestQueue_gRefCountedForQueued()
    {
        // Identities and cancellables stored in the queue must be g_object_ref'd
        // to prevent use-after-free when polkitd releases its copy.
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString source = readTextFile(sourcePath);
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        QVERIFY(source.contains(QStringLiteral("g_object_ref(identity)")));
        QVERIFY(source.contains(QStringLiteral("g_object_unref(req.identity)")));
        QVERIFY(source.contains(QStringLiteral("g_object_unref(req.task)")));
    }

    // ── State machine: auth timeout ───────────────────────────────────────────

    void authTimeout_isSetUpAndConnected()
    {
        // A pending auth request must time out if the user never answers.
        // Without this, a race where the dialog is not visible would cause
        // polkitd to wait forever, blocking the requesting application.
        const QString headerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.h");
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString header = readTextFile(headerPath);
        const QString source = readTextFile(sourcePath);

        QVERIFY2(!header.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(headerPath)));
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // Timeout timer declared.
        QVERIFY(header.contains(QStringLiteral("m_requestTimeout")));
        // Interval and single-shot set up.
        QVERIFY(source.contains(QStringLiteral("m_requestTimeout.setInterval(")));
        QVERIFY(source.contains(QStringLiteral("m_requestTimeout.setSingleShot(true)")));
        // Timeout triggers completeAuthenticationRequest with "timeout" error.
        QVERIFY(source.contains(QStringLiteral("completeAuthenticationRequest(false, QStringLiteral(\"timeout\"))")));
        // Timeout started when a request begins.
        QVERIFY(source.contains(QStringLiteral("m_requestTimeout.start()")));
        // Timeout stopped when a request completes.
        QVERIFY(source.contains(QStringLiteral("m_requestTimeout.stop()")));
    }

    // ── Restart-safety: lock file ─────────────────────────────────────────────

    void lockFile_preventsDuplicateInstances()
    {
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString source = readTextFile(sourcePath);
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // Lock file with stale recovery.
        QVERIFY(source.contains(QStringLiteral("m_lockFile.tryLock()")));
        QVERIFY(source.contains(QStringLiteral("m_lockFile.removeStaleLockFile()")));
        QVERIFY(source.contains(QStringLiteral("m_lockFile.unlock()")));
        // Stale lock detection is active (getLockInfo call logs the stale owner).
        QVERIFY(source.contains(QStringLiteral("m_lockFile.getLockInfo(")));
    }

    // ── Cancel path ───────────────────────────────────────────────────────────

    void cancelPath_disconnectsHandlerAndClearsState()
    {
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString source = readTextFile(sourcePath);
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        // GCancellable handler is disconnected when request completes.
        QVERIFY(source.contains(QStringLiteral("g_cancellable_disconnect(")));
        QVERIFY(source.contains(QStringLiteral("g_object_unref(m_pendingCancellable)")));
        QVERIFY(source.contains(QStringLiteral("m_pendingCancellable = nullptr")));
        QVERIFY(source.contains(QStringLiteral("m_pendingCancelHandler = 0")));
        // GTask is unref'd when completed.
        QVERIFY(source.contains(QStringLiteral("g_object_unref(task)")));
        QVERIFY(source.contains(QStringLiteral("m_pendingTask = nullptr")));
    }

    void cancelledQueuedRequest_isSkipped()
    {
        // processNextRequest must skip already-cancelled queued requests
        // rather than trying to start an auth session for them.
        const QString sourcePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/login/polkit-agent/polkitagentapp.cpp");
        const QString source = readTextFile(sourcePath);
        QVERIFY2(!source.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));

        QVERIFY(source.contains(QStringLiteral("g_cancellable_is_cancelled(")));
        QVERIFY(source.contains(QStringLiteral("request-cancelled-while-queued")));
    }
};

QTEST_MAIN(PolkitAgentContractTest)
#include "polkit_agent_contract_test.moc"
