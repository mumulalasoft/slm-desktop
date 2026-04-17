#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PortalSecretConsentContractSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void portalSecretConsentContract_snapshot();
};

void PortalSecretConsentContractSnapshotTest::portalSecretConsentContract_snapshot()
{
    const QString path = QStringLiteral(CONTRACTS_DIR)
                         + QStringLiteral("/portal-secret-consent.md");
    QVERIFY2(QFileInfo::exists(path), "missing portal-secret-consent.md");

    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
             "cannot open portal-secret-consent.md");
    const QString text = QString::fromUtf8(f.readAll());

    QVERIFY2(text.contains(QStringLiteral("# Portal Secret Consent Contract")),
             "missing contract title");
    QVERIFY2(text.contains(QStringLiteral("Contract invariants:")),
             "missing invariants section");
    QVERIFY2(text.contains(QStringLiteral("StoreSecret` -> `Secret.Store")),
             "missing StoreSecret capability mapping");
    QVERIFY2(text.contains(QStringLiteral("GetSecret` -> `Secret.Read")),
             "missing GetSecret capability mapping");
    QVERIFY2(text.contains(QStringLiteral("DeleteSecret` -> `Secret.Delete")),
             "missing DeleteSecret capability mapping");
    QVERIFY2(text.contains(QStringLiteral("ClearAppSecrets` -> `Secret.Delete")),
             "missing ClearAppSecrets capability mapping");
    QVERIFY2(text.contains(QStringLiteral("StoreSecret`, `GetSecret`, `DeleteSecret` => `true`")),
             "missing persistentEligible=true matrix row");
    QVERIFY2(text.contains(QStringLiteral("ClearAppSecrets` => `false`")),
             "missing persistentEligible=false matrix row");
    QVERIFY2(text.contains(QStringLiteral("Re-prompt behavior:")),
             "missing reprompt behavior section");
    QVERIFY2(text.contains(QStringLiteral("Suite execution:")),
             "missing suite execution section");
    QVERIFY2(text.contains(QStringLiteral("scripts/test.sh secret-consent build/toppanel-Debug")),
             "missing unified runner command");
}

QTEST_GUILESS_MAIN(PortalSecretConsentContractSnapshotTest)
#include "portal_secret_consent_contract_snapshot_test.moc"
