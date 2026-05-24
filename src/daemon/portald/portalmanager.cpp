#include "portalmanager.h"
#include "portalmethodnames.h"
#include "portalresponsebuilder.h"
#include "portalvalidation.h"
#include "portalsettingstypes.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
constexpr const char kUiIface[] = "org.slm.Desktop.PortalUI";
constexpr int kUiTimeoutMs = 190000;

constexpr const char kSettingsService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsIface[] = "org.slm.Desktop.Settings";

QSet<QString> defaultAllowedOpenUriSchemes()
{
    return {
        QStringLiteral("file"),
        QStringLiteral("http"),
        QStringLiteral("https"),
        QStringLiteral("mailto"),
        QStringLiteral("ftp"),
        QStringLiteral("smb"),
        QStringLiteral("sftp"),
    };
}

QSet<QString> parseSchemes(const QStringList &items)
{
    QSet<QString> out;
    for (const QString &rawItem : items) {
        const QString s = rawItem.trimmed().toLower();
        if (!s.isEmpty()) {
            out.insert(s);
        }
    }
    return out;
}

QSet<QString> parseSchemes(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QString) {
        const QString raw = value.toString();
        QStringList parts = raw.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                      Qt::SkipEmptyParts);
        return parseSchemes(parts);
    }
    if (value.metaType().id() == QMetaType::QStringList) {
        return parseSchemes(value.toStringList());
    }
    if (value.canConvert<QVariantList>()) {
        QStringList items;
        const QVariantList list = value.toList();
        items.reserve(list.size());
        for (const QVariant &v : list) {
            items.push_back(v.toString());
        }
        return parseSchemes(items);
    }
    return {};
}

QSet<QString> configAllowedOpenUriSchemes()
{
    QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.trimmed().isEmpty()) {
        configHome = QDir::homePath() + QStringLiteral("/.config");
    }
    const QString configPath = QDir(configHome).filePath(QStringLiteral("slm/portald.conf"));
    if (!QFileInfo::exists(configPath)) {
        return {};
    }

    QSettings settings(configPath, QSettings::IniFormat);
    const QVariant direct = settings.value(QStringLiteral("openuri_allowed_schemes"));
    QSet<QString> parsed = parseSchemes(direct);
    if (!parsed.isEmpty()) {
        return parsed;
    }

    const QVariant sectionLower = settings.value(QStringLiteral("Portal/openuri_allowed_schemes"));
    parsed = parseSchemes(sectionLower);
    if (!parsed.isEmpty()) {
        return parsed;
    }

    const QVariant sectionCamel = settings.value(QStringLiteral("Portal/OpenUriAllowedSchemes"));
    return parseSchemes(sectionCamel);
}

QSet<QString> allowedOpenUriSchemes(const QVariantMap &options)
{
    if (options.contains(QStringLiteral("allowedSchemes"))) {
        const QSet<QString> parsed = parseSchemes(options.value(QStringLiteral("allowedSchemes")));
        if (!parsed.isEmpty()) {
            return parsed;
        }
    }

    const QByteArray envValue = qgetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");
    if (!envValue.isEmpty()) {
        const QString raw = QString::fromUtf8(envValue);
        QStringList parts = raw.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                      Qt::SkipEmptyParts);
        const QSet<QString> parsed = parseSchemes(parts);
        if (!parsed.isEmpty()) {
            return parsed;
        }
    }

    const QSet<QString> fromConfig = configAllowedOpenUriSchemes();
    if (!fromConfig.isEmpty()) {
        return fromConfig;
    }

    return defaultAllowedOpenUriSchemes();
}

bool isAllowedOpenUriScheme(const QString &scheme, const QVariantMap &options)
{
    return allowedOpenUriSchemes(options).contains(scheme.toLower());
}

QVariantMap callUiFileChooser(const QVariantMap &options)
{
    QDBusInterface iface(QString::fromLatin1(kUiService),
                         QString::fromLatin1(kUiPath),
                         QString::fromLatin1(kUiIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::uiServiceUnavailable(
            QString::fromLatin1(SlmPortalMethod::kFileChooser));
    }
    iface.setTimeout(kUiTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(
        QString::fromLatin1(SlmPortalMethod::kFileChooser), options);
    if (!reply.isValid()) {
        return SlmPortalResponseBuilder::uiCallFailed(
            QString::fromLatin1(SlmPortalMethod::kFileChooser));
    }
    return reply.value();
}

QVariantList toVariantListNormalized(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }
    if (value.canConvert<QVariantList>()) {
        const QVariantList list = value.toList();
        if (!list.isEmpty()) {
            return list;
        }
    }
    if (value.canConvert<QStringList>()) {
        const QStringList list = value.toStringList();
        if (!list.isEmpty()) {
            QVariantList out;
            out.reserve(list.size());
            for (const QString &item : list) {
                out.push_back(item);
            }
            return out;
        }
    }
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument dbusArg = value.value<QDBusArgument>();
        QVariantList out;
        QDBusArgument arg = dbusArg;
        arg.beginArray();
        while (!arg.atEnd()) {
            QVariant item;
            arg >> item;
            out.push_back(item);
        }
        arg.endArray();
        if (!out.isEmpty()) {
            return out;
        }
    }
    const QString single = value.toString().trimmed();
    if (!single.isEmpty()) {
        return {single};
    }
    return {};
}

