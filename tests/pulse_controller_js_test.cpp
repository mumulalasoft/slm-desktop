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

    void calculator_supports_compound_semantics()
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

        const QJSValue calcFn = engine.globalObject().property(QStringLiteral("_pulseCalculatorEvaluate"));
        QVERIFY2(calcFn.isCallable(), "_pulseCalculatorEvaluate is not callable");
        const QJSValue parseFn = engine.globalObject().property(QStringLiteral("_pulseCalculatorParseUnitExpression"));
        QVERIFY2(parseFn.isCallable(), "_pulseCalculatorParseUnitExpression is not callable");

        auto callCalc = [&](const QString &expression) -> QJSValue {
            return calcFn.call(QJSValueList{QJSValue(expression)});
        };
        auto callParse = [&](const QString &expression) -> QJSValue {
            return parseFn.call(QJSValueList{QJSValue(expression)});
        };

        const QJSValue parsedTorque = callParse(QStringLiteral("newton meter"));
        QVERIFY2(!parsedTorque.isError(), qPrintable(parsedTorque.toString()));
        QCOMPARE(parsedTorque.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(parsedTorque.property(QStringLiteral("dimKey")).toString(), QStringLiteral("L:2|M:1|T:-2"));

        const QJSValue torque = callCalc(QStringLiteral("10 N*m to J"));
        QVERIFY2(!torque.isError(), qPrintable(torque.toString()));
        QCOMPARE(torque.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(torque.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Torque"));
        QCOMPARE(torque.property(QStringLiteral("formatted")).toString(), QStringLiteral("10 J"));

        const QJSValue power = callCalc(QStringLiteral("1 J/s to W"));
        QVERIFY2(!power.isError(), qPrintable(power.toString()));
        QCOMPARE(power.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(power.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Power"));
        QCOMPARE(power.property(QStringLiteral("formatted")).toString(), QStringLiteral("1 W"));

        const QJSValue pressure = callCalc(QStringLiteral("100000 Pa to bar"));
        QVERIFY2(!pressure.isError(), qPrintable(pressure.toString()));
        QCOMPARE(pressure.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(pressure.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Pressure"));
        QCOMPARE(pressure.property(QStringLiteral("formatted")).toString(), QStringLiteral("1 bar"));

        const QJSValue energy = callCalc(QStringLiteral("1 kWh to joule"));
        QVERIFY2(!energy.isError(), qPrintable(energy.toString()));
        QCOMPARE(energy.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(energy.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Energy"));
        QCOMPARE(energy.property(QStringLiteral("formatted")).toString(), QStringLiteral("3600000 joule"));

        const QJSValue joule = callCalc(QStringLiteral("1 joule to kWh"));
        QVERIFY2(!joule.isError(), qPrintable(joule.toString()));
        QCOMPARE(joule.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(joule.property(QStringLiteral("formatted")).toString(), QStringLiteral("2.77777777778e-7 kWh"));

        const QJSValue pascal = callCalc(QStringLiteral("100000 pascal to bar"));
        QVERIFY2(!pascal.isError(), qPrintable(pascal.toString()));
        QCOMPARE(pascal.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(pascal.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Pressure"));
        QCOMPARE(pascal.property(QStringLiteral("formatted")).toString(), QStringLiteral("1 bar"));

        const QJSValue pascals = callCalc(QStringLiteral("101325 pascals to atm"));
        QVERIFY2(!pascals.isError(), qPrintable(pascals.toString()));
        QCOMPARE(pascals.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(pascals.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Pressure"));
        QCOMPARE(pascals.property(QStringLiteral("formatted")).toString(), QStringLiteral("1 atm"));

        const QJSValue watthour = callCalc(QStringLiteral("1 watthour to joule"));
        QVERIFY2(!watthour.isError(), qPrintable(watthour.toString()));
        QCOMPARE(watthour.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(watthour.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Energy"));
        QCOMPARE(watthour.property(QStringLiteral("formatted")).toString(), QStringLiteral("3600 joule"));

        const QJSValue kilowatthour = callCalc(QStringLiteral("1 kilowatthour to joule"));
        QVERIFY2(!kilowatthour.isError(), qPrintable(kilowatthour.toString()));
        QCOMPARE(kilowatthour.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(kilowatthour.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Energy"));
        QCOMPARE(kilowatthour.property(QStringLiteral("formatted")).toString(), QStringLiteral("3600000 joule"));

        const QJSValue newton = callCalc(QStringLiteral("1 newton to kgf"));
        QVERIFY2(!newton.isError(), qPrintable(newton.toString()));
        QCOMPARE(newton.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(newton.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Force"));
        QCOMPARE(newton.property(QStringLiteral("formatted")).toString(), QStringLiteral("0.101971621298 kgf"));

        const QJSValue watt = callCalc(QStringLiteral("1 watt to hp"));
        QVERIFY2(!watt.isError(), qPrintable(watt.toString()));
        QCOMPARE(watt.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(watt.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Power"));
        QCOMPARE(watt.property(QStringLiteral("formatted")).toString(), QStringLiteral("0.0013410220896 hp"));

        const QJSValue horsepower = callCalc(QStringLiteral("1 horsepower to watt"));
        QVERIFY2(!horsepower.isError(), qPrintable(horsepower.toString()));
        QCOMPARE(horsepower.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(horsepower.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Power"));
        QCOMPARE(horsepower.property(QStringLiteral("formatted")).toString(), QStringLiteral("745.699871582 watt"));

        const QJSValue btu = callCalc(QStringLiteral("1 btu to joule"));
        QVERIFY2(!btu.isError(), qPrintable(btu.toString()));
        QCOMPARE(btu.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(btu.property(QStringLiteral("compoundLabel")).toString(), QStringLiteral("Energy"));
        QCOMPARE(btu.property(QStringLiteral("formatted")).toString(), QStringLiteral("1055.05585262 joule"));

        const QJSValue ratio = callCalc(QStringLiteral("50% to ratio"));
        QVERIFY2(!ratio.isError(), qPrintable(ratio.toString()));
        QCOMPARE(ratio.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(ratio.property(QStringLiteral("formatted")).toString(), QStringLiteral("0.5 ratio"));

        const QJSValue angle = callCalc(QStringLiteral("1 turn to deg"));
        QVERIFY2(!angle.isError(), qPrintable(angle.toString()));
        QCOMPARE(angle.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(angle.property(QStringLiteral("formatted")).toString(), QStringLiteral("360 deg"));

        const QJSValue frequency = callCalc(QStringLiteral("60 rpm to hz"));
        QVERIFY2(!frequency.isError(), qPrintable(frequency.toString()));
        QCOMPARE(frequency.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(frequency.property(QStringLiteral("formatted")).toString(), QStringLiteral("1 hz"));

        const QJSValue dataRate = callCalc(QStringLiteral("1 MiB/s to B/s"));
        QVERIFY2(!dataRate.isError(), qPrintable(dataRate.toString()));
        QCOMPARE(dataRate.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(dataRate.property(QStringLiteral("formatted")).toString(), QStringLiteral("8388608 B/s"));

        const QJSValue mph = callCalc(QStringLiteral("60 mph to km/h"));
        QVERIFY2(!mph.isError(), qPrintable(mph.toString()));
        QCOMPARE(mph.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(mph.property(QStringLiteral("formatted")).toString(), QStringLiteral("96.56064 km/h"));

        const QJSValue kph = callCalc(QStringLiteral("100 kph to m/s"));
        QVERIFY2(!kph.isError(), qPrintable(kph.toString()));
        QCOMPARE(kph.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(kph.property(QStringLiteral("formatted")).toString(), QStringLiteral("27.7777777778 m/s"));

        const QJSValue kmh = callCalc(QStringLiteral("36 kmh to mph"));
        QVERIFY2(!kmh.isError(), qPrintable(kmh.toString()));
        QCOMPARE(kmh.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(kmh.property(QStringLiteral("formatted")).toString(), QStringLiteral("22.3693629205 mph"));

        const QJSValue fps = callCalc(QStringLiteral("100 fps to m/s"));
        QVERIFY2(!fps.isError(), qPrintable(fps.toString()));
        QCOMPARE(fps.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(fps.property(QStringLiteral("formatted")).toString(), QStringLiteral("30.48 m/s"));

        const QJSValue kn = callCalc(QStringLiteral("10 kn to m/s"));
        QVERIFY2(!kn.isError(), qPrintable(kn.toString()));
        QCOMPARE(kn.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(kn.property(QStringLiteral("formatted")).toString(), QStringLiteral("5.14444444444 m/s"));

        const QJSValue knot = callCalc(QStringLiteral("10 knot to km/h"));
        QVERIFY2(!knot.isError(), qPrintable(knot.toString()));
        QCOMPARE(knot.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(knot.property(QStringLiteral("formatted")).toString(), QStringLiteral("18.52 km/h"));

        const QJSValue mps = callCalc(QStringLiteral("10 mps to km/h"));
        QVERIFY2(!mps.isError(), qPrintable(mps.toString()));
        QCOMPARE(mps.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(mps.property(QStringLiteral("formatted")).toString(), QStringLiteral("36 km/h"));

        const QJSValue knotsAlias = callCalc(QStringLiteral("10 knots to m/s"));
        QVERIFY2(!knotsAlias.isError(), qPrintable(knotsAlias.toString()));
        QCOMPARE(knotsAlias.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(knotsAlias.property(QStringLiteral("formatted")).toString(), QStringLiteral("5.14444444444 m/s"));

        const QJSValue ktAlias = callCalc(QStringLiteral("10 kt to m/s"));
        QVERIFY2(!ktAlias.isError(), qPrintable(ktAlias.toString()));
        QCOMPARE(ktAlias.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(ktAlias.property(QStringLiteral("formatted")).toString(), QStringLiteral("5.14444444444 m/s"));

        const QJSValue angularSpeed = callCalc(QStringLiteral("180 deg/s to rad/s"));
        QVERIFY2(!angularSpeed.isError(), qPrintable(angularSpeed.toString()));
        QCOMPARE(angularSpeed.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(angularSpeed.property(QStringLiteral("formatted")).toString(), QStringLiteral("3.14159265359 rad/s"));

        const QJSValue angularSpeedAlias = callCalc(QStringLiteral("1 rev/s to deg/s"));
        QVERIFY2(!angularSpeedAlias.isError(), qPrintable(angularSpeedAlias.toString()));
        QCOMPARE(angularSpeedAlias.property(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(angularSpeedAlias.property(QStringLiteral("formatted")).toString(), QStringLiteral("360 deg/s"));
    }
};

QTEST_GUILESS_MAIN(PulseControllerJsTest)
#include "pulse_controller_js_test.moc"
