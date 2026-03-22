#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "../../src/filemanager/ops/fileoperationsmanager.h"
#include "../../src/filemanager/ops/fileoperationserrors.h"

class FileOperationsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void invalidDestination_emitsCompatAndDetailErrors()
    {
        FileOperationsManager manager;
        QSignalSpy errorSpy(&manager, SIGNAL(Error(QString)));
        QSignalSpy errorDetailSpy(&manager, SIGNAL(ErrorDetail(QString,QString,QString)));
        QSignalSpy finishedSpy(&manager, SIGNAL(Finished(QString)));
        QVERIFY(errorSpy.isValid());
        QVERIFY(errorDetailSpy.isValid());
        QVERIFY(finishedSpy.isValid());

        QStringList order;
        QString jobId;
        connect(&manager, &FileOperationsManager::Error, &manager, [&](const QString &id) {
            if (id == jobId) {
                order.push_back(QStringLiteral("Error"));
            }
        });
        connect(&manager,
                &FileOperationsManager::ErrorDetail,
                &manager,
                [&](const QString &id, const QString &, const QString &) {
                    if (id == jobId) {
                        order.push_back(QStringLiteral("ErrorDetail"));
                    }
                });

        const QString badDestination = QStringLiteral("/__slm_desktop_should_not_exist__/dest");
        jobId = manager.Copy(QStringList{QStringLiteral("/tmp/irrelevant")}, badDestination);
        QVERIFY(!jobId.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(errorDetailSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(order.size(), 2, 2000);

        QCOMPARE(order.at(0), QStringLiteral("Error"));
        QCOMPARE(order.at(1), QStringLiteral("ErrorDetail"));

        const QList<QVariant> errArgs = errorSpy.takeFirst();
        QCOMPARE(errArgs.at(0).toString(), jobId);

        const QList<QVariant> detailArgs = errorDetailSpy.takeFirst();
        QCOMPARE(detailArgs.at(0).toString(), jobId);
        QCOMPARE(detailArgs.at(1).toString(),
                 QString::fromLatin1(SlmFileOperationErrors::kInvalidDestination));
        QCOMPARE(detailArgs.at(2).toString(), badDestination);
    }

    void protectedTarget_isRejected()
    {
        FileOperationsManager manager;
        QSignalSpy errorDetailSpy(&manager, SIGNAL(ErrorDetail(QString,QString,QString)));
        QSignalSpy finishedSpy(&manager, SIGNAL(Finished(QString)));
        QVERIFY(errorDetailSpy.isValid());
        QVERIFY(finishedSpy.isValid());

        const QString jobId = manager.Delete(QStringList{QStringLiteral("/")});
        QVERIFY(!jobId.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(errorDetailSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);

        const QList<QVariant> detailArgs = errorDetailSpy.takeFirst();
        QCOMPARE(detailArgs.at(0).toString(), jobId);
        QCOMPARE(detailArgs.at(1).toString(),
                 QString::fromLatin1(SlmFileOperationErrors::kProtectedTarget));
    }

    void homeRoot_isRejected()
    {
        FileOperationsManager manager;
        QSignalSpy errorDetailSpy(&manager, SIGNAL(ErrorDetail(QString,QString,QString)));
        QSignalSpy finishedSpy(&manager, SIGNAL(Finished(QString)));
        QVERIFY(errorDetailSpy.isValid());
        QVERIFY(finishedSpy.isValid());

        const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QVERIFY(!home.trimmed().isEmpty());
        const QString jobId = manager.Delete(QStringList{home});
        QVERIFY(!jobId.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(errorDetailSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
        const QList<QVariant> detailArgs = errorDetailSpy.takeFirst();
        QCOMPARE(detailArgs.at(0).toString(), jobId);
        QCOMPARE(detailArgs.at(1).toString(),
                 QString::fromLatin1(SlmFileOperationErrors::kProtectedTarget));
    }

    void symlinkToCritical_isRejected()
    {
        FileOperationsManager manager;
        QSignalSpy errorDetailSpy(&manager, SIGNAL(ErrorDetail(QString,QString,QString)));
        QSignalSpy finishedSpy(&manager, SIGNAL(Finished(QString)));
        QVERIFY(errorDetailSpy.isValid());
        QVERIFY(finishedSpy.isValid());

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString linkPath = tmp.filePath(QStringLiteral("root-link"));
        QVERIFY(QFile::link(QStringLiteral("/"), linkPath));

        const QString jobId = manager.Delete(QStringList{linkPath});
        QVERIFY(!jobId.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(errorDetailSpy.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
        const QList<QVariant> detailArgs = errorDetailSpy.takeFirst();
        QCOMPARE(detailArgs.at(0).toString(), jobId);
        QCOMPARE(detailArgs.at(1).toString(),
                 QString::fromLatin1(SlmFileOperationErrors::kProtectedTarget));
    }
};

QTEST_MAIN(FileOperationsManagerTest)
#include "fileoperationsmanager_test.moc"
