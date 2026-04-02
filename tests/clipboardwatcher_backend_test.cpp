#include <QtTest>

#include "src/services/clipboard/ClipboardWatcher.h"

class ClipboardWatcherBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void selectsNativeWaylandWhenPlatformIsWayland();
    void selectsQtFallbackWhenForced();
    void selectsQtFallbackForNonWaylandPlatform();
};

void ClipboardWatcherBackendTest::selectsNativeWaylandWhenPlatformIsWayland()
{
    const auto mode = Slm::Clipboard::ClipboardWatcher::selectBackendMode(
        QStringLiteral("wayland"), false);
    QCOMPARE(mode, Slm::Clipboard::ClipboardWatcher::BackendMode::NativeWayland);
}

void ClipboardWatcherBackendTest::selectsQtFallbackWhenForced()
{
    const auto mode = Slm::Clipboard::ClipboardWatcher::selectBackendMode(
        QStringLiteral("wayland"), true);
    QCOMPARE(mode, Slm::Clipboard::ClipboardWatcher::BackendMode::QtFallback);
}

void ClipboardWatcherBackendTest::selectsQtFallbackForNonWaylandPlatform()
{
    const auto mode = Slm::Clipboard::ClipboardWatcher::selectBackendMode(
        QStringLiteral("xcb"), false);
    QCOMPARE(mode, Slm::Clipboard::ClipboardWatcher::BackendMode::QtFallback);
}

QTEST_MAIN(ClipboardWatcherBackendTest)
#include "clipboardwatcher_backend_test.moc"
