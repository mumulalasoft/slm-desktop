#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <unistd.h>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "recoveryapp.h"
#include "../../core/system/missingcomponentcontroller.h"

// Writes/updates the shared lifecycle marker file that the watchdog reads.
// Uses the same path as slm-desktop: $XDG_RUNTIME_DIR/slm-shell-lifecycle.json.
static void writeRecoveryLifecycle(const QString &phase)
{
    static QJsonObject s_lc;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    s_lc.insert(phase, now);
    const QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    const QString path = runtimeDir.isEmpty()
        ? QStringLiteral("/tmp/slm-shell-lifecycle.json")
        : QString::fromLocal8Bit(runtimeDir) + QStringLiteral("/slm-shell-lifecycle.json");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(s_lc).toJson(QJsonDocument::Compact));
    qInfo("slm-recovery-app: lifecycle[%s]=%s", qPrintable(phase), qPrintable(now));
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-recovery-app"));
    app.setOrganizationName(QStringLiteral("SLM Desktop"));

    qInfo("=== slm-recovery-app start (pid=%d) ===", static_cast<int>(getpid()));
    writeRecoveryLifecycle(QStringLiteral("started"));

    Slm::Login::RecoveryApp recovery;
    Slm::System::MissingComponentController missingComponents;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("RecoveryApp"), &recovery);
    engine.rootContext()->setContextProperty(QStringLiteral("MissingComponents"), &missingComponents);

    const QUrl url(QStringLiteral("qrc:/qt/qml/SlmRecovery/Qml/recovery/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // Hook firstFrame on the root window.  Qt::SingleShotConnection ensures this
    // fires exactly once so the timestamp is pinned to the first rendered frame.
    for (QObject *obj : engine.rootObjects()) {
        if (auto *win = qobject_cast<QQuickWindow *>(obj)) {
            QObject::connect(win, &QQuickWindow::frameSwapped, win, [&recovery]() {
                writeRecoveryLifecycle(QStringLiteral("firstFrame"));
                recovery.markRecoveryUiHealthy();
            }, Qt::SingleShotConnection);
            break;
        }
    }

    return app.exec();
}
