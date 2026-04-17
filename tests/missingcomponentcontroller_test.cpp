#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "src/core/system/dependencyguard.h"
#include "src/core/system/missingcomponentcontroller.h"

class MissingComponentControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void installComponentForDomain_rejectsUnsupportedComponent();
    void blockingApi_available();
    void installComponentWithPolkit_mockedInstall_recoversComponent();
};

namespace {

class ScopedEnvVar
{
public:
    explicit ScopedEnvVar(const char *name)
        : m_name(name)
        , m_hadOld(qEnvironmentVariableIsSet(name))
        , m_old(qgetenv(name))
    {
    }

    ~ScopedEnvVar()
    {
        if (m_hadOld) {
            qputenv(m_name.constData(), m_old);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    bool m_hadOld = false;
    QByteArray m_old;
};

bool writeExecutableScript(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(content) != content.size()) {
        return false;
    }
    file.close();
    QFile::Permissions perms = file.permissions();
    perms |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
    return file.setPermissions(perms);
}

} // namespace

void MissingComponentControllerTest::installComponentForDomain_rejectsUnsupportedComponent()
{
    Slm::System::MissingComponentController controller;
    const QVariantMap result = controller.installComponentForDomain(QStringLiteral("recovery"),
                                                                    QStringLiteral("samba"));
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("unsupported-component"));
}

void MissingComponentControllerTest::blockingApi_available()
{
    Slm::System::MissingComponentController controller;
    const QVariantList blocking = controller.blockingMissingComponentsForDomain(QStringLiteral("filemanager"));
    const bool hasBlocking = controller.hasBlockingMissingForDomain(QStringLiteral("filemanager"));
    QCOMPARE(hasBlocking, !blocking.isEmpty());
}

void MissingComponentControllerTest::installComponentWithPolkit_mockedInstall_recoversComponent()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString binDir = tempDir.path() + QStringLiteral("/bin");
    QVERIFY2(QDir().mkpath(binDir), "failed to create temp bin dir");

    const QString markerFile = tempDir.path() + QStringLiteral("/component-installed.marker");
    QVERIFY2(writeExecutableScript(binDir + QStringLiteral("/pkexec"),
                                   QByteArrayLiteral("#!/bin/sh\nexec \"$@\"\n")),
             "failed to create fake pkexec");
    QVERIFY2(writeExecutableScript(
                     binDir + QStringLiteral("/apt-get"),
                     QByteArrayLiteral("#!/bin/sh\n"
                                       "if [ -n \"$SLM_TEST_INSTALL_TOUCH\" ]; then\n"
                                       "  mkdir -p \"$(dirname \"$SLM_TEST_INSTALL_TOUCH\")\"\n"
                                       "  : > \"$SLM_TEST_INSTALL_TOUCH\"\n"
                                       "fi\n"
                                       "exit 0\n")),
             "failed to create fake apt-get");

    ScopedEnvVar pathGuard("PATH");
    ScopedEnvVar touchGuard("SLM_TEST_INSTALL_TOUCH");
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", (binDir.toUtf8() + ':' + oldPath));
    qputenv("SLM_TEST_INSTALL_TOUCH", markerFile.toUtf8());

    Slm::System::ComponentRequirement req;
    req.id = QStringLiteral("e2e-mock-component");
    req.title = QStringLiteral("E2E Mock Component");
    req.description = QStringLiteral("Synthetic component for install e2e.");
    req.packageName = QStringLiteral("mock-package");
    req.requiredPaths = {markerFile};
    req.autoInstallable = true;
    req.guidance = QStringLiteral("Used by test only.");
    req.severity = QStringLiteral("required");

    const QVariantMap before = Slm::System::checkComponent(req);
    QCOMPARE(before.value(QStringLiteral("ready")).toBool(), false);

    const QVariantMap install = Slm::System::installComponentWithPolkit(req, 5000);
    QCOMPARE(install.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(install.value(QStringLiteral("readyAfterInstall")).toBool(), true);
    QCOMPARE(install.value(QStringLiteral("packageManager")).toString(), QStringLiteral("apt"));

    const QVariantMap after = Slm::System::checkComponent(req);
    QCOMPARE(after.value(QStringLiteral("ready")).toBool(), true);
}

QTEST_MAIN(MissingComponentControllerTest)
#include "missingcomponentcontroller_test.moc"
