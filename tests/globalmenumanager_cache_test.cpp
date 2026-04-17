#include <QtTest/QtTest>
#include <QBuffer>
#include <QImage>

#define private public
#include "../globalmenumanager.h"
#undef private

class GlobalMenuManagerCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void invalidateMenuCache_removesKey()
    {
        GlobalMenuManager manager;
        const QString service = QStringLiteral("com.example.Menu");
        const QString path = QStringLiteral("/com/example/Menu");
        const QString key = manager.menuCacheKey(service, path);
        QVERIFY(!key.isEmpty());

        manager.m_menuItemsCache[key].insert(100, QVariantList{QVariantMap{
                                                  {QStringLiteral("id"), 100},
                                                  {QStringLiteral("label"), QStringLiteral("Item")},
                                              }});
        QVERIFY(manager.m_menuItemsCache.contains(key));

        manager.invalidateMenuCache(service, path);
        QVERIFY(!manager.m_menuItemsCache.contains(key));
    }

    void refresh_override_invalidatesOldBindingCache()
    {
        GlobalMenuManager manager;
        manager.m_activeMenuService = QStringLiteral("com.example.old");
        manager.m_activeMenuPath = QStringLiteral("/com/example/old");
        const QString oldKey = manager.menuCacheKey(manager.m_activeMenuService, manager.m_activeMenuPath);
        manager.m_menuItemsCache[oldKey].insert(200, QVariantList{QVariantMap{
                                                   {QStringLiteral("id"), 200},
                                                   {QStringLiteral("label"), QStringLiteral("Old")},
                                               }});
        QVERIFY(manager.m_menuItemsCache.contains(oldKey));

        manager.m_overrideEnabled = true;
        manager.m_overrideContext = QStringLiteral("override-test");
        manager.m_overrideMenus = QVariantList{
            QVariantMap{
                {QStringLiteral("id"), 1},
                {QStringLiteral("label"), QStringLiteral("File")},
                {QStringLiteral("enabled"), true},
            },
        };
        manager.refresh();

        QVERIFY(!manager.m_menuItemsCache.contains(oldKey));
    }

    void onLayoutChanged_invalidatesActiveBindingCache()
    {
        GlobalMenuManager manager;
        manager.m_overrideEnabled = false;
        manager.m_activeMenuService = QStringLiteral("com.example.live");
        manager.m_activeMenuPath = QStringLiteral("/com/example/live");
        const QString key = manager.menuCacheKey(manager.m_activeMenuService, manager.m_activeMenuPath);
        manager.m_menuItemsCache[key].insert(300, QVariantList{QVariantMap{
                                                  {QStringLiteral("id"), 300},
                                                  {QStringLiteral("label"), QStringLiteral("Live")},
                                              }});
        QVERIFY(manager.m_menuItemsCache.contains(key));

        manager.onActiveMenuLayoutChanged();
        QVERIFY(!manager.m_menuItemsCache.contains(key));
    }

    void parseDbusIconDataUri_acceptsSmallPngPayload()
    {
        QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::red);
        QByteArray pngBytes;
        QBuffer buf(&pngBytes);
        QVERIFY(buf.open(QIODevice::WriteOnly));
        QVERIFY(img.save(&buf, "PNG"));
        QVERIFY(!pngBytes.isEmpty());

        const QString uri = GlobalMenuManager::parseDbusIconDataUri(pngBytes);
        QVERIFY(!uri.isEmpty());
        QVERIFY(uri.startsWith(QStringLiteral("data:image/png;base64,")));
    }

    void parseDbusIconDataUri_rejectsOversizedRawPayload()
    {
        const QByteArray hugeBytes(600 * 1024, 'x');
        const QString uri = GlobalMenuManager::parseDbusIconDataUri(hugeBytes);
        QVERIFY(uri.isEmpty());
    }

    void parseDbusIconDataUri_rejectsOversizedTextPayload()
    {
        const QString hugeBase64(2 * 1024 * 1024, QLatin1Char('A'));
        const QString uri = GlobalMenuManager::parseDbusIconDataUri(hugeBase64);
        QVERIFY(uri.isEmpty());
    }
};

QTEST_MAIN(GlobalMenuManagerCacheTest)
#include "globalmenumanager_cache_test.moc"
