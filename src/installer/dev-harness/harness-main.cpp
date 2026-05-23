// slm-installer-harness — visual harness for SLM installer QML screens.
//
// Standalone Qt 6 process that hosts a chosen screen with a mock config
// and sample data. Designed for tight UI iteration: edit a screen file,
// rebuild, relaunch — no Calamares, no VM, no .deb.
//
// Usage:
//     slm-installer-harness <screen>
//     slm-installer-harness disk-select
//     slm-installer-harness summary
//     slm-installer-harness welcome
//
// Each `<screen>` maps to qrc:/harness/<screen>-harness.qml, which is
// embedded in this binary via resources.qrc. The SlmInstaller QML module
// is embedded at qrc:/qt/qml/SlmInstaller/ (mirrors the viewmodule layout)
// so `import SlmInstaller 1.0` resolves without any system path.

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickView>
#include <QString>
#include <QUrl>

#include <cstdio>

namespace {

constexpr const char *kKnownScreens =
    "  disk-select   §2.3 disk selection (3 mock disks: NVMe healthy + ESP,\n"
    "                SATA SSD warning, USB SMART-failed)\n"
    "  summary       §2.5 confirmation (with sample warnings list)\n"
    "  welcome       §2.1 welcome card";

int usage(const char *progName)
{
    std::fprintf(stderr,
                 "Usage: %s <screen>\n\n"
                 "Available screens:\n%s\n",
                 progName, kKnownScreens);
    return 64;  // sysexits EX_USAGE
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-installer-harness"));

    if (argc != 2) {
        return usage(argv[0]);
    }
    const QString screen = QString::fromLocal8Bit(argv[1]);

    QQuickView view;
    view.engine()->addImportPath(QStringLiteral("qrc:/qt/qml"));
    view.setSource(QUrl(QStringLiteral("qrc:/harness/%1-harness.qml").arg(screen)));

    if (view.status() == QQuickView::Error) {
        std::fprintf(stderr, "Failed to load harness scene '%s':\n",
                     qPrintable(screen));
        for (const QQmlError &err : view.errors()) {
            std::fprintf(stderr, "  %s\n", qPrintable(err.toString()));
        }
        return 1;
    }

    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(960, 720);
    view.setTitle(QStringLiteral("SLM Installer — %1").arg(screen));
    view.show();

    return app.exec();
}
