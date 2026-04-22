#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>
#include <QStringList>

namespace {
QString normalizeForQjsEval(const QString &script)
{
    QStringList out;
    const QStringList lines = script.split(QLatin1Char('\n'));
    out.reserve(lines.size());
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral(".pragma"))
            || trimmed.startsWith(QStringLiteral(".import"))) {
            continue;
        }
        out.push_back(line);
    }
    return out.join(QLatin1Char('\n'));
}
}

class PulseControllerJsTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshResults_handlesUndefinedResultsModel_contract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                             + QStringLiteral("/Qml/components/shell/PulseController.js");
        QVERIFY2(QFileInfo::exists(path), "PulseController.js is missing");

        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                 "cannot open PulseController.js");
        const QString script = normalizeForQjsEval(QString::fromUtf8(file.readAll()));

        QJSEngine engine;
        engine.globalObject().setProperty(QStringLiteral("ShellUtils"), engine.newObject());

        const QJSValue eval = engine.evaluate(script, path);
        QVERIFY2(!eval.isError(), qPrintable(eval.toString()));

        const QJSValue fn = engine.globalObject().property(QStringLiteral("refreshResults"));
        QVERIFY2(fn.isCallable(), "refreshResults is not callable");

        QJSValue shell = engine.newObject();
        shell.setProperty(QStringLiteral("pulseQueryGeneration"), 7);
        shell.setProperty(QStringLiteral("pulseQuery"), QStringLiteral("hello"));
        shell.setProperty(QStringLiteral("pulseSelectedIndex"), 3);
        shell.setProperty(QStringLiteral("pulseProviderStats"), engine.newObject());
        shell.setProperty(QStringLiteral("pulsePreviewData"), engine.newObject());

        const QJSValue result = fn.call(QJSValueList{
            shell,
            QJSValue(QJSValue::SpecialValue::UndefinedValue),
            QJSValue(false)
        });
        QVERIFY2(!result.isError(), qPrintable(result.toString()));

        QCOMPARE(shell.property(QStringLiteral("pulseSelectedIndex")).toInt(), -1);
        QVERIFY(shell.property(QStringLiteral("pulseProviderStats")).isObject());
        QVERIFY(shell.property(QStringLiteral("pulsePreviewData")).isObject());
    }
};

QTEST_GUILESS_MAIN(PulseControllerJsTest)
#include "pulse_controller_js_test.moc"
