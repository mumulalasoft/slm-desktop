#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QMap>

class PortalProfileSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void mvp_profile_snapshot_contract();
    void beta_profile_snapshot_contract();
    void production_profile_snapshot_contract();

private:
    using KV = QMap<QString, QString>;

    static QString normalize(QString value)
    {
        value = value.trimmed();
        while (value.endsWith(QLatin1Char(';'))) {
            value.chop(1);
            value = value.trimmed();
        }
        return value;
    }

    static QString templatePath(const QString &name)
    {
        return QStringLiteral(PORTAL_TEMPLATES_DIR) + QLatin1Char('/') + name;
    }

    static KV parsePreferred(const QString &profileFile)
    {
        QFile f(templatePath(profileFile));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        bool inPreferred = false;
        KV out;
        while (!f.atEnd()) {
            const QString raw = QString::fromUtf8(f.readLine()).trimmed();
            if (raw.isEmpty() || raw.startsWith(QLatin1Char('#')) || raw.startsWith(QLatin1Char(';'))) {
                continue;
            }
            if (raw.startsWith(QLatin1Char('[')) && raw.endsWith(QLatin1Char(']'))) {
                inPreferred = (raw == QStringLiteral("[preferred]"));
                continue;
            }
            if (!inPreferred) {
                continue;
            }
            const int eq = raw.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                continue;
            }
            const QString key = normalize(raw.left(eq));
            const QString value = normalize(raw.mid(eq + 1));
            out.insert(key, value);
        }
        return out;
    }

    static void assertMapEquals(const KV &actual, const KV &expected, const QString &profile)
    {
        QVERIFY2(actual.size() == expected.size(),
                 qPrintable(profile + QStringLiteral(": mapping size mismatch (actual=")
                            + QString::number(actual.size()) + QStringLiteral(", expected=")
                            + QString::number(expected.size()) + QStringLiteral(")")));
        for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
            QVERIFY2(actual.contains(it.key()),
                     qPrintable(profile + QStringLiteral(": missing key '") + it.key()
                                + QStringLiteral("'")));
            QVERIFY2(actual.value(it.key()) == it.value(),
                     qPrintable(profile + QStringLiteral(": unexpected value for '") + it.key()
                                + QStringLiteral("' (actual='") + actual.value(it.key())
                                + QStringLiteral("' expected='") + it.value() + QStringLiteral("')")));
        }
    }
};

void PortalProfileSnapshotTest::mvp_profile_snapshot_contract()
{
    const KV actual = parsePreferred(QStringLiteral("portals.mvp.conf"));
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.mvp.conf"))),
             "missing portals.mvp.conf");
    const KV expected{
        {QStringLiteral("default"), QStringLiteral("gtk")},
        {QStringLiteral("org.freedesktop.impl.portal.FileChooser"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.OpenURI"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Settings"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Notification"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Inhibit"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Screenshot"), QStringLiteral("slm")},
    };
    assertMapEquals(actual, expected, QStringLiteral("portals.mvp.conf"));
}

void PortalProfileSnapshotTest::beta_profile_snapshot_contract()
{
    const KV actual = parsePreferred(QStringLiteral("portals.beta.conf"));
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.beta.conf"))),
             "missing portals.beta.conf");
    const KV expected{
        {QStringLiteral("default"), QStringLiteral("gtk")},
        {QStringLiteral("org.freedesktop.impl.portal.FileChooser"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.OpenURI"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Screenshot"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.ScreenCast"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Settings"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Notification"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Inhibit"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.OpenWith"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Documents"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Trash"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("wlr;kde;gtk")},
        {QStringLiteral("org.freedesktop.impl.portal.Camera"), QStringLiteral("gtk;kde")},
        {QStringLiteral("org.freedesktop.impl.portal.Location"), QStringLiteral("gtk;kde")},
        {QStringLiteral("org.freedesktop.impl.portal.InputCapture"), QStringLiteral("wlr;kde;gtk")},
    };
    assertMapEquals(actual, expected, QStringLiteral("portals.beta.conf"));
}

void PortalProfileSnapshotTest::production_profile_snapshot_contract()
{
    const KV actual = parsePreferred(QStringLiteral("portals.production.conf"));
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.production.conf"))),
             "missing portals.production.conf");
    const KV expected{
        {QStringLiteral("default"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.FileChooser"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.OpenURI"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Screenshot"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.ScreenCast"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Settings"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Notification"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Inhibit"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.OpenWith"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Documents"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.Trash"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts"), QStringLiteral("slm")},
        {QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"), QStringLiteral("wlr;kde;gtk")},
        {QStringLiteral("org.freedesktop.impl.portal.Camera"), QStringLiteral("gtk;kde")},
        {QStringLiteral("org.freedesktop.impl.portal.Location"), QStringLiteral("gtk;kde")},
        {QStringLiteral("org.freedesktop.impl.portal.InputCapture"), QStringLiteral("wlr;kde;gtk")},
    };
    assertMapEquals(actual, expected, QStringLiteral("portals.production.conf"));
}

QTEST_GUILESS_MAIN(PortalProfileSnapshotTest)
#include "portal_profile_snapshot_test.moc"
