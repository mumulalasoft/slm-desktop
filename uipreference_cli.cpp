#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

#include "src/core/prefs/uipreferences.h"

namespace {
void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  uipreference ls\n";
    out << "  uipreference --help\n";
    out << "  uipreference man\n";
    out << "  uipreference <app-name> where\n";
    out << "  uipreference <app-name> get <key> [fallback]\n";
    out << "  uipreference <app-name> set <key> <value>\n";
    out << "  uipreference <app-name> reset <key>\n";
    out << "  uipreference <app-name> list\n";
    out << "  uipreference <app-name> dock-lively\n";
    out << "  uipreference <app-name> dock-subtle\n";
}

void printMan()
{
    QTextStream out(stdout);
    out << "uipreference - centralized UI preference CLI\n\n";
    out << "SYNOPSIS\n";
    out << "  uipreference ls\n";
    out << "  uipreference --help\n";
    out << "  uipreference man\n";
    out << "  uipreference <app-name> where\n";
    out << "  uipreference <app-name> get <key> [fallback]\n";
    out << "  uipreference <app-name> set <key> <value>\n";
    out << "  uipreference <app-name> reset <key>\n";
    out << "  uipreference <app-name> list\n";
    out << "  uipreference <app-name> dock-lively\n";
    out << "  uipreference <app-name> dock-subtle\n\n";
    out << "DESCRIPTION\n";
    out << "  Read/write/reset UI preferences stored under organization 'SLM'.\n";
    out << "  Dot keys and slash keys are both accepted (e.g. dock.motionPreset or dock/motionPreset).\n\n";
    out << "COMMANDS\n";
    out << "  ls\n";
    out << "      List detected application names from ~/.config/SLM/*.ini or *.conf.\n";
    out << "  list\n";
    out << "      Print preference snapshot for <app-name> as JSON.\n";
    out << "  where\n";
    out << "      Print backing settings file path for <app-name>.\n";
    out << "  get\n";
    out << "      Print a key value for <app-name>.\n";
    out << "  set\n";
    out << "      Set a key for <app-name>. Value parser supports bool, integer, float, null, string.\n";
    out << "      For windowing.bind* keys, use Disabled/None/Off to disable a shortcut.\n";
    out << "      For dock.motionPreset, aliases are accepted: subtle | lively (or expressive).\n";
    out << "  reset\n";
    out << "      Reset a key to its default (for known central keys) or remove custom key.\n\n";
    out << "  dock-lively\n";
    out << "      Shortcut command for: set dock.motionPreset lively\n";
    out << "  dock-subtle\n";
    out << "      Shortcut command for: set dock.motionPreset subtle\n\n";
    out << "KNOWN KEYS\n";
    out << "  dock.motionPreset\n";
    out << "  dock.dropPulseEnabled\n";
    out << "  dock.autoHideEnabled\n";
    out << "  dock.hideMode\n";
    out << "  dock.hideDurationMs\n";
    out << "  dock.dragThresholdMousePx\n";
    out << "  dock.dragThresholdTouchpadPx\n";
    out << "  debug.verboseLogging\n";
    out << "  iconTheme.light\n";
    out << "  iconTheme.dark\n\n";
    out << "  windowing.bindMinimize\n";
    out << "  windowing.bindRestore\n";
    out << "  windowing.bindSwitchNext\n";
    out << "  windowing.bindSwitchPrev\n\n";
    out << "  windowing.bindWorkspace\n";
    out << "  windowing.bindOverview (legacy alias)\n\n";
    out << "  windowing.bindClose\n";
    out << "  windowing.bindMaximize\n";
    out << "  windowing.bindTileLeft\n";
    out << "  windowing.bindTileRight\n";
    out << "  windowing.bindUntile\n\n";
    out << "  windowing.bindQuarterTopLeft\n";
    out << "  windowing.bindQuarterTopRight\n";
    out << "  windowing.bindQuarterBottomLeft\n";
    out << "  windowing.bindQuarterBottomRight\n\n";
    out << "  windowing.shadowEnabled\n";
    out << "  windowing.shadowStrength\n";
    out << "  windowing.roundedEnabled\n";
    out << "  windowing.roundedRadius\n";
    out << "  windowing.animationEnabled\n";
    out << "  windowing.animationSpeed\n\n";
    out << "  windowing.sceneFxEnabled\n";
    out << "  windowing.sceneFxRoundedClipEnabled\n";
    out << "  windowing.sceneFxDimAlpha\n";
    out << "  windowing.sceneFxAnimBoost\n\n";
    out << "  notifications.doNotDisturb\n";
    out << "  notifications.bubbleDurationMs\n\n";
    out << "EXAMPLES\n";
    out << "  uipreference ls\n";
    out << "  uipreference SLM get dock.motionPreset\n";
    out << "  uipreference SLM set dock.motionPreset lively\n";
    out << "  uipreference SLM dock-lively\n";
    out << "  uipreference SLM dock-subtle\n";
    out << "  uipreference SLM set dock.autoHideEnabled true\n";
    out << "  uipreference SLM set dock.hideMode smart_hide\n";
    out << "  uipreference SLM set dock.hideDurationMs 900\n";
    out << "  uipreference SLM reset debug.verboseLogging\n";
    out << "  uipreference compositor set windowing.bindSwitchNext Alt+F8\n";
    out << "  uipreference compositor set windowing.bindWorkspace Alt+F3\n";
}

