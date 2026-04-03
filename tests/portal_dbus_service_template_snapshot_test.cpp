#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QMap>

class PortalDbusServiceTemplateSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void dbusServiceTemplate_snapshot_contract();

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

void PortalDbusServiceTemplateSnapshotTest::dbusServiceTemplate_snapshot_contract()
{
    const QString path = templatePath(QStringLiteral("org.freedesktop.impl.portal.desktop.slm.service"));
    QVERIFY2(QFileInfo::exists(path), "missing org.freedesktop.impl.portal.desktop.slm.service template");

    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open service template");

    bool inServiceSection = false;
    QString serviceName;
    QString execPath;
    QMap<QString, QString> parsed;

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inServiceSection = (line == QStringLiteral("[D-BUS Service]"));
            continue;
        }
        if (!inServiceSection) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = normalize(line.left(eq));
        const QString value = normalize(line.mid(eq + 1));
        parsed.insert(key, value);
        if (key == QStringLiteral("Name")) {
            serviceName = value;
        } else if (key == QStringLiteral("Exec")) {
            execPath = value;
        }
    }

    QCOMPARE(serviceName, QStringLiteral("org.freedesktop.impl.portal.desktop.slm"));
    QCOMPARE(execPath, QStringLiteral("%h/Development/Qt/Slm_Desktop/build/slm-portald"));
    QCOMPARE(parsed.size(), 2); // Keep contract tight: only Name + Exec in service section.
}

QTEST_GUILESS_MAIN(PortalDbusServiceTemplateSnapshotTest)
#include "portal_dbus_service_template_snapshot_test.moc"
