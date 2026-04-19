#include "componenthealthcontroller.h"

#include "../../../../core/system/missingcomponentcontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtConcurrent/QtConcurrentRun>

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

void ComponentHealthController::evaluatePackagePolicy(const QString &tool,
                                                      const QString &arguments)
{
    if (m_evaluating) return;

    const QString normalizedTool = tool.trimmed().toLower();
    if (normalizedTool != QStringLiteral("apt")
        && normalizedTool != QStringLiteral("apt-get")
        && normalizedTool != QStringLiteral("dpkg")) {
        emit policyEvaluated({
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("unsupported-tool")},
            {QStringLiteral("message"), QStringLiteral("Tool harus apt, apt-get, atau dpkg.")},
        });
        return;
    }

    const QStringList args = QProcess::splitCommand(arguments.trimmed());
    if (args.isEmpty()) {
        emit policyEvaluated({
            {QStringLiteral("allowed"), false},
            {QStringLiteral("error"), QStringLiteral("missing-arguments")},
            {QStringLiteral("message"), QStringLiteral("Masukkan argumen command paket yang akan dicek.")},
        });
        return;
    }

    m_evaluating = true;
    emit evaluatingChanged();

    m_policyWatcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_policyWatcher, &QFutureWatcher<QVariantMap>::finished, this, [this]() {
        QVariantMap result = m_policyWatcher->result();
        m_policyWatcher->deleteLater();
        m_policyWatcher = nullptr;
        m_evaluating    = false;
        emit evaluatingChanged();
        emit policyEvaluated(result);
    });

    m_policyWatcher->setFuture(QtConcurrent::run([normalizedTool, args]() -> QVariantMap {
        QDBusInterface iface(QStringLiteral("org.slm.PackagePolicy1"),
                             QStringLiteral("/org/slm/PackagePolicy1"),
                             QStringLiteral("org.slm.PackagePolicy1"),
                             QDBusConnection::sessionBus());
        QVariantMap out;
        if (!iface.isValid()) {
            out = runPolicyOneShot(normalizedTool, args);
        } else {
            QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Evaluate"),
                                                       normalizedTool, args);
            if (!reply.isValid()) {
                out = {
                    {QStringLiteral("allowed"), false},
                    {QStringLiteral("error"), QStringLiteral("policy-service-call-failed")},
                    {QStringLiteral("message"), reply.error().message().trimmed().isEmpty()
                                                    ? QStringLiteral("Gagal memanggil service package policy.")
                                                    : reply.error().message().trimmed()},
                };
            } else {
                out = reply.value();
            }
        }
        out.insert(QStringLiteral("tool"), normalizedTool);
        out.insert(QStringLiteral("arguments"), args);
        return out;
    }));
}
