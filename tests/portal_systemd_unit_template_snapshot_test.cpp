#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QMap>

class PortalSystemdUnitTemplateSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void systemdUnitTemplate_snapshot_contract();

private:
    static QString templatePath()
    {
        return QStringLiteral(SYSTEMD_TEMPLATES_DIR) + QLatin1String("/slm-portald.service");
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

void PortalSystemdUnitTemplateSnapshotTest::systemdUnitTemplate_snapshot_contract()
{
    const QString path = templatePath();
    QVERIFY2(QFileInfo::exists(path), "missing scripts/systemd/slm-portald.service template");

    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open systemd unit template");

    QString currentSection;
    QMap<QString, QString> unit;
    QMap<QString, QString> service;
    QMap<QString, QString> install;

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line;
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = normalize(line.left(eq));
        const QString value = normalize(line.mid(eq + 1));

        if (currentSection == QStringLiteral("[Unit]")) {
            unit.insert(key, value);
        } else if (currentSection == QStringLiteral("[Service]")) {
            service.insert(key, value);
        } else if (currentSection == QStringLiteral("[Install]")) {
            install.insert(key, value);
        }
    }

    QCOMPARE(unit.value(QStringLiteral("Description")), QStringLiteral("SLM Desktop Portal Daemon"));
    QCOMPARE(unit.value(QStringLiteral("After")), QStringLiteral("default.target"));

    QCOMPARE(service.value(QStringLiteral("Type")), QStringLiteral("simple"));
    QCOMPARE(service.value(QStringLiteral("ExecStart")),
             QStringLiteral("%h/Development/Qt/Slm_Desktop/build/slm-portald"));
    QCOMPARE(service.value(QStringLiteral("Restart")), QStringLiteral("on-failure"));
    QCOMPARE(service.value(QStringLiteral("RestartSec")), QStringLiteral("1"));

    QCOMPARE(install.value(QStringLiteral("WantedBy")), QStringLiteral("default.target"));

    // Keep contract explicit and stable.
    QCOMPARE(unit.size(), 2);
    QCOMPARE(service.size(), 4);
    QCOMPARE(install.size(), 1);
}

QTEST_GUILESS_MAIN(PortalSystemdUnitTemplateSnapshotTest)
#include "portal_systemd_unit_template_snapshot_test.moc"