void normalizeFileChooserPayload(QVariantMap *out)
{
    if (!out) {
        return;
    }
    QVariantList paths = toVariantListNormalized(out->value(QStringLiteral("paths")));
    QVariantList uris = toVariantListNormalized(out->value(QStringLiteral("uris")));
    const QString path = out->value(QStringLiteral("path")).toString().trimmed();
    const QString uri = out->value(QStringLiteral("uri")).toString().trimmed();

    if (paths.isEmpty() && !path.isEmpty()) {
        paths.push_back(path);
    }
    if (uris.isEmpty() && !uri.isEmpty()) {
        uris.push_back(uri);
    }

    out->insert(QStringLiteral("paths"), paths);
    out->insert(QStringLiteral("uris"), uris);
}
}

PortalManager::PortalManager(QObject *parent)
    : QObject(parent)
{
}

QVariantMap PortalManager::Screenshot(const QVariantMap &options) const
{
    const QString fileName = QStringLiteral("slm-screenshot-%1.png")
        .arg(QDateTime::currentMSecsSinceEpoch());
    const QString path = QDir(QDir::tempPath()).filePath(fileName);

    // grim is the Wayland native screen capture tool
    int exitCode = QProcess::execute(QStringLiteral("grim"), {path});
    if (exitCode == 0 && QFileInfo::exists(path)) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kScreenshot)},
            {QStringLiteral("uri"), QUrl::fromLocalFile(path).toString()},
        };
    }

    return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kScreenshot),
                              {{QStringLiteral("options"), options},
                               {QStringLiteral("reason"), QStringLiteral("capture-backend-unavailable")}});
}

QVariantMap PortalManager::ScreenCast(const QVariantMap &options) const
{
    QString sessionHandle = options.value(QStringLiteral("session_handle")).toString().trimmed();
    if (sessionHandle.isEmpty()) {
        sessionHandle = options.value(QStringLiteral("sessionHandle")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = options.value(QStringLiteral("sessionPath")).toString().trimmed();
    }

    if (!sessionHandle.isEmpty()) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kScreenCast)},
            {QStringLiteral("session_handle"), sessionHandle},
            {QStringLiteral("results"), QVariantMap{{QStringLiteral("session_handle"), sessionHandle}}}
        };
    }

    return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kScreenCast),
                              {{QStringLiteral("options"), options}});
}

QVariantMap PortalManager::FileChooser(const QVariantMap &options) const
{
    const QString mode = options.value(QStringLiteral("mode")).toString().trimmed();
    if (!SlmPortalValidation::isValidFileChooserMode(mode)) {
        return SlmPortalResponseBuilder::invalidArgument(
                                                         QString::fromLatin1(SlmPortalMethod::kFileChooser),
                                                         {{QStringLiteral("options"), options}});
    }

    QVariantMap out = callUiFileChooser(options);
    if (!out.contains(QStringLiteral("ok"))) {
        out.insert(QStringLiteral("ok"), false);
    }
    normalizeFileChooserPayload(&out);
    out.insert(QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kFileChooser));
    return out;
}

QVariantMap PortalManager::OpenURI(const QString &uri, const QVariantMap &options) const
{
    const QString raw = uri.trimmed();
    if (raw.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
                                                         QString::fromLatin1(SlmPortalMethod::kOpenURI),
                                                         {{QStringLiteral("uri"), uri},
                                                          {QStringLiteral("options"), options}});
    }

    const QUrl targetUrl = QUrl::fromUserInput(raw);
    if (!targetUrl.isValid() || targetUrl.scheme().trimmed().isEmpty()
        || !isAllowedOpenUriScheme(targetUrl.scheme(), options)) {
        return SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kOpenURI),
            {{QStringLiteral("uri"), uri},
             {QStringLiteral("options"), options}});
    }

    const QString normalizedUri = targetUrl.toString(QUrl::FullyEncoded);
    const bool dryRun = options.value(QStringLiteral("dryRun")).toBool();
    if (dryRun) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kOpenURI)},
            {QStringLiteral("uri"), normalizedUri},
            {QStringLiteral("options"), options},
            {QStringLiteral("dryRun"), true},
            {QStringLiteral("launched"), false},
        };
    }

    bool started = QProcess::startDetached(QStringLiteral("gio"),
                                           {QStringLiteral("open"), normalizedUri});
    if (started) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kOpenURI)},
            {QStringLiteral("uri"), normalizedUri},
            {QStringLiteral("options"), options},
            {QStringLiteral("launched"), true},
            {QStringLiteral("backend"), QStringLiteral("gio")},
        };
    }

    return SlmPortalResponseBuilder::withError(
        QString::fromLatin1(SlmPortalMethod::kOpenURI),
        QString::fromLatin1(SlmPortalError::kLaunchFailed),
        {{QStringLiteral("uri"), normalizedUri},
         {QStringLiteral("options"), options}});
}