QStringList listAppNames()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QDir orgDir(QDir(configRoot).filePath(QStringLiteral("SLM")));
    if (!orgDir.exists()) {
        return {};
    }

    const QStringList confFiles = orgDir.entryList(QStringList() << QStringLiteral("*.conf")
                                                                  << QStringLiteral("*.ini"),
                                                   QDir::Files, QDir::Name);
    QSet<QString> names;
    for (const QString &fileName : confFiles) {
        const QString base = QFileInfo(fileName).completeBaseName().trimmed();
        if (!base.isEmpty()) {
            names.insert(base);
        }
    }
    QStringList out = names.values();
    out.sort(Qt::CaseInsensitive);
    return out;
}

QVariant parseValue(const QString &raw)
{
    const QString v = raw.trimmed();
    if (v.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (v.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    bool okInt = false;
    const qlonglong asInt = v.toLongLong(&okInt);
    if (okInt) {
        return asInt;
    }
    bool okDouble = false;
    const double asDouble = v.toDouble(&okDouble);
    if (okDouble) {
        return asDouble;
    }
    if (v.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0) {
        return QVariant();
    }
    return v;
}

QVariant normalizeCliValueForKey(const QString &key, const QVariant &value)
{
    const QString keyLower = key.trimmed().toLower();
    if (keyLower != QStringLiteral("dock.motionpreset") &&
        keyLower != QStringLiteral("dock/motionpreset")) {
        return value;
    }
    const QString raw = value.toString().trimmed().toLower();
    if (raw == QStringLiteral("lively") ||
        raw == QStringLiteral("macos-lively") ||
        raw == QStringLiteral("expressive")) {
        return QStringLiteral("macos-lively");
    }
    if (raw == QStringLiteral("subtle")) {
        return QStringLiteral("subtle");
    }
    return value;
}

QString windowFxCommandFor(const QString &key, const QVariant &value)
{
    const QString k = key.trimmed().toLower();
    if (k == QStringLiteral("windowing.shadowenabled")) {
        return QStringLiteral("windowfx shadowEnabled %1").arg(value.toBool() ? 1 : 0);
    }
    if (k == QStringLiteral("windowing.shadowstrength")) {
        return QStringLiteral("windowfx shadowStrength %1").arg(value.toInt());
    }
    if (k == QStringLiteral("windowing.roundedenabled")) {
        return QStringLiteral("windowfx roundedEnabled %1").arg(value.toBool() ? 1 : 0);
    }
    if (k == QStringLiteral("windowing.roundedradius")) {
        return QStringLiteral("windowfx roundedRadius %1").arg(value.toInt());
    }
    if (k == QStringLiteral("windowing.animationenabled")) {
        return QStringLiteral("windowfx animationEnabled %1").arg(value.toBool() ? 1 : 0);
    }
    if (k == QStringLiteral("windowing.animationspeed")) {
        return QStringLiteral("windowfx animationSpeed %1").arg(value.toInt());
    }
    if (k == QStringLiteral("windowing.scenefxenabled")) {
        return QStringLiteral("windowfx sceneFxEnabled %1").arg(value.toBool() ? 1 : 0);
    }
    if (k == QStringLiteral("windowing.scenefxdimalpha")) {
        return QStringLiteral("windowfx sceneFxDimAlpha %1")
            .arg(QString::number(value.toDouble(), 'f', 2));
    }
    if (k == QStringLiteral("windowing.scenefxanimboost")) {
        return QStringLiteral("windowfx sceneFxAnimBoost %1").arg(value.toInt());
    }
    if (k == QStringLiteral("windowing.scenefxroundedclipenabled")) {
        return QStringLiteral("windowfx sceneFxRoundedClipEnabled %1").arg(value.toBool() ? 1 : 0);
    }
    return QString();
}

QString windowBindCommandFor(const QString &key, const QVariant &value)
{
    const QString k = key.trimmed().toLower();
    QString action;
    if (k == QStringLiteral("windowing.bindminimize")) action = QStringLiteral("minimize");
    else if (k == QStringLiteral("windowing.bindrestore")) action = QStringLiteral("restore");
    else if (k == QStringLiteral("windowing.bindswitchnext")) action = QStringLiteral("switchNext");
    else if (k == QStringLiteral("windowing.bindswitchprev")) action = QStringLiteral("switchPrev");
    else if (k == QStringLiteral("windowing.bindworkspace")
             || k == QStringLiteral("windowing.bindoverview")) action = QStringLiteral("workspace");
    else if (k == QStringLiteral("windowing.bindclose")) action = QStringLiteral("close");
    else if (k == QStringLiteral("windowing.bindmaximize")) action = QStringLiteral("maximize");
    else if (k == QStringLiteral("windowing.bindtileleft")) action = QStringLiteral("tileLeft");
    else if (k == QStringLiteral("windowing.bindtileright")) action = QStringLiteral("tileRight");
    else if (k == QStringLiteral("windowing.binduntile")) action = QStringLiteral("untile");
    else if (k == QStringLiteral("windowing.bindquartertopleft")) action = QStringLiteral("quarterTopLeft");
    else if (k == QStringLiteral("windowing.bindquartertopright")) action = QStringLiteral("quarterTopRight");
    else if (k == QStringLiteral("windowing.bindquarterbottomleft")) action = QStringLiteral("quarterBottomLeft");
    else if (k == QStringLiteral("windowing.bindquarterbottomright")) action = QStringLiteral("quarterBottomRight");
    if (action.isEmpty()) {
        return QString();
    }
    return QStringLiteral("windowbind %1 %2").arg(action, value.toString());
}

void tryApplyCompositorLive(const QString &key, const QVariant &value)
{
    Q_UNUSED(key);
    Q_UNUSED(value);
    // kwin-only mode: no direct Unix-socket compositor live apply path.
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString firstArg = args.at(1).trimmed();
    const QString firstArgLower = firstArg.toLower();
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (firstArgLower == QStringLiteral("--help") ||
        firstArgLower == QStringLiteral("-h") ||
        firstArgLower == QStringLiteral("help")) {
        printUsage();
        return 0;
    }
    if (firstArgLower == QStringLiteral("man")) {
        printMan();
        return 0;
    }
    if (firstArgLower == QStringLiteral("ls")) {
        const QStringList names = listAppNames();
        for (const QString &name : names) {
            out << name << '\n';
        }
        return 0;
    }

    if (args.size() < 3) {
        printUsage();
        return 1;
    }

    const QString appName = args.at(1).trimmed();
    const QString command = args.at(2).trimmed().toLower();
    if (appName.isEmpty()) {
        printUsage();
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(appName);

    UIPreferences prefs;

    if (command == QStringLiteral("list")) {
        const QVariantMap map = prefs.preferenceSnapshot();
        QJsonObject obj = QJsonObject::fromVariantMap(map);
        out << QJsonDocument(obj).toJson(QJsonDocument::Indented);
        return 0;
    }

    if (command == QStringLiteral("where")) {
        out << prefs.settingsFilePath() << '\n';
        return 0;
    }

    if (command == QStringLiteral("dock-lively")) {
        const QString before = prefs.dockMotionPreset();
        prefs.setDockMotionPreset(QStringLiteral("macos-lively"));
        out << ((before == prefs.dockMotionPreset()) ? "nochange" : "ok") << '\n';
        return 0;
    }

    if (command == QStringLiteral("dock-subtle")) {
        const QString before = prefs.dockMotionPreset();
        prefs.setDockMotionPreset(QStringLiteral("subtle"));
        out << ((before == prefs.dockMotionPreset()) ? "nochange" : "ok") << '\n';
        return 0;
    }

    if (command == QStringLiteral("get")) {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        const QString key = args.at(3);
        const QVariant fallback = args.size() >= 5 ? parseValue(args.at(4)) : QVariant();
        const QVariant value = prefs.getPreference(key, fallback);
        out << value.toString() << '\n';
        return 0;
    }

    if (command == QStringLiteral("set")) {
        if (args.size() < 5) {
            printUsage();
            return 1;
        }
        const QString key = args.at(3);
        const QVariant value = normalizeCliValueForKey(key, parseValue(args.at(4)));
        const bool changed = prefs.setPreference(key, value);
        if (changed) {
            tryApplyCompositorLive(key, value);
        }
        out << (changed ? "ok" : "nochange") << '\n';
        return 0;
    }

    if (command == QStringLiteral("reset")) {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        const QString key = args.at(3);
        const bool changed = prefs.resetPreference(key);
        if (changed) {
            tryApplyCompositorLive(key, prefs.getPreference(key, QVariant()));
        }
        out << (changed ? "ok" : "nochange") << '\n';
        return 0;
    }

    err << "Unknown command: " << command << '\n';
    printUsage();
    return 1;
}
