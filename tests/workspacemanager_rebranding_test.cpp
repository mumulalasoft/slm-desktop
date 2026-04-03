#include <QtTest/QtTest>
#include <QMetaObject>
#include <QMetaMethod>

#include "src/core/workspace/workspacemanager.h"

class WorkspaceManagerRebrandingTest : public QObject
{
    Q_OBJECT

private slots:
    void testAliasMethodsPresent()
    {
        WorkspaceManager manager(nullptr, nullptr, nullptr);
        const QMetaObject *mo = manager.metaObject();
        
        QSet<QString> methods;
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            methods.insert(QString::fromLatin1(mo->method(i).name()));
        }

        // Old aliases
        QVERIFY(methods.contains("ShowOverview"));
        QVERIFY(methods.contains("HideOverview"));
        QVERIFY(methods.contains("ToggleOverview"));

        // New canonical names
        QVERIFY(methods.contains("ShowWorkspace"));
        QVERIFY(methods.contains("HideWorkspace"));
        QVERIFY(methods.contains("ToggleWorkspace"));
    }

    void testAliasSignalsPresent()
    {
        WorkspaceManager manager(nullptr, nullptr, nullptr);
        const QMetaObject *mo = manager.metaObject();
        
        QSet<QString> signalNames;
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            if (mo->method(i).methodType() == QMetaMethod::Signal) {
                signalNames.insert(QString::fromLatin1(mo->method(i).name()));
            }
        }

        QVERIFY(signalNames.contains("OverviewShown"));
        QVERIFY(signalNames.contains("OverviewVisibilityChanged"));
        QVERIFY(signalNames.contains("WorkspaceShown"));
        QVERIFY(signalNames.contains("WorkspaceVisibilityChanged"));
    }
};

QTEST_MAIN(WorkspaceManagerRebrandingTest)
#include "workspacemanager_rebranding_test.moc"
