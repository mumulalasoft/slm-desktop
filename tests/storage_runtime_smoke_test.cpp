#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Storage";
constexpr const char kPath[] = "/org/slm/Desktop/Storage";
constexpr const char kIface[] = "org.slm.Desktop.Storage";

bool envEnabled(const char *name)
{
    const QByteArray raw = qgetenv(name).trimmed().toLower();
    return raw == "1" || raw == "true" || raw == "yes" || raw == "on";
}

bool requireService()
{
    return envEnabled("SLM_STORAGE_SMOKE_REQUIRE_SERVICE");
}

QString persistencePhase()
{
    return QString::fromUtf8(qgetenv("SLM_STORAGE_SMOKE_POLICY_PHASE")).trimmed().toLower();
}

QString persistenceStateFilePath()
{
    const QString raw = QString::fromUtf8(qgetenv("SLM_STORAGE_SMOKE_POLICY_STATE_FILE")).trimmed();
    if (!raw.isEmpty()) {
        return raw;
    }
    return QStringLiteral("/tmp/slm-storage-policy-state.json");
}

bool writePersistenceStateFile(const QString &path,
                               const QString &scope,
                               bool beforeReadOnly,
                               bool afterReadOnly,
                               QString *errorOut = nullptr)
{
    const QString filePath = persistenceStateFilePath();
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = QStringLiteral("failed-open-state-file");
        }
        return false;
    }
    QJsonObject root{
        {QStringLiteral("path"), path},
        {QStringLiteral("scope"), scope},
        {QStringLiteral("before_read_only"), beforeReadOnly},
        {QStringLiteral("after_read_only"), afterReadOnly},
    };
    if (f.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("failed-write-state-file");
        }
        return false;
    }
    return true;
}

QJsonObject readPersistenceStateFile(QString *errorOut = nullptr)
{
    QFile f(persistenceStateFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = QStringLiteral("failed-open-state-file");
        }
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("invalid-state-file");
        }
        return {};
    }
    return doc.object();
}

} // namespace

class StorageRuntimeSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void storageLocations_runtimeShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            if (requireService()) {
                QVERIFY2(false, "session bus is not available in this test environment");
            }
            QSKIP("session bus is not available in this test environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        if (!iface.isValid()) {
            if (requireService()) {
                QVERIFY2(false, "org.slm.Desktop.Storage is not registered");
            }
            QSKIP("org.slm.Desktop.Storage is not registered");
        }

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

        const QVariantMap out = reply.value();
        QVERIFY(out.contains(QStringLiteral("ok")));
        QVERIFY(out.value(QStringLiteral("ok")).toBool());

        const QVariantList rows = out.value(QStringLiteral("rows")).toList();
        for (const QVariant &v : rows) {
            const QVariantMap row = v.toMap();
            QVERIFY(row.contains(QStringLiteral("path")) || row.contains(QStringLiteral("rootPath")));
            QVERIFY(row.contains(QStringLiteral("device")));
            QVERIFY(row.contains(QStringLiteral("policyAction")));
            QVERIFY(row.contains(QStringLiteral("policyVisible")));
            QVERIFY(row.contains(QStringLiteral("policyScope")));
            QVERIFY(row.contains(QStringLiteral("policyInput")));
            QVERIFY(row.contains(QStringLiteral("policyOutput")));

            const QVariantMap policyInput = row.value(QStringLiteral("policyInput")).toMap();
            QVERIFY(policyInput.contains(QStringLiteral("uuid")));
            QVERIFY(policyInput.contains(QStringLiteral("partuuid")));
            QVERIFY(policyInput.contains(QStringLiteral("fstype")));
            QVERIFY(policyInput.contains(QStringLiteral("is_removable")));
            QVERIFY(policyInput.contains(QStringLiteral("is_system")));
            QVERIFY(policyInput.contains(QStringLiteral("is_encrypted")));

            const QVariantMap policyOutput = row.value(QStringLiteral("policyOutput")).toMap();
            QVERIFY(policyOutput.contains(QStringLiteral("action")));
            QVERIFY(policyOutput.contains(QStringLiteral("auto_open")));
            QVERIFY(policyOutput.contains(QStringLiteral("visible")));
            QVERIFY(policyOutput.contains(QStringLiteral("read_only")));
            QVERIFY(policyOutput.contains(QStringLiteral("exec")));
        }
    }

    void multiPartition_optionalHardwareCheck()
    {
        if (!envEnabled("SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION")) {
            QSKIP("set SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION=1 to enable this check");
        }

        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            if (requireService()) {
                QVERIFY2(false, "session bus is not available in this test environment");
            }
            QSKIP("session bus is not available in this test environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        if (!iface.isValid()) {
            if (requireService()) {
                QVERIFY2(false, "org.slm.Desktop.Storage is not registered");
            }
            QSKIP("org.slm.Desktop.Storage is not registered");
        }

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        const QVariantList rows = reply.value().value(QStringLiteral("rows")).toList();

        QHash<QString, int> countByGroup;
        for (const QVariant &v : rows) {
            const QVariantMap row = v.toMap();
            if (!row.value(QStringLiteral("policyVisible"), true).toBool()) {
                continue;
            }
            const QString group = row.value(QStringLiteral("deviceGroupKey")).toString().trimmed();
            if (group.isEmpty()) {
                continue;
            }
            countByGroup[group] = countByGroup.value(group, 0) + 1;
        }

        bool hasMultiPartition = false;
        for (auto it = countByGroup.constBegin(); it != countByGroup.constEnd(); ++it) {
            if (it.value() >= 2) {
                hasMultiPartition = true;
                break;
            }
        }

        QVERIFY2(hasMultiPartition,
                 "No multi-partition device detected. Connect USB with >=2 visible partitions.");
    }

    void encrypted_optionalHardwareCheck()
    {
        if (!envEnabled("SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED")) {
            QSKIP("set SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED=1 to enable this check");
        }

        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            if (requireService()) {
                QVERIFY2(false, "session bus is not available in this test environment");
            }
            QSKIP("session bus is not available in this test environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        if (!iface.isValid()) {
            if (requireService()) {
                QVERIFY2(false, "org.slm.Desktop.Storage is not registered");
            }
            QSKIP("org.slm.Desktop.Storage is not registered");
        }

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        const QVariantList rows = reply.value().value(QStringLiteral("rows")).toList();

        bool hasEncrypted = false;
        for (const QVariant &v : rows) {
            const QVariantMap row = v.toMap();
            if (!row.value(QStringLiteral("isEncrypted")).toBool()) {
                continue;
            }
            hasEncrypted = true;
            const QString action = row.value(QStringLiteral("policyAction")).toString().trimmed();
            QVERIFY2(action == QStringLiteral("ask") || action == QStringLiteral("mount"),
                     "Encrypted row has unexpected policyAction");
        }

        QVERIFY2(hasEncrypted,
                 "No encrypted storage detected. Connect encrypted media and retry.");
    }

    void policyPersistence_optionalHardwareCheck()
    {
        if (!envEnabled("SLM_STORAGE_SMOKE_REQUIRE_POLICY_PERSISTENCE")) {
            QSKIP("set SLM_STORAGE_SMOKE_REQUIRE_POLICY_PERSISTENCE=1 to enable this check");
        }

        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            if (requireService()) {
                QVERIFY2(false, "session bus is not available in this test environment");
            }
            QSKIP("session bus is not available in this test environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        if (!iface.isValid()) {
            if (requireService()) {
                QVERIFY2(false, "org.slm.Desktop.Storage is not registered");
            }
            QSKIP("org.slm.Desktop.Storage is not registered");
        }

        QDBusReply<QVariantMap> rowsReply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY2(rowsReply.isValid(), qPrintable(rowsReply.error().message()));
        const QVariantList rows = rowsReply.value().value(QStringLiteral("rows")).toList();
        const QString phase = persistencePhase();

        if (phase == QStringLiteral("verify")) {
            QString stateErr;
            const QJsonObject state = readPersistenceStateFile(&stateErr);
            QVERIFY2(!state.isEmpty(), qPrintable(QStringLiteral("failed to read persistence state: %1").arg(stateErr)));

            const QString path = state.value(QStringLiteral("path")).toString().trimmed();
            const QString scope = state.value(QStringLiteral("scope")).toString().trimmed();
            const bool beforeReadOnly = state.value(QStringLiteral("before_read_only")).toBool();
            const bool afterReadOnly = state.value(QStringLiteral("after_read_only")).toBool();
            QVERIFY2(!path.isEmpty(), "state file path is empty");

            QDBusReply<QVariantMap> verifyReply = iface.call(QStringLiteral("StoragePolicyForPath"), path);
            QVERIFY2(verifyReply.isValid(), qPrintable(verifyReply.error().message()));
            const QVariantMap verifyOut = verifyReply.value();
            QVERIFY(verifyOut.value(QStringLiteral("ok")).toBool());
            const QVariantMap changedPolicy = verifyOut.value(QStringLiteral("policy")).toMap();
            QCOMPARE(changedPolicy.value(QStringLiteral("read_only")).toBool(), afterReadOnly);

            QVariantMap restorePatch;
            restorePatch.insert(QStringLiteral("read_only"), beforeReadOnly);
            QDBusReply<QVariantMap> restoreReply = iface.call(QStringLiteral("SetStoragePolicyForPath"),
                                                              path,
                                                              restorePatch,
                                                              scope.isEmpty()
                                                                  ? QStringLiteral("partition")
                                                                  : scope);
            QVERIFY2(restoreReply.isValid(), qPrintable(restoreReply.error().message()));
            QVERIFY(restoreReply.value().value(QStringLiteral("ok")).toBool());

            QDBusReply<QVariantMap> finalReply = iface.call(QStringLiteral("StoragePolicyForPath"), path);
            QVERIFY2(finalReply.isValid(), qPrintable(finalReply.error().message()));
            const QVariantMap finalOut = finalReply.value();
            QVERIFY(finalOut.value(QStringLiteral("ok")).toBool());
            const QVariantMap finalPolicy = finalOut.value(QStringLiteral("policy")).toMap();
            QCOMPARE(finalPolicy.value(QStringLiteral("read_only")).toBool(), beforeReadOnly);

            QFile::remove(persistenceStateFilePath());
            return;
        }

        bool verified = false;
        for (const QVariant &v : rows) {
            const QVariantMap row = v.toMap();
            const QString path = row.value(QStringLiteral("path")).toString().trimmed();
            if (path.isEmpty()) {
                continue;
            }
            if (!row.value(QStringLiteral("policyVisible"), true).toBool()) {
                continue;
            }

            QDBusReply<QVariantMap> readReply = iface.call(QStringLiteral("StoragePolicyForPath"), path);
            if (!readReply.isValid()) {
                continue;
            }
            const QVariantMap readOut = readReply.value();
            if (!readOut.value(QStringLiteral("ok")).toBool()) {
                continue;
            }
            const QVariantMap currentPolicy = readOut.value(QStringLiteral("policy")).toMap();
            if (currentPolicy.isEmpty()) {
                continue;
            }

            const bool beforeReadOnly = currentPolicy.value(QStringLiteral("read_only")).toBool();
            const QString scope = readOut.value(QStringLiteral("policyScope"),
                                                QStringLiteral("partition")).toString().trimmed();

            QVariantMap patch;
            patch.insert(QStringLiteral("read_only"), !beforeReadOnly);
            QDBusReply<QVariantMap> writeReply = iface.call(QStringLiteral("SetStoragePolicyForPath"),
                                                            path,
                                                            patch,
                                                            scope.isEmpty()
                                                                ? QStringLiteral("partition")
                                                                : scope);
            if (!writeReply.isValid() || !writeReply.value().value(QStringLiteral("ok")).toBool()) {
                continue;
            }

            QDBusReply<QVariantMap> verifyReply = iface.call(QStringLiteral("StoragePolicyForPath"), path);
            QVERIFY2(verifyReply.isValid(), qPrintable(verifyReply.error().message()));
            const QVariantMap verifyOut = verifyReply.value();
            QVERIFY(verifyOut.value(QStringLiteral("ok")).toBool());
            const QVariantMap changedPolicy = verifyOut.value(QStringLiteral("policy")).toMap();
            QCOMPARE(changedPolicy.value(QStringLiteral("read_only")).toBool(), !beforeReadOnly);

            if (phase == QStringLiteral("prepare")) {
                QString stateErr;
                QVERIFY2(writePersistenceStateFile(path,
                                                   scope.isEmpty() ? QStringLiteral("partition") : scope,
                                                   beforeReadOnly,
                                                   !beforeReadOnly,
                                                   &stateErr),
                         qPrintable(QStringLiteral("failed to write persistence state: %1").arg(stateErr)));
                verified = true;
                break;
            }

            QVariantMap restorePatch;
            restorePatch.insert(QStringLiteral("read_only"), beforeReadOnly);
            QDBusReply<QVariantMap> restoreReply = iface.call(QStringLiteral("SetStoragePolicyForPath"),
                                                              path,
                                                              restorePatch,
                                                              scope.isEmpty()
                                                                  ? QStringLiteral("partition")
                                                                  : scope);
            QVERIFY2(restoreReply.isValid(), qPrintable(restoreReply.error().message()));
            QVERIFY(restoreReply.value().value(QStringLiteral("ok")).toBool());

            QDBusReply<QVariantMap> finalReply = iface.call(QStringLiteral("StoragePolicyForPath"), path);
            QVERIFY2(finalReply.isValid(), qPrintable(finalReply.error().message()));
            const QVariantMap finalOut = finalReply.value();
            QVERIFY(finalOut.value(QStringLiteral("ok")).toBool());
            const QVariantMap finalPolicy = finalOut.value(QStringLiteral("policy")).toMap();
            QCOMPARE(finalPolicy.value(QStringLiteral("read_only")).toBool(), beforeReadOnly);

            verified = true;
            break;
        }

        QVERIFY2(verified,
                 "No suitable storage row found for policy persistence write/read/restore check.");
    }
};

QTEST_MAIN(StorageRuntimeSmokeTest)
#include "storage_runtime_smoke_test.moc"
