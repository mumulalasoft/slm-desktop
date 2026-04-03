#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVariantMap>

#include "../src/services/indicator/statusnotifierhost.h"

namespace {
constexpr const char kItemInterface[] = "org.kde.StatusNotifierItem";
constexpr const char kItemPath[] = "/StatusNotifierItem";
}

class FakeStatusNotifierItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title NOTIFY NewTitle)
    Q_PROPERTY(QString Status READ status NOTIFY NewStatus)
    Q_PROPERTY(QString IconName READ iconName NOTIFY NewIcon)
    Q_PROPERTY(QString AttentionIconName READ attentionIconName CONSTANT)
    Q_PROPERTY(QString IconThemePath READ iconThemePath NOTIFY NewIconThemePath)
    Q_PROPERTY(QDBusObjectPath Menu READ menu NOTIFY NewMenu)
    Q_PROPERTY(QVariant IconPixmap READ iconPixmap CONSTANT)

public:
    QString id() const { return QStringLiteral("fake-item"); }
    QString title() const { return m_title; }
    QString status() const { return m_status; }
    QString iconName() const { return m_iconName; }
    QString attentionIconName() const { return QStringLiteral("dialog-warning-symbolic"); }
    QString iconThemePath() const { return QString(); }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/MenuBar")); }
    QVariant iconPixmap() const { return QVariant(); }

    void setTitle(const QString &title)
    {
        if (m_title == title) {
            return;
        }
        m_title = title;
        emit NewTitle();
    }

public slots:
    void Activate(int x, int y)
    {
        ++activateCount;
        lastX = x;
        lastY = y;
    }

    void SecondaryActivate(int x, int y)
    {
        ++secondaryActivateCount;
        lastX = x;
        lastY = y;
    }

    void ContextMenu(int x, int y)
    {
        ++contextMenuCount;
        lastX = x;
        lastY = y;
    }

    void Scroll(int delta, const QString &orientation)
    {
        ++scrollCount;
        lastDelta = delta;
        lastOrientation = orientation;
    }

signals:
    void NewTitle();
    void NewStatus(const QString &status);
    void NewIcon();
    void NewToolTip();
    void NewMenu();
    void NewAttentionIcon();
    void NewOverlayIcon();
    void NewIconThemePath(const QString &iconThemePath);

public:
    int activateCount = 0;
    int secondaryActivateCount = 0;
    int contextMenuCount = 0;
    int scrollCount = 0;
    int lastX = 0;
    int lastY = 0;
    int lastDelta = 0;
    QString lastOrientation;

private:
    QString m_title = QStringLiteral("Fake Item");
    QString m_status = QStringLiteral("Active");
    QString m_iconName = QStringLiteral("network-wireless-symbolic");
};

class FakeActivateOnlyItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QString IconName READ iconName CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu CONSTANT)
    Q_PROPERTY(QVariant IconPixmap READ iconPixmap CONSTANT)

public:
    QString id() const { return QStringLiteral("activate-only"); }
    QString title() const { return QStringLiteral("Activate Only"); }
    QString status() const { return QStringLiteral("Active"); }
    QString iconName() const { return QStringLiteral("application-x-executable-symbolic"); }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/MenuBar")); }
    QVariant iconPixmap() const { return QVariant(); }

public slots:
    void Activate(int, int) { ++activateCount; }

public:
    int activateCount = 0;
};

class FakeSecondaryOnlyItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QString IconName READ iconName CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu CONSTANT)
    Q_PROPERTY(QVariant IconPixmap READ iconPixmap CONSTANT)

public:
    QString id() const { return QStringLiteral("secondary-only"); }
    QString title() const { return QStringLiteral("Secondary Only"); }
    QString status() const { return QStringLiteral("Active"); }
    QString iconName() const { return QStringLiteral("application-x-executable-symbolic"); }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/MenuBar")); }
    QVariant iconPixmap() const { return QVariant(); }

public slots:
    void SecondaryActivate(int, int) { ++secondaryActivateCount; }
    void Activate(int, int) { ++activateCount; }

public:
    int secondaryActivateCount = 0;
    int activateCount = 0;
};

class FakeNoActionItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ title CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QString IconName READ iconName CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu CONSTANT)
    Q_PROPERTY(QVariant IconPixmap READ iconPixmap CONSTANT)

public:
    QString id() const { return QStringLiteral("no-action"); }
    QString title() const { return QStringLiteral("No Action"); }
    QString status() const { return QStringLiteral("Active"); }
    QString iconName() const { return QStringLiteral("application-x-executable-symbolic"); }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/MenuBar")); }
    QVariant iconPixmap() const { return QVariant(); }
};

class StatusNotifierHostTest : public QObject
{
    Q_OBJECT

private slots:
    void registerUpdateAndDispatch()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QString serviceName = QStringLiteral("org.slm.Test.StatusNotifierItem.%1")
                                        .arg(QCoreApplication::applicationPid());
        bus.unregisterService(serviceName);
        QVERIFY(bus.registerService(serviceName));

