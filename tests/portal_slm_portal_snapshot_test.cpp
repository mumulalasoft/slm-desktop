#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QSet>

class PortalSlmPortalSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void slmPortal_snapshot_contract();

private:
    static QString templatePath(const QString &name)
    {
        return QStringLiteral(PORTAL_TEMPLATES_DIR) + QLatin1Char('/') + name;
    }

    static QString normalize(QString value)
    {
        value = value.trimmed();
        while (value.endsWith(QLatin1Char(';'))) {
            value.chop(1);
            value = value.trimmed();
        }
        return value;
    }
};

void PortalSlmPortalSnapshotTest::slmPortal_snapshot_contract()
{
    const QString path = templatePath(QStringLiteral("slm.portal"));
    QVERIFY2(QFileInfo::exists(path), "missing slm.portal");

    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open slm.portal");

    QString dbusName;
    QString useIn;
    QString interfacesRaw;
    bool inPortal = false;

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inPortal = (line == QStringLiteral("[portal]"));
            continue;
        }
        if (!inPortal) {
            continue;
        }
        if (line.startsWith(QStringLiteral("DBusName="))) {
            dbusName = normalize(line.mid(QStringLiteral("DBusName=").size()));
        } else if (line.startsWith(QStringLiteral("UseIn="))) {
            useIn = normalize(line.mid(QStringLiteral("UseIn=").size()));
        } else if (line.startsWith(QStringLiteral("Interfaces="))) {
            interfacesRaw = normalize(line.mid(QStringLiteral("Interfaces=").size()));
        }
    }

    QCOMPARE(dbusName, QStringLiteral("org.freedesktop.impl.portal.desktop.slm"));
    QCOMPARE(useIn, QStringLiteral("SLM"));
    QVERIFY2(!interfacesRaw.isEmpty(), "Interfaces list is empty");

    QSet<QString> actual;
    const QStringList ifaceList = interfacesRaw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &iface : ifaceList) {
        actual.insert(normalize(iface));
    }

    const QSet<QString> expected{
        QStringLiteral("org.freedesktop.impl.portal.FileChooser"),
        QStringLiteral("org.freedesktop.impl.portal.OpenURI"),
        QStringLiteral("org.freedesktop.impl.portal.Screenshot"),
        QStringLiteral("org.freedesktop.impl.portal.ScreenCast"),
        QStringLiteral("org.freedesktop.impl.portal.Settings"),
        QStringLiteral("org.freedesktop.impl.portal.Notification"),
        QStringLiteral("org.freedesktop.impl.portal.Inhibit"),
        QStringLiteral("org.freedesktop.impl.portal.OpenWith"),
        QStringLiteral("org.freedesktop.impl.portal.Documents"),
        QStringLiteral("org.freedesktop.impl.portal.Trash"),
        QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts"),
        QStringLiteral("org.freedesktop.impl.portal.InputCapture"),
    };

    QVERIFY2(actual == expected,
             qPrintable(QStringLiteral("Unexpected Interfaces set in slm.portal.\nactual=%1\nexpected=%2")
                            .arg(QStringList(actual.values()).join(QStringLiteral(", ")),
                                 QStringList(expected.values()).join(QStringLiteral(", ")))));
}

QTEST_GUILESS_MAIN(PortalSlmPortalSnapshotTest)
#include "portal_slm_portal_snapshot_test.moc"
