#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace {

bool writeTextFile(const QString &path, const QByteArray &content)
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

QString payloadFromProcess(QProcess &proc)
{
    const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    return out.isEmpty() ? err : out;
}

} // namespace

class PackagePolicyOneShotCliTest : public QObject
{
    Q_OBJECT

private slots:
    void dpkgRemoveProtected_blockedExit42();
    void dpkgInstallNonProtected_allowedExit0();
};

void PackagePolicyOneShotCliTest::dpkgRemoveProtected_blockedExit42()
{
    const QString bin = QString::fromUtf8(SLM_PACKAGE_POLICY_SERVICE_BIN_PATH);
    if (!QFile::exists(bin)) {
        QSKIP("slm-package-policy-service binary not found in build tree");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString protectedFile = tempDir.path() + QStringLiteral("/protected-capabilities.yaml");
    const QString mappingFile = tempDir.path() + QStringLiteral("/debian.yaml");
    const QString sourcePoliciesDir = tempDir.path() + QStringLiteral("/source-policies");
    QVERIFY(QDir().mkpath(sourcePoliciesDir));

    QVERIFY(writeTextFile(protectedFile,
                          QByteArrayLiteral("protected_capabilities:\n"
                                            "  - core.libc\n")));
    QVERIFY(writeTextFile(mappingFile,
                          QByteArrayLiteral("core.libc:\n"
                                            "  - libc6\n")));

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_PROTECTED_FILE"), protectedFile);
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_MAPPING_FILE"), mappingFile);
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_SOURCE_POLICIES_DIR"), sourcePoliciesDir);
    proc.setProcessEnvironment(env);
    proc.start(bin,
               QStringList{
                   QStringLiteral("--check"),
                   QStringLiteral("--json"),
                   QStringLiteral("--tool"),
                   QStringLiteral("dpkg"),
                   QStringLiteral("--"),
                   QStringLiteral("--remove"),
                   QStringLiteral("libc6"),
               });
    QVERIFY2(proc.waitForStarted(5000), "failed to start one-shot policy binary");
    QVERIFY2(proc.waitForFinished(10000), "one-shot policy binary timed out");
    QCOMPARE(proc.exitCode(), 42);

    const QJsonDocument doc = QJsonDocument::fromJson(payloadFromProcess(proc).toUtf8());
    QVERIFY2(doc.isObject(), "expected JSON object output");
    const QJsonObject obj = doc.object();
    QCOMPARE(obj.value(QStringLiteral("allowed")).toBool(), false);
    QVERIFY(obj.value(QStringLiteral("blockReasons")).toArray().size() > 0);
}

void PackagePolicyOneShotCliTest::dpkgInstallNonProtected_allowedExit0()
{
    const QString bin = QString::fromUtf8(SLM_PACKAGE_POLICY_SERVICE_BIN_PATH);
    if (!QFile::exists(bin)) {
        QSKIP("slm-package-policy-service binary not found in build tree");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString protectedFile = tempDir.path() + QStringLiteral("/protected-capabilities.yaml");
    const QString mappingFile = tempDir.path() + QStringLiteral("/debian.yaml");
    const QString sourcePoliciesDir = tempDir.path() + QStringLiteral("/source-policies");
    QVERIFY(QDir().mkpath(sourcePoliciesDir));

    QVERIFY(writeTextFile(protectedFile,
                          QByteArrayLiteral("protected_capabilities:\n"
                                            "  - core.libc\n")));
    QVERIFY(writeTextFile(mappingFile,
                          QByteArrayLiteral("core.libc:\n"
                                            "  - libc6\n")));

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_PROTECTED_FILE"), protectedFile);
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_MAPPING_FILE"), mappingFile);
    env.insert(QStringLiteral("SLM_PACKAGE_POLICY_SOURCE_POLICIES_DIR"), sourcePoliciesDir);
    proc.setProcessEnvironment(env);
    proc.start(bin,
               QStringList{
                   QStringLiteral("--check"),
                   QStringLiteral("--json"),
                   QStringLiteral("--tool"),
                   QStringLiteral("dpkg"),
                   QStringLiteral("--"),
                   QStringLiteral("--install"),
                   QStringLiteral("/tmp/vim_1.0.0-1_amd64.deb"),
               });
    QVERIFY2(proc.waitForStarted(5000), "failed to start one-shot policy binary");
    QVERIFY2(proc.waitForFinished(10000), "one-shot policy binary timed out");
    QCOMPARE(proc.exitCode(), 0);

    const QJsonDocument doc = QJsonDocument::fromJson(payloadFromProcess(proc).toUtf8());
    QVERIFY2(doc.isObject(), "expected JSON object output");
    const QJsonObject obj = doc.object();
    QCOMPARE(obj.value(QStringLiteral("allowed")).toBool(), true);
}

QTEST_MAIN(PackagePolicyOneShotCliTest)
#include "packagepolicy_oneshot_cli_test.moc"