        FakeStatusNotifierItem item;
        QVERIFY(bus.registerObject(QString::fromLatin1(kItemPath),
                                   &item,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllSignals |
                                       QDBusConnection::ExportAllProperties));

        StatusNotifierHost host;
        host.RegisterStatusNotifierItem(serviceName);

        QTRY_VERIFY(!host.entries().isEmpty());
        const QString itemId = serviceName + QString::fromLatin1(kItemPath);

        QVariantMap row = findEntry(host, itemId);
        QVERIFY(!row.isEmpty());
        QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Fake Item"));
        QCOMPARE(row.value(QStringLiteral("menuPath")).toString(), QStringLiteral("/MenuBar"));
        QCOMPARE(row.value(QStringLiteral("status")).toString(), QStringLiteral("Active"));

        host.activate(itemId, 10, 20);
        host.secondaryActivate(itemId, 30, 40);
        host.contextMenu(itemId, 50, 60);
        host.scroll(itemId, 120, QStringLiteral("vertical"));

        QTRY_COMPARE(item.activateCount, 1);
        QTRY_COMPARE(item.secondaryActivateCount, 1);
        QTRY_COMPARE(item.contextMenuCount, 1);
        QTRY_COMPARE(item.scrollCount, 1);
        QCOMPARE(item.lastDelta, 120);
        QCOMPARE(item.lastOrientation, QStringLiteral("vertical"));

        item.setTitle(QStringLiteral("Updated Title"));
        QTRY_VERIFY(findEntry(host, itemId).value(QStringLiteral("title")).toString() == QStringLiteral("Updated Title"));

        QVERIFY(bus.unregisterService(serviceName));
        QTRY_VERIFY(findEntry(host, itemId).isEmpty());
        QVERIFY(host.entries().isEmpty());
    }

    void contextMenu_fallbackToSecondaryActivate()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QString serviceName = QStringLiteral("org.slm.Test.StatusNotifierItem.Secondary.%1")
                                        .arg(QCoreApplication::applicationPid());
        bus.unregisterService(serviceName);
        QVERIFY(bus.registerService(serviceName));

        FakeSecondaryOnlyItem item;
        QVERIFY(bus.registerObject(QString::fromLatin1(kItemPath),
                                   &item,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllProperties));

        StatusNotifierHost host;
        host.RegisterStatusNotifierItem(serviceName);

        const QString itemId = serviceName + QString::fromLatin1(kItemPath);
        QTRY_VERIFY(!findEntry(host, itemId).isEmpty());

        host.contextMenu(itemId, 12, 34);
        QTRY_COMPARE(item.secondaryActivateCount, 1);
        QCOMPARE(item.activateCount, 0);

        QVERIFY(bus.unregisterService(serviceName));
    }

    void contextMenu_fallbackToActivate()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QString serviceName = QStringLiteral("org.slm.Test.StatusNotifierItem.Activate.%1")
                                        .arg(QCoreApplication::applicationPid());
        bus.unregisterService(serviceName);
        QVERIFY(bus.registerService(serviceName));

        FakeActivateOnlyItem item;
        QVERIFY(bus.registerObject(QString::fromLatin1(kItemPath),
                                   &item,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllProperties));

        StatusNotifierHost host;
        host.RegisterStatusNotifierItem(serviceName);

        const QString itemId = serviceName + QString::fromLatin1(kItemPath);
        QTRY_VERIFY(!findEntry(host, itemId).isEmpty());

        host.contextMenu(itemId, 7, 8);
        QTRY_COMPARE(item.activateCount, 1);

        QVERIFY(bus.unregisterService(serviceName));
    }

    void contextMenu_noSupportedAction_noCrashNoHang()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QString serviceName = QStringLiteral("org.slm.Test.StatusNotifierItem.NoAction.%1")
                                        .arg(QCoreApplication::applicationPid());
        bus.unregisterService(serviceName);
        QVERIFY(bus.registerService(serviceName));

        FakeNoActionItem item;
        QVERIFY(bus.registerObject(QString::fromLatin1(kItemPath),
                                   &item,
                                   QDBusConnection::ExportAllProperties));

        StatusNotifierHost host;
        host.RegisterStatusNotifierItem(serviceName);

        const QString itemId = serviceName + QString::fromLatin1(kItemPath);
        QTRY_VERIFY(!findEntry(host, itemId).isEmpty());

        QElapsedTimer timer;
        timer.start();
        host.contextMenu(itemId, 1, 2);
        QVERIFY2(timer.elapsed() < 500, "contextMenu fallback chain took too long");

        QVERIFY(bus.unregisterService(serviceName));
    }

private:
    static QVariantMap findEntry(const StatusNotifierHost &host, const QString &id)
    {
        const QVariantList list = host.entries();
        for (const QVariant &rowValue : list) {
            const QVariantMap row = rowValue.toMap();
            if (row.value(QStringLiteral("id")).toString() == id) {
                return row;
            }
        }
        return {};
    }
};

QTEST_MAIN(StatusNotifierHostTest)
#include "statusnotifierhost_test.moc"
