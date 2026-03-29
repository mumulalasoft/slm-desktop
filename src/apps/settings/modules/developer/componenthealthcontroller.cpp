#include "componenthealthcontroller.h"

#include "src/core/system/missingcomponentcontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace {
QVariantMap runPolicyOneShot(const QString &tool, const QStringList &args)
{
    const QString bin = qEnvironmentVariable("SLM_PACKAGE_POLICY_SERVICE_BIN",
                                             QStringLiteral("slm-package-policy-service"));
    QProcess process;
    QStringList cmdArgs;
    cmdArgs << QStringLiteral("--check")
            << QStringLiteral("--json")
            << QStringLiteral("--tool")
            << tool
            << QStringLiteral("--");
    cmdArgs << args;
    process.start(bin, cmdArgs);
    if (!process.waitForStarted(3000)) {
        return {
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("policy-service-unavailable")},
            {QStringLiteral("message"), QStringLiteral("Service package policy tidak aktif.")},
        };
    }
    process.waitForFinished(15000);
    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
    const QString payload = !stdoutText.isEmpty() ? stdoutText : stderrText;
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (doc.isObject()) {
        return doc.object().toVariantMap();
    }
    return {
        {QStringLiteral("allowed"), false},
        {QStringLiteral("error"), QStringLiteral("policy-service-call-failed")},
        {QStringLiteral("message"), stderrText.isEmpty()
                                        ? QStringLiteral("Gagal memanggil service package policy.")
                                        : stderrText},
    };
}
} // namespace

ComponentHealthController::ComponentHealthController(QObject *parent)
    : QObject(parent)
    , m_missingController(new Slm::System::MissingComponentController(this))
{
}

QVariantList ComponentHealthController::missingComponentsForDomain(const QString &domain) const
{
    return m_missingController->missingComponentsForDomain(domain);
}

bool ComponentHealthController::hasBlockingMissingForDomain(const QString &domain) const
{
    return m_missingController->hasBlockingMissingForDomain(domain);
}

QVariantMap ComponentHealthController::installComponent(const QString &componentId) const
{
    return m_missingController->installComponent(componentId);
}

QVariantMap ComponentHealthController::evaluatePackagePolicy(const QString &tool,
                                                             const QString &arguments) const
{
    const QString normalizedTool = tool.trimmed().toLower();
    if (normalizedTool != QStringLiteral("apt")
        && normalizedTool != QStringLiteral("apt-get")
        && normalizedTool != QStringLiteral("dpkg")) {
        return {
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("unsupported-tool")},
            {QStringLiteral("message"), QStringLiteral("Tool harus apt, apt-get, atau dpkg.")},
        };
    }

    const QStringList args = QProcess::splitCommand(arguments.trimmed());
    if (args.isEmpty()) {
        return {
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("missing-arguments")},
            {QStringLiteral("message"), QStringLiteral("Masukkan argumen command paket yang akan dicek.")},
        };
    }

    QDBusInterface iface(QStringLiteral("org.slm.PackagePolicy1"),
                         QStringLiteral("/org/slm/PackagePolicy1"),
                         QStringLiteral("org.slm.PackagePolicy1"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        QVariantMap out = runPolicyOneShot(normalizedTool, args);
        out.insert(QStringLiteral("tool"), normalizedTool);
        out.insert(QStringLiteral("arguments"), args);
        return out;
    }

    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Evaluate"),
                                               normalizedTool,
                                               args);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("policy-service-call-failed")},
            {QStringLiteral("message"), reply.error().message().trimmed().isEmpty()
                                            ? QStringLiteral("Gagal memanggil service package policy.")
                                            : reply.error().message().trimmed()},
        };
    }

    QVariantMap out = reply.value();
    out.insert(QStringLiteral("tool"), normalizedTool);
    out.insert(QStringLiteral("arguments"), args);
    return out;
}
