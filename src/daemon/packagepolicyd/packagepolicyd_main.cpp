#include "src/services/packagepolicy/packagepolicyservice.h"

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

using Slm::PackagePolicy::PackagePolicyService;

namespace {
constexpr const char kServiceName[] = "org.slm.PackagePolicy1";
constexpr const char kServicePath[] = "/org/slm/PackagePolicy1";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-package-policy-service"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SLM package policy gate"));
    parser.addHelpOption();

    QCommandLineOption daemonOpt(QStringList{QStringLiteral("d"), QStringLiteral("daemon")},
                                  QStringLiteral("Run as session D-Bus daemon"));
    QCommandLineOption checkOpt(QStringList{QStringLiteral("check")},
                                 QStringLiteral("Run a one-shot policy check"));
    QCommandLineOption toolOpt(QStringList{QStringLiteral("tool")},
                               QStringLiteral("Tool name: apt, apt-get, dpkg"),
                               QStringLiteral("tool"));
    QCommandLineOption jsonOpt(QStringList{QStringLiteral("json")},
                               QStringLiteral("Print one-shot output as JSON"));

    parser.addOption(daemonOpt);
    parser.addOption(checkOpt);
    parser.addOption(toolOpt);
    parser.addOption(jsonOpt);
    parser.process(app);

    PackagePolicyService service;

    if (parser.isSet(checkOpt)) {
        const QString tool = parser.value(toolOpt).trimmed();
        const QStringList positional = parser.positionalArguments();

        const QVariantMap result = service.Evaluate(tool, positional);
        const bool allowed = result.value(QStringLiteral("allowed")).toBool();
        const QJsonObject json = QJsonObject::fromVariantMap(result);
        if (parser.isSet(jsonOpt) || !allowed) {
            const QByteArray out = QJsonDocument(json).toJson(QJsonDocument::Compact);
            fprintf(allowed ? stdout : stderr, "%s\n", out.constData());
        }
        return allowed ? 0 : 42;
    }

    if (!parser.isSet(daemonOpt)) {
        parser.showHelp(1);
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        fprintf(stderr,
                "slm-package-policy-service: failed to register service %s: %s\n",
                kServiceName,
                qPrintable(bus.lastError().message()));
        return 2;
    }
    if (!bus.registerObject(QString::fromLatin1(kServicePath),
                            &service,
                            QDBusConnection::ExportAllSlots)) {
        fprintf(stderr,
                "slm-package-policy-service: failed to register object %s: %s\n",
                kServicePath,
                qPrintable(bus.lastError().message()));
        return 2;
    }

    return app.exec();
}
