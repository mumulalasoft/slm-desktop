#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include "../core/workspace/windowingbackendmanager.h"

namespace {
void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  windowingctl --help\n";
    out << "  windowingctl matrix [--backend <kwin-wayland>] [--wait-ms <n>] [--pretty]\n";
    out << "  windowingctl watch [--backend <kwin-wayland>] [--duration-ms <n>]\n";
}

QString argValue(const QStringList &args, const QString &name)
{
    const int idx = args.indexOf(name);
    if (idx < 0 || idx + 1 >= args.size()) {
        return QString();
    }
    return args.at(idx + 1).trimmed();
}

bool hasArg(const QStringList &args, const QString &name)
{
    return args.contains(name);
}

QJsonObject mapToJson(const QVariantMap &map)
{
    return QJsonObject::fromVariantMap(map);
}

QJsonObject staticMatrixJson()
{
    QJsonObject staticMatrix;
    staticMatrix.insert(QStringLiteral("kwin-wayland"),
                        mapToJson(WindowingBackendManager::baseCapabilitiesForBackend(QStringLiteral("kwin-wayland"))));
    return staticMatrix;
}

QJsonObject runtimeSnapshot(WindowingBackendManager &manager, const QString &requestedBackend)
{
    QJsonObject root;
    root.insert(QStringLiteral("timestampMs"), QJsonValue::fromVariant(QDateTime::currentMSecsSinceEpoch()));
    root.insert(QStringLiteral("requestedBackend"),
                requestedBackend.isEmpty() ? QJsonValue(QStringLiteral("auto")) : QJsonValue(requestedBackend));
    root.insert(QStringLiteral("activeBackend"), manager.backend());
    root.insert(QStringLiteral("connected"), manager.connected());
    root.insert(QStringLiteral("protocolVersion"), manager.protocolVersion());
    root.insert(QStringLiteral("eventSchemaVersion"), manager.eventSchemaVersion());
    root.insert(QStringLiteral("runtimeCapabilities"), mapToJson(manager.capabilities()));
    return root;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QStringList args = app.arguments();

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString cmd = args.at(1).trimmed().toLower();
    if (cmd == QStringLiteral("--help") || cmd == QStringLiteral("-h") || cmd == QStringLiteral("help")) {
        printUsage();
        return 0;
    }

    if (cmd != QStringLiteral("matrix") && cmd != QStringLiteral("watch")) {
        err << "unknown command: " << cmd << '\n';
        printUsage();
        return 1;
    }

    const QString requestedBackend = argValue(args, QStringLiteral("--backend"));
    WindowingBackendManager manager;
    if (!requestedBackend.isEmpty()) {
        manager.configureBackend(requestedBackend);
    }

    if (cmd == QStringLiteral("matrix")) {
        bool okWait = false;
        int waitMs = argValue(args, QStringLiteral("--wait-ms")).toInt(&okWait);
        if (!okWait) {
            waitMs = 250;
        }
        waitMs = qBound(0, waitMs, 10000);
        const bool pretty = hasArg(args, QStringLiteral("--pretty"));

        if (waitMs > 0) {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(&manager, &WindowingBackendManager::connectedChanged, &loop, &QEventLoop::quit);
            QObject::connect(&manager, &WindowingBackendManager::capabilitiesChanged, &loop, &QEventLoop::quit);
            QObject::connect(&manager, &WindowingBackendManager::protocolVersionChanged, &loop, &QEventLoop::quit);
            QObject::connect(&manager, &WindowingBackendManager::eventSchemaVersionChanged, &loop, &QEventLoop::quit);
            timer.start(waitMs);
            loop.exec();
        }

        QJsonObject root = runtimeSnapshot(manager, requestedBackend);
        root.insert(QStringLiteral("staticMatrix"), staticMatrixJson());

        const QJsonDocument doc(root);
        out << (pretty ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
                       : QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        if (!pretty) {
            out << '\n';
        }
        return 0;
    }

    bool okDuration = false;
    int durationMs = argValue(args, QStringLiteral("--duration-ms")).toInt(&okDuration);
    if (!okDuration) {
        durationMs = 0;
    }
    durationMs = qBound(0, durationMs, 3600000);

    const auto emitLine = [&](const QString &kind, const QVariantMap &eventPayload = QVariantMap()) {
        QJsonObject line = runtimeSnapshot(manager, requestedBackend);
        line.insert(QStringLiteral("kind"), kind);
        if (!eventPayload.isEmpty()) {
            line.insert(QStringLiteral("eventPayload"), mapToJson(eventPayload));
        }
        out << QString::fromUtf8(QJsonDocument(line).toJson(QJsonDocument::Compact)) << '\n';
        out.flush();
    };

    emitLine(QStringLiteral("snapshot"));

    QObject::connect(&manager, &WindowingBackendManager::connectedChanged, &app, [&]() {
        emitLine(QStringLiteral("connected-changed"));
    });
    QObject::connect(&manager, &WindowingBackendManager::capabilitiesChanged, &app, [&]() {
        emitLine(QStringLiteral("capabilities-changed"));
    });
    QObject::connect(&manager, &WindowingBackendManager::protocolVersionChanged, &app, [&]() {
        emitLine(QStringLiteral("protocol-changed"));
    });
    QObject::connect(&manager, &WindowingBackendManager::eventSchemaVersionChanged, &app, [&]() {
        emitLine(QStringLiteral("event-schema-changed"));
    });
    QObject::connect(&manager, &WindowingBackendManager::eventReceived, &app,
                     [&](const QString &event, const QVariantMap &payload) {
        QVariantMap envelope;
        envelope.insert(QStringLiteral("event"), event);
        envelope.insert(QStringLiteral("payload"), payload);
        emitLine(QStringLiteral("event-received"), envelope);
    });

    QTimer timeout;
    if (durationMs > 0) {
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &app, &QCoreApplication::quit);
        timeout.start(durationMs);
    }
    return app.exec();
}
