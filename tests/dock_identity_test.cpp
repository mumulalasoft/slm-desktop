#include <QtTest/QtTest>
#include "../dockidentity.h"
using namespace DockIdentity;

class DockIdentityTest : public QObject {
    Q_OBJECT

private slots:
    void weakToken_shouldNotBeStrong()
    {
        QVERIFY(isWeakIdentityToken(QStringLiteral("code")));
        QVERIFY(!isStrongIdentityToken(QStringLiteral("code")));
        QVERIFY(isWeakIdentityToken(QStringLiteral("flatpak")));
    }

    void strongToken_shouldBeAccepted()
    {
        QVERIFY(isStrongIdentityToken(QStringLiteral("com.visualstudio.code")));
        QVERIFY(isStrongIdentityToken(QStringLiteral("io.elementary.music")));
        QVERIFY(isStrongIdentityToken(QStringLiteral("onlyoffice-desktopeditors")));
    }

    void launchHintTokens_shouldDropWeakTokens()
    {
        QSet<QString> in{
            QStringLiteral("code"),
            QStringLiteral("flatpak"),
            QStringLiteral("com.visualstudio.code"),
            QStringLiteral("io.elementary.music.desktop"),
        };
        const QSet<QString> out = filterLaunchHintTokens(in);
        QVERIFY(!out.contains(QStringLiteral("code")));
        QVERIFY(!out.contains(QStringLiteral("flatpak")));
        QVERIFY(out.contains(QStringLiteral("com.visualstudio.code")));
        QVERIFY(out.contains(QStringLiteral("io.elementary.music.desktop")));
    }
};

QTEST_MAIN(DockIdentityTest)
#include "dock_identity_test.moc"
