#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSet>

class PortalPortalsConfStrategyTest : public QObject
{
    Q_OBJECT

private slots:
    void templates_exist();
    void mvp_profile_prefers_fallback_default_and_core_slm_overrides();
    void beta_profile_promotes_expected_interfaces_to_slm();
    void production_profile_defaults_to_slm_with_targeted_fallbacks();
    void slmMappedInterfaces_areAdvertisedBySlmPortal();

private:
    static QString normalizeIface(QString value)
    {
        value = value.trimmed();
        while (value.endsWith(QLatin1Char(';'))) {
            value.chop(1);
            value = value.trimmed();
        }
        return value.toLower();
    }

    static QString templatePath(const QString &name)
    {
        return QStringLiteral(PORTAL_TEMPLATES_DIR) + QLatin1Char('/') + name;
    }

    static QSettings settingsFor(const QString &path)
    {
        return QSettings(path, QSettings::IniFormat);
    }

    static QSet<QString> advertisedInterfaces()
    {
        QFile f(templatePath(QStringLiteral("slm.portal")));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QString raw;
        bool inPortal = false;
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                inPortal = (line == QStringLiteral("[portal]"));
                continue;
            }
            if (!inPortal) {
                continue;
            }
            const QString prefix = QStringLiteral("Interfaces=");
            if (line.startsWith(prefix)) {
                raw = line.mid(prefix.size()).trimmed();
                break;
            }
        }
        QSet<QString> out;
        const QStringList rows = raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &row : rows) {
            const QString iface = normalizeIface(row);
            if (!iface.isEmpty()) {
                out.insert(iface);
            }
        }
        return out;
    }

    static QSet<QString> slmMappingsFromProfile(const QString &profileFile)
    {
        QSet<QString> out;
        QFile f(templatePath(profileFile));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return out;
        }
        bool inPreferred = false;
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
            const QString key = normalizeIface(raw.left(eq));
            const QString value = raw.mid(eq + 1).trimmed().toLower();
            if (key == QStringLiteral("default")) {
                continue;
            }
            if (value == QStringLiteral("slm")) {
                out.insert(key);
            }
        }
        return out;
    }
};

void PortalPortalsConfStrategyTest::templates_exist()
{
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.mvp.conf"))),
             "missing portals.mvp.conf");
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.beta.conf"))),
             "missing portals.beta.conf");
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("portals.production.conf"))),
             "missing portals.production.conf");
}

void PortalPortalsConfStrategyTest::mvp_profile_prefers_fallback_default_and_core_slm_overrides()
{
    QSettings s = settingsFor(templatePath(QStringLiteral("portals.mvp.conf")));
    s.beginGroup(QStringLiteral("preferred"));
    QCOMPARE(s.value(QStringLiteral("default")).toString(), QStringLiteral("gtk"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.FileChooser")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.OpenURI")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Screenshot")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Settings")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Notification")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Inhibit")).toString(),
             QStringLiteral("slm"));
    QVERIFY(s.value(QStringLiteral("org.freedesktop.impl.portal.ScreenCast")).toString().isEmpty());
    s.endGroup();
}

void PortalPortalsConfStrategyTest::beta_profile_promotes_expected_interfaces_to_slm()
{
    QSettings s = settingsFor(templatePath(QStringLiteral("portals.beta.conf")));
    s.beginGroup(QStringLiteral("preferred"));
    QCOMPARE(s.value(QStringLiteral("default")).toString(), QStringLiteral("gtk"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.ScreenCast")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Documents")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Trash")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.OpenWith")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts")).toString(),
             QStringLiteral("slm"));

    const QString remoteDesktop = s.value(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"))
                                      .toString();
    const QString camera = s.value(QStringLiteral("org.freedesktop.impl.portal.Camera")).toString();
    const QString location = s.value(QStringLiteral("org.freedesktop.impl.portal.Location")).toString();
    const QString inputCapture = s.value(QStringLiteral("org.freedesktop.impl.portal.InputCapture"))
                                     .toString();
    QVERIFY(!remoteDesktop.isEmpty());
    QVERIFY(!camera.isEmpty());
    QVERIFY(!location.isEmpty());
    QVERIFY(!inputCapture.isEmpty());
    QVERIFY(!remoteDesktop.contains(QStringLiteral("slm")));
    QVERIFY(!camera.contains(QStringLiteral("slm")));
    QVERIFY(!location.contains(QStringLiteral("slm")));
    QVERIFY(!inputCapture.contains(QStringLiteral("slm")));
    s.endGroup();
}

void PortalPortalsConfStrategyTest::production_profile_defaults_to_slm_with_targeted_fallbacks()
{
    QSettings s = settingsFor(templatePath(QStringLiteral("portals.production.conf")));
    s.beginGroup(QStringLiteral("preferred"));
    QCOMPARE(s.value(QStringLiteral("default")).toString(), QStringLiteral("slm"));

    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.FileChooser")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.OpenURI")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.ScreenCast")).toString(),
             QStringLiteral("slm"));
    QCOMPARE(s.value(QStringLiteral("org.freedesktop.impl.portal.Documents")).toString(),
             QStringLiteral("slm"));

    const QString remoteDesktop = s.value(QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"))
                                      .toString();
    const QString camera = s.value(QStringLiteral("org.freedesktop.impl.portal.Camera")).toString();
    const QString location = s.value(QStringLiteral("org.freedesktop.impl.portal.Location")).toString();
    const QString inputCapture = s.value(QStringLiteral("org.freedesktop.impl.portal.InputCapture"))
                                     .toString();
    QVERIFY(!remoteDesktop.isEmpty());
    QVERIFY(!camera.isEmpty());
    QVERIFY(!location.isEmpty());
    QVERIFY(!inputCapture.isEmpty());
    QVERIFY(!remoteDesktop.contains(QStringLiteral("slm")));
    QVERIFY(!camera.contains(QStringLiteral("slm")));
    QVERIFY(!location.contains(QStringLiteral("slm")));
    QVERIFY(!inputCapture.contains(QStringLiteral("slm")));
    s.endGroup();
}

void PortalPortalsConfStrategyTest::slmMappedInterfaces_areAdvertisedBySlmPortal()
{
    QVERIFY2(QFileInfo::exists(templatePath(QStringLiteral("slm.portal"))), "missing slm.portal");
    const QSet<QString> advertised = advertisedInterfaces();
    QVERIFY(!advertised.isEmpty());

    const QStringList profiles{
        QStringLiteral("portals.mvp.conf"),
        QStringLiteral("portals.beta.conf"),
        QStringLiteral("portals.production.conf"),
    };

    for (const QString &profile : profiles) {
        const QSet<QString> mapped = slmMappingsFromProfile(profile);
        for (const QString &iface : mapped) {
            QVERIFY2(advertised.contains(iface),
                     qPrintable(QStringLiteral("%1 maps '%2=slm' but slm.portal does not advertise it")
                                    .arg(profile, iface)));
        }
    }
}

QTEST_GUILESS_MAIN(PortalPortalsConfStrategyTest)
#include "portal_portals_conf_strategy_test.moc"