QVariantMap PortalManager::PickColor(const QVariantMap &options) const
{
    // hyprpicker outputs a hex color like "#RRGGBB" on stdout
    QProcess proc;
    proc.start(QStringLiteral("hyprpicker"), {QStringLiteral("-f"), QStringLiteral("hex")});
    if (!proc.waitForStarted(3000)) {
        return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kPickColor),
                                  {{QStringLiteral("options"), options},
                                   {QStringLiteral("reason"), QStringLiteral("color-picker-unavailable")}});
    }
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kPickColor),
                                  {{QStringLiteral("options"), options},
                                   {QStringLiteral("reason"), QStringLiteral("color-picker-timeout")}});
    }
    if (proc.exitCode() != 0) {
        return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kPickColor),
                                  {{QStringLiteral("options"), options},
                                   {QStringLiteral("reason"), QStringLiteral("color-picker-cancelled")}});
    }

    QString hex = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (hex.startsWith(QLatin1Char('#')))
        hex = hex.mid(1);

    bool ok = false;
    const uint rgb = hex.toUInt(&ok, 16);
    if (!ok || hex.length() != 6) {
        return makeNotImplemented(QString::fromLatin1(SlmPortalMethod::kPickColor),
                                  {{QStringLiteral("options"), options},
                                   {QStringLiteral("reason"), QStringLiteral("color-picker-bad-output")}});
    }

    const double r = ((rgb >> 16) & 0xFFu) / 255.0;
    const double g = ((rgb >>  8) & 0xFFu) / 255.0;
    const double b =  (rgb        & 0xFFu) / 255.0;

    PortalColorValue color{r, g, b};
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kPickColor)},
        {QStringLiteral("color"), QVariant::fromValue(color)},
    };
}

QVariantMap PortalManager::PickFolder(const QVariantMap &options) const
{
    if (SlmPortalValidation::hasPickFolderConflicts(options)) {
        return SlmPortalResponseBuilder::invalidArgument(
                                                         QString::fromLatin1(SlmPortalMethod::kPickFolder),
                                                         {{QStringLiteral("options"), options}});
    }

    QVariantMap chooser = options;
    chooser.insert(QStringLiteral("mode"), QStringLiteral("folder"));
    chooser.insert(QStringLiteral("selectFolders"), true);
    chooser.insert(QStringLiteral("multiple"), false);
    QVariantMap out = callUiFileChooser(chooser);
    if (!out.contains(QStringLiteral("ok"))) {
        out.insert(QStringLiteral("ok"), false);
    }
    normalizeFileChooserPayload(&out);
    out.insert(QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kPickFolder));
    return out;
}

QVariantMap PortalManager::Wallpaper(const QVariantMap &options) const
{
    const QString raw = options.value(QStringLiteral("uri")).toString().trimmed();
    if (raw.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kWallpaper),
            {{QStringLiteral("reason"), QStringLiteral("empty-uri")}});
    }

    const QUrl url = QUrl::fromUserInput(raw);
    if (!url.isValid()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kWallpaper),
            {{QStringLiteral("uri"), raw},
             {QStringLiteral("reason"), QStringLiteral("invalid-uri")}});
    }
    const QString finalUri = url.toString(QUrl::FullyEncoded);

    QDBusInterface iface(QString::fromLatin1(kSettingsService),
                         QString::fromLatin1(kSettingsPath),
                         QString::fromLatin1(kSettingsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QString::fromLatin1(SlmPortalMethod::kWallpaper));
    }

    QDBusReply<QVariantMap> reply = iface.call(
        QStringLiteral("SetSetting"),
        QStringLiteral("wallpaper.uri"),
        QVariant::fromValue(QDBusVariant(finalUri)));
    if (!reply.isValid()) {
        return SlmPortalResponseBuilder::withError(
            QString::fromLatin1(SlmPortalMethod::kWallpaper),
            QStringLiteral("settings-call-failed"),
            {{QStringLiteral("uri"), finalUri}});
    }

    const QVariantMap result = reply.value();
    const bool ok = !result.contains(QStringLiteral("error"))
                    && result.value(QStringLiteral("ok"), true).toBool();
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("method"), QString::fromLatin1(SlmPortalMethod::kWallpaper)},
        {QStringLiteral("uri"), finalUri},
    };
}

QVariantMap PortalManager::makeNotImplemented(const QString &method, const QVariantMap &extra)
{
    return SlmPortalResponseBuilder::notImplemented(method, extra);
}
