#include "implportalservice.h"

#include "implportaladaptors.h"
#include "portalmanager.h"
#include "../../../portalresponsebuilder.h"
#include "portal-adapter/PortalAppContextResolver.h"
#include "portal-adapter/PortalBackendService.h"
#include "../../core/permissions/Capability.h"
#include "../../core/permissions/CallerIdentity.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <QtGlobal>
#include <QUuid>
#include <QUrl>
#include <algorithm>

#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace {
constexpr const char kService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kApiVersion[] = "0.1";
constexpr const char kNotificationsService[] = "org.freedesktop.Notifications";
constexpr const char kNotificationsPath[] = "/org/freedesktop/Notifications";
constexpr const char kNotificationsIface[] = "org.freedesktop.Notifications";
constexpr const char kSessionStateService[] = "org.slm.SessionState";
constexpr const char kSessionStatePath[] = "/org/slm/SessionState";
constexpr const char kSessionStateIface[] = "org.slm.SessionState1";
constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
constexpr const char kFileOpsPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kFileOpsIface[] = "org.slm.Desktop.FileOperations";
constexpr const char kCaptureService[] = "org.slm.Desktop.Capture";
constexpr const char kCapturePath[] = "/org/slm/Desktop/Capture";
constexpr const char kCaptureIface[] = "org.slm.Desktop.Capture";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
constexpr const char kInputCaptureService[] = "org.slm.Desktop.InputCapture";
constexpr const char kInputCapturePath[] = "/org/slm/Desktop/InputCapture";
constexpr const char kInputCaptureIface[] = "org.slm.Desktop.InputCapture";
constexpr const char kPortalUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kPortalUiPath[] = "/org/slm/Desktop/PortalUI";
constexpr const char kPortalUiIface[] = "org.slm.Desktop.PortalUI";
constexpr int kPortalUiTimeoutMs = 190000;
constexpr int kNotificationRateWindowMs = 30000;
constexpr int kNotificationRateMaxPerWindow = 20;

static QVariant unmarshalVariant(const QVariant &v)
{
    if (v.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        if (arg.currentType() == QDBusArgument::MapType) {
            QVariantMap map;
            arg >> map;
            for (auto it = map.begin(); it != map.end(); ++it) {
                *it = unmarshalVariant(*it);
            }
            return map;
        } else if (arg.currentType() == QDBusArgument::ArrayType) {
            QVariantList list;
            arg >> list;
            for (int i = 0; i < list.size(); ++i) {
                list[i] = unmarshalVariant(list[i]);
            }
            return list;
        }
    } else if (v.userType() == QMetaType::QVariantMap) {
        QVariantMap map = v.toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            *it = unmarshalVariant(*it);
        }
        return map;
    } else if (v.userType() == QMetaType::QVariantList) {
        QVariantList list = v.toList();
        for (int i = 0; i < list.size(); ++i) {
            list[i] = unmarshalVariant(list[i]);
        }
        return list;
    }
    return v;
}

static QVariantMap unmarshalVariantMap(const QVariant &v)
{
    const QVariant unmarshalled = unmarshalVariant(v);
    if (unmarshalled.userType() == QMetaType::QVariantMap) {
        return unmarshalled.toMap();
    }
    return v.toMap();
}

QVariantMap normalizePortalLikeResult(const QVariantMap &raw)
{
    QVariantMap out = raw;
    if (!out.contains(QStringLiteral("response"))) {
        const bool ok = out.value(QStringLiteral("ok")).toBool();
        uint response = ok ? 0u : 3u;
        const QString err = out.value(QStringLiteral("error")).toString().trimmed().toLower();
        if (!ok && (err == QStringLiteral("permissiondenied") || err == QStringLiteral("denied")
                    || err == QStringLiteral("permission-denied"))) {
            response = 1u;
        } else if (!ok && (err == QStringLiteral("cancelledbyuser") || err == QStringLiteral("canceled")
                           || err == QStringLiteral("cancelled") || err == QStringLiteral("timeout"))) {
            response = 2u;
        }
        out.insert(QStringLiteral("response"), response);
    }
    if (!out.contains(QStringLiteral("results"))) {
        out.insert(QStringLiteral("results"), raw);
    }
    QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
    QString requestHandle = results.value(QStringLiteral("request_handle")).toString().trimmed();
    if (requestHandle.isEmpty()) {
        requestHandle = results.value(QStringLiteral("requestHandle")).toString().trimmed();
    }
    if (requestHandle.isEmpty()) {
        requestHandle = results.value(QStringLiteral("requestPath")).toString().trimmed();
    }
    if (requestHandle.isEmpty()) {
        requestHandle = out.value(QStringLiteral("requestHandle")).toString().trimmed();
    }
    if (requestHandle.isEmpty()) {
        requestHandle = out.value(QStringLiteral("requestPath")).toString().trimmed();
    }
    if (requestHandle.isEmpty()) {
        requestHandle = out.value(QStringLiteral("handle")).toString().trimmed();
    }
    if (!results.contains(QStringLiteral("request_handle"))) {
        results.insert(QStringLiteral("request_handle"), requestHandle);
    }
    QString sessionHandle = results.value(QStringLiteral("session_handle")).toString().trimmed();
    if (sessionHandle.isEmpty()) {
        sessionHandle = results.value(QStringLiteral("sessionHandle")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = results.value(QStringLiteral("sessionPath")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = out.value(QStringLiteral("session_handle")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = out.value(QStringLiteral("sessionHandle")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = out.value(QStringLiteral("sessionPath")).toString().trimmed();
    }
    if (!sessionHandle.isEmpty() && !results.contains(QStringLiteral("session_handle"))) {
        results.insert(QStringLiteral("session_handle"), sessionHandle);
    }
    out.insert(QStringLiteral("results"), results);
    if (!out.contains(QStringLiteral("request_handle"))) {
        out.insert(QStringLiteral("request_handle"), requestHandle);
    }
    if (!sessionHandle.isEmpty() && !out.contains(QStringLiteral("session_handle"))) {
        out.insert(QStringLiteral("session_handle"), sessionHandle);
    }
    return out;
}

bool isConsentAllowDecision(const QString &decision)
{
    const QString d = decision.trimmed().toLower();
    return d == QStringLiteral("allow_once") || d == QStringLiteral("allow_always");
}

QVariantMap requestTrustedInputCapturePreemptConsent(const QVariantMap &ctx,
                                                     const QString &sessionPath,
                                                     const QVariantMap &hostResults)
{
    QDBusInterface ui(QString::fromLatin1(kPortalUiService),
                      QString::fromLatin1(kPortalUiPath),
                      QString::fromLatin1(kPortalUiIface),
                      QDBusConnection::sessionBus());
    if (!ui.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("reason"), QStringLiteral("consent-ui-unavailable")}};
    }
    ui.setTimeout(kPortalUiTimeoutMs);

    const QString requestPath = QStringLiteral("/org/slm/portal/request/inputcapture/preempt/%1")
                                    .arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    const QString mediationAction =
        hostResults.value(QStringLiteral("mediation_action")).toString().trimmed().isEmpty()
            ? QStringLiteral("inputcapture-preempt-consent")
            : hostResults.value(QStringLiteral("mediation_action")).toString();
    const QString mediationScope =
        hostResults.value(QStringLiteral("mediation_scope")).toString().trimmed().isEmpty()
            ? QStringLiteral("session-conflict")
            : hostResults.value(QStringLiteral("mediation_scope")).toString();
    const QString conflictSession = hostResults.value(QStringLiteral("conflict_session")).toString();

    const QVariantMap payload{
        {QStringLiteral("requestPath"), requestPath},
        {QStringLiteral("appId"), ctx.value(QStringLiteral("appId")).toString()},
        {QStringLiteral("desktopFileId"), ctx.value(QStringLiteral("desktopFileId")).toString()},
        {QStringLiteral("busName"), ctx.value(QStringLiteral("callerBusName")).toString()},
        {QStringLiteral("capability"), QStringLiteral("InputCapture.Enable")},
        {QStringLiteral("resourceType"), QStringLiteral("inputcapture-session")},
        {QStringLiteral("resourceId"), sessionPath},
        {QStringLiteral("sensitivityLevel"), QStringLiteral("high")},
        {QStringLiteral("initiatedByUserGesture"),
         ctx.value(QStringLiteral("initiatedByUserGesture")).toBool()},
        {QStringLiteral("mediation"),
         QVariantMap{
             {QStringLiteral("required"), true},
             {QStringLiteral("action"), mediationAction},
             {QStringLiteral("scope"), mediationScope},
             {QStringLiteral("conflict_session"), conflictSession},
             {QStringLiteral("reason"), QStringLiteral("conflict-active-session")},
         }},
    };

    QDBusReply<QVariantMap> reply = ui.call(QStringLiteral("ConsentRequest"), payload);
    if (!reply.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("reason"), QStringLiteral("consent-ui-call-failed")}};
    }

    QVariantMap out = unmarshalVariantMap(reply.value());
    const QString decision = out.value(QStringLiteral("decision")).toString().trimmed().toLower();
    out.insert(QStringLiteral("ok"), isConsentAllowDecision(decision));
    if (!out.contains(QStringLiteral("reason"))) {
        out.insert(QStringLiteral("reason"),
                   out.value(QStringLiteral("ok")).toBool()
                       ? QStringLiteral("consent-approved")
                       : QStringLiteral("consent-denied"));
    }
    return out;
}

QVariantMap normalizeFileChooserResult(const QVariantMap &raw)
{
    QVariantMap out = normalizePortalLikeResult(raw);
    QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
    QVariantList uris = results.value(QStringLiteral("uris")).toList();
    if (uris.isEmpty()) {
        const QVariantList paths = results.value(QStringLiteral("paths")).toList();
        for (const QVariant &p : paths) {
            const QString path = p.toString();
            if (!path.trimmed().isEmpty()) {
                uris.push_back(QUrl::fromLocalFile(path).toString());
            }
        }
    }
    if (uris.isEmpty()) {
        const QString oneUri = results.value(QStringLiteral("uri")).toString().trimmed();
        const QString onePath = results.value(QStringLiteral("path")).toString().trimmed();
        if (!oneUri.isEmpty()) {
            uris.push_back(oneUri);
        } else if (!onePath.isEmpty()) {
            uris.push_back(QUrl::fromLocalFile(onePath).toString());
        }
    }
    if (!uris.isEmpty()) {
        results.insert(QStringLiteral("uris"), uris);
    } else if (!results.contains(QStringLiteral("uris"))) {
        results.insert(QStringLiteral("uris"), QVariantList{});
    }
    if (!results.contains(QStringLiteral("choices"))) {
        results.insert(QStringLiteral("choices"), QVariantMap{});
    }
    if (!results.contains(QStringLiteral("current_filter"))) {
        results.insert(QStringLiteral("current_filter"), QVariantMap{});
    }
    out.insert(QStringLiteral("results"), results);
    return out;
}

QVariantMap normalizeScreenshotResult(const QVariantMap &raw)
{
    QVariantMap out = normalizePortalLikeResult(raw);
    QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
    const QString uri = results.value(QStringLiteral("uri")).toString().trimmed();
    const QString path = results.value(QStringLiteral("path")).toString().trimmed();
    if (uri.isEmpty() && !path.isEmpty()) {
        results.insert(QStringLiteral("uri"), QUrl::fromLocalFile(path).toString());
    } else if (uri.isEmpty() && !results.contains(QStringLiteral("uri"))) {
        results.insert(QStringLiteral("uri"), QString());
    }
    out.insert(QStringLiteral("results"), results);
    return out;
}

QVariantMap normalizeScreencastResult(const QVariantMap &raw, const QString &sessionPathHint = QString())
{
    QVariantMap out = normalizePortalLikeResult(raw);
    QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
    QString sessionHandle = results.value(QStringLiteral("session_handle")).toString().trimmed();
    if (sessionHandle.isEmpty()) {
        sessionHandle = results.value(QStringLiteral("sessionHandle")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = results.value(QStringLiteral("sessionPath")).toString().trimmed();
    }
    if (sessionHandle.isEmpty()) {
        sessionHandle = sessionPathHint.trimmed();
    }
    if (!sessionHandle.isEmpty()) {
        results.insert(QStringLiteral("session_handle"), sessionHandle);
    } else if (!results.contains(QStringLiteral("session_handle"))) {
        results.insert(QStringLiteral("session_handle"), QString());
    }
    if (!results.contains(QStringLiteral("streams"))) {
        results.insert(QStringLiteral("streams"), QVariantList{});
    }
    out.insert(QStringLiteral("results"), results);
    if (!out.contains(QStringLiteral("session_handle"))) {
        out.insert(QStringLiteral("session_handle"), results.value(QStringLiteral("session_handle")));
    }
    return out;
}

QVariantList normalizeCaptureStreams(const QVariantList &input)
{
    QVariantList out;
    for (const QVariant &value : input) {
        const QVariantMap stream = unmarshalVariantMap(value);
        if (stream.isEmpty()) {
            continue;
        }
        QVariantMap normalized = stream;
        if (!normalized.contains(QStringLiteral("node_id"))
            && normalized.contains(QStringLiteral("pipewire_node_id"))) {
            normalized.insert(QStringLiteral("node_id"),
                              normalized.value(QStringLiteral("pipewire_node_id")));
        }
        if (!normalized.contains(QStringLiteral("source_type"))) {
            normalized.insert(QStringLiteral("source_type"), QStringLiteral("monitor"));
        }
        if (!normalized.contains(QStringLiteral("cursor_mode"))) {
            normalized.insert(QStringLiteral("cursor_mode"), QStringLiteral("embedded"));
        }
        out.push_back(normalized);
    }
    return out;
}

QVariantList queryScreencastStreamsFromCaptureProvider(const QString &sessionPath,
                                                       const QString &appId,
                                                       const QVariantMap &options)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kCaptureService),
                         QString::fromLatin1(kCapturePath),
                         QString::fromLatin1(kCaptureIface),
                         bus);
    if (!iface.isValid()) {
        return {};
    }

    auto extractStreams = [](const QDBusMessage &reply) -> QVariantList {
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            return {};
        }
        const QVariantMap out = unmarshalVariantMap(reply.arguments().constFirst());
        if (out.isEmpty()) {
            return {};
        }
        if (out.contains(QStringLiteral("ok")) && !out.value(QStringLiteral("ok")).toBool()) {
            return {};
        }
        QVariantList streams = unmarshalVariantMap(out.value(QStringLiteral("results")))
                                   .value(QStringLiteral("streams")).toList();
        if (streams.isEmpty()) {
            streams = out.value(QStringLiteral("streams")).toList();
        }
        return normalizeCaptureStreams(streams);
    };

    QDBusMessage reply = iface.call(QStringLiteral("GetScreencastStreams"),
                                    sessionPath,
                                    appId,
                                    options);
    QVariantList streams = extractStreams(reply);
    if (!streams.isEmpty()) {
        return streams;
    }

    // Backward-compatible fallback signature: (sessionPath, options)
    reply = iface.call(QStringLiteral("GetScreencastStreams"),
                       sessionPath,
                       options);
    return extractStreams(reply);
}

QVariantList queryScreencastStreamsFromScreencastService(const QString &sessionPath)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kScreencastService),
                         QString::fromLatin1(kScreencastPath),
                         QString::fromLatin1(kScreencastIface),
                         bus);
    if (!iface.isValid()) {
        return {};
    }
    QDBusMessage reply = iface.call(QStringLiteral("GetSessionStreams"), sessionPath);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QVariantMap out = unmarshalVariantMap(reply.arguments().constFirst());
    if (out.isEmpty()) {
        return {};
    }
    if (out.contains(QStringLiteral("ok")) && !out.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    QVariantList streams = unmarshalVariantMap(out.value(QStringLiteral("results")))
                               .value(QStringLiteral("streams")).toList();
    if (streams.isEmpty()) {
        streams = out.value(QStringLiteral("streams")).toList();
    }
    return normalizeCaptureStreams(streams);
}

QString normalizeAcceleratorToken(const QString &value)
{
    QString out = value.trimmed().toLower();
    out.remove(' ');
    return out;
}

QVariantList normalizedBindingList(const QVariantMap &options)
{
    QVariantList out;
    const QVariantList raw = options.value(QStringLiteral("bindings")).toList();
    for (const QVariant &value : raw) {
        QVariantMap row = unmarshalVariantMap(value);
        const QString id = row.value(QStringLiteral("id")).toString().trimmed();
        const QString accel = row.value(QStringLiteral("accelerator")).toString().trimmed();
        if (id.isEmpty() || accel.isEmpty()) {
            continue;
        }
        row.insert(QStringLiteral("id"), id);
        row.insert(QStringLiteral("accelerator"), accel);
        row.insert(QStringLiteral("accelerator_key"), normalizeAcceleratorToken(accel));
        if (!row.contains(QStringLiteral("enabled"))) {
            row.insert(QStringLiteral("enabled"), true);
        }
        out.push_back(row);
    }
    return out;
}

QSettings globalShortcutsSettings()
{
    QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.trimmed().isEmpty()) {
        configHome = QDir::home().filePath(QStringLiteral(".config"));
    }
    const QString configPath = QDir(configHome).filePath(QStringLiteral("slm/portal-shortcuts.ini"));
    return QSettings(configPath, QSettings::IniFormat);
}

QString shortcutPersistenceKey(const QString &appId)
{
    QString key = appId.trimmed().toLower();
    if (key.isEmpty()) {
        key = QStringLiteral("unknown.desktop");
    }
    return QStringLiteral("globalshortcuts/%1/bindings").arg(key);
}

void applyCaptureProviderStreamsIfMissing(QVariantMap &normalized,
                                          const QString &sessionPath,
                                          const QString &appId,
                                          const QVariantMap &options)
{
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    const QVariantList existing = results.value(QStringLiteral("streams")).toList();
    bool needsRefresh = existing.isEmpty();
    if (!needsRefresh) {
        bool anyReal = false;
        for (const QVariant &value : existing) {
            const QVariantMap stream = unmarshalVariantMap(value);
            if (stream.isEmpty()) {
                continue;
            }
            if (!stream.value(QStringLiteral("synthetic")).toBool()) {
                anyReal = true;
                break;
            }
        }
        needsRefresh = !anyReal;
    }
    if (!needsRefresh) {
        return;
    }
    QVariantList streams = queryScreencastStreamsFromScreencastService(sessionPath);
    QString streamProvider = QStringLiteral("screencast-service");
    if (streams.isEmpty()) {
        streams = queryScreencastStreamsFromCaptureProvider(sessionPath, appId, options);
        streamProvider = QStringLiteral("capture-service");
    }
    if (streams.isEmpty()) {
        return;
    }
    results.insert(QStringLiteral("streams"), streams);
    results.insert(QStringLiteral("stream_provider"), streamProvider);
    normalized.insert(QStringLiteral("results"), results);
}

void notifyCaptureSessionLifecycle(const QString &sessionPath,
                                   bool revoked,
                                   const QString &reason)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    const QString normalizedReason = reason.trimmed().isEmpty()
        ? QStringLiteral("revoked-by-policy")
        : reason.trimmed();

    // Preferred path: publish lifecycle on dedicated screencast service.
    QDBusInterface screencastIface(QString::fromLatin1(kScreencastService),
                                   QString::fromLatin1(kScreencastPath),
                                   QString::fromLatin1(kScreencastIface),
                                   bus);
    bool delivered = false;
    if (screencastIface.isValid()) {
        if (revoked) {
            const QDBusMessage reply =
                screencastIface.call(QStringLiteral("RevokeSession"), session, normalizedReason);
            delivered = (reply.type() != QDBusMessage::ErrorMessage);
        } else {
            const QDBusMessage reply =
                screencastIface.call(QStringLiteral("EndSession"), session);
            delivered = (reply.type() != QDBusMessage::ErrorMessage);
        }
    }
    if (delivered) {
        return;
    }

    // Compatibility fallback for older desktopd builds.
    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                bus);
    if (!captureIface.isValid()) {
        return;
    }
    if (revoked) {
        captureIface.call(QStringLiteral("RevokeScreencastSession"), session, normalizedReason);
    } else {
        captureIface.call(QStringLiteral("ClearScreencastSession"), session);
    }
}

void upsertCaptureSessionStreams(const QString &sessionPath, const QVariantList &streams)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty() || streams.isEmpty()) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    // Preferred path: dedicated screencast service emits stream update signal.
    QDBusInterface screencastIface(QString::fromLatin1(kScreencastService),
                                   QString::fromLatin1(kScreencastPath),
                                   QString::fromLatin1(kScreencastIface),
                                   bus);
    bool delivered = false;
    if (screencastIface.isValid()) {
        const QDBusMessage reply =
            screencastIface.call(QStringLiteral("UpdateSessionStreams"), session, streams);
        delivered = (reply.type() != QDBusMessage::ErrorMessage);
    }
    if (delivered) {
        return;
    }

    // Compatibility fallback for older desktopd builds.
    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                bus);
    if (!captureIface.isValid()) {
        return;
    }
    captureIface.call(QStringLiteral("SetScreencastSessionStreams"), session, streams, QVariantMap{});
}

QVariantMap buildSettingsSnapshot()
{
    QVariantMap appearance;
    QVariantMap desktopInterface;

    QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.trimmed().isEmpty()) {
        configHome = QDir::home().filePath(QStringLiteral(".config"));
    }
    const QString configPath = QDir(configHome).filePath(QStringLiteral("slm/portald.conf"));
    QSettings cfg(configPath, QSettings::IniFormat);

    uint colorScheme = 0u; // 0=no-preference, 1=prefer-dark, 2=prefer-light
    const QString mode = cfg.value(QStringLiteral("appearance_theme_mode")).toString().trimmed().toLower();
    if (mode == QStringLiteral("dark")) {
        colorScheme = 1u;
    } else if (mode == QStringLiteral("light")) {
        colorScheme = 2u;
    }
    appearance.insert(QStringLiteral("color-scheme"), colorScheme);
    appearance.insert(QStringLiteral("contrast"), 0u);

    const QString iconTheme = cfg.value(QStringLiteral("appearance_icon_theme"),
                                        QStringLiteral("breeze")).toString().trimmed();
    desktopInterface.insert(QStringLiteral("color-scheme"), colorScheme);
    desktopInterface.insert(QStringLiteral("icon-theme"),
                            iconTheme.isEmpty() ? QStringLiteral("breeze") : iconTheme);

    QVariantMap out;
    out.insert(QStringLiteral("org.freedesktop.appearance"), appearance);
    out.insert(QStringLiteral("org.freedesktop.desktop.interface"), desktopInterface);
    return out;
}

bool allowNotificationForApp(const QString &appId)
{
    static QHash<QString, QList<qint64>> s_history;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString key = appId.trimmed().isEmpty() ? QStringLiteral("unknown.desktop")
                                                   : appId.trimmed().toLower();
    QList<qint64> &times = s_history[key];
    while (!times.isEmpty() && (now - times.front()) > kNotificationRateWindowMs) {
        times.pop_front();
    }
    if (times.size() >= kNotificationRateMaxPerWindow) {
        return false;
    }
    times.push_back(now);
    return true;
}

struct DocumentTokenEntry
{
    QString token;
    QString appId;
    QString uri;
    qint64 createdAtMs = 0;
};

QString normalizeAppKey(const QString &appId)
{
    const QString trimmed = appId.trimmed().toLower();
    return trimmed.isEmpty() ? QStringLiteral("unknown.desktop") : trimmed;
}

QString makeDocumentToken(const QString &appId)
{
    static quint64 seq = 0;
    ++seq;
    return QStringLiteral("doc_%1_%2")
        .arg(normalizeAppKey(appId).replace('.', '_'))
        .arg(QString::number(QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(seq)));
}

QHash<QString, DocumentTokenEntry> &documentTokenStore()
{
    static QHash<QString, DocumentTokenEntry> store;
    return store;
}

QString iconNameFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return QString::fromUtf8(names[0]);
        }
    }
    return QString();
}

QVariantMap appPresentationForDesktopId(const QString &appId)
{
    QVariantMap out{
        {QStringLiteral("app_label"), appId},
        {QStringLiteral("icon_name"), QStringLiteral("application-x-executable")},
    };
    QString desktopId = appId.trimmed();
    if (desktopId.isEmpty()) {
        return out;
    }
    if (!desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        desktopId += QStringLiteral(".desktop");
    }
    GDesktopAppInfo *info = g_desktop_app_info_new(desktopId.toUtf8().constData());
    if (!info) {
        return out;
    }
    const char *displayName = g_app_info_get_display_name(G_APP_INFO(info));
    if (displayName && *displayName) {
        out.insert(QStringLiteral("app_label"), QString::fromUtf8(displayName));
    }
    const QString iconName = iconNameFromGIcon(g_app_info_get_icon(G_APP_INFO(info)));
    if (!iconName.trimmed().isEmpty()) {
        out.insert(QStringLiteral("icon_name"), iconName);
    }
    g_object_unref(info);
    return out;
}

QString contentTypeForPath(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        return QString();
    }
    GFile *file = g_file_new_for_path(QFile::encodeName(fi.absoluteFilePath()).constData());
    if (!file) {
        return QString();
    }
    GError *error = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &error);
    QString contentType;
    if (info) {
        const char *ct = g_file_info_get_content_type(info);
        if (ct) {
            contentType = QString::fromUtf8(ct);
        }
        g_object_unref(info);
    }
    if (error) {
        g_error_free(error);
    }
    g_object_unref(file);
    return contentType;
}

QVariantList queryOpenWithHandlers(const QString &path, int limit)
{
    const QString contentType = contentTypeForPath(path);
    if (contentType.isEmpty()) {
        return {};
    }

    GAppInfo *defaultApp = g_app_info_get_default_for_type(contentType.toUtf8().constData(), false);
    GList *apps = g_app_info_get_all_for_type(contentType.toUtf8().constData());
    const int hardLimit = limit <= 0 ? 64 : qMin(limit, 256);
    int count = 0;
    QVariantList out;
    for (GList *it = apps; it != nullptr && count < hardLimit; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), QString::fromUtf8(g_app_info_get_id(info)));
        row.insert(QStringLiteral("name"), QString::fromUtf8(g_app_info_get_display_name(info)));
        row.insert(QStringLiteral("default"),
                   defaultApp != nullptr && g_app_info_equal(defaultApp, info));
        row.insert(QStringLiteral("icon"), iconNameFromGIcon(g_app_info_get_icon(info)));
        out.push_back(row);
        ++count;
    }
    if (defaultApp) {
        g_object_unref(defaultApp);
    }
    g_list_free_full(apps, g_object_unref);
    return out;
}

bool launchWithDesktopApp(const QString &desktopId, const QString &target, bool targetIsUri)
{
    const QString desktopFileId = desktopId.trimmed();
    if (desktopFileId.isEmpty()) {
        return false;
    }
    GDesktopAppInfo *app = g_desktop_app_info_new(desktopFileId.toUtf8().constData());
    if (!app) {
        return false;
    }
    GList *uris = nullptr;
    const QString uri = targetIsUri ? target.trimmed() : QUrl::fromLocalFile(target).toString();
    uris = g_list_append(uris, g_strdup(uri.toUtf8().constData()));
    GError *error = nullptr;
    const bool ok = g_app_info_launch_uris(G_APP_INFO(app), uris, nullptr, &error);
    g_list_free_full(uris, g_free);
    if (error) {
        g_error_free(error);
    }
    g_object_unref(app);
    return ok;
}

QVariantMap callInputCaptureService(const QString &method,
                                    const QString &sessionPath,
                                    const QVariantList &barriers = {},
                                    const QString &appId = QString(),
                                    const QString &reason = QString(),
                                    const QVariantMap &options = {})
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kInputCaptureService),
                         QString::fromLatin1(kInputCapturePath),
                         QString::fromLatin1(kInputCaptureIface),
                         bus);
    if (!iface.isValid()) {
        return {};
    }
    if (method == QStringLiteral("CreateSession")) {
        QDBusReply<QVariantMap> reply =
            iface.call(method, sessionPath, appId, options);
        return reply.isValid() ? reply.value() : QVariantMap{};
    }
    if (method == QStringLiteral("SetPointerBarriers")) {
        QDBusReply<QVariantMap> reply =
            iface.call(method, sessionPath, barriers, QVariantMap{});
        return reply.isValid() ? reply.value() : QVariantMap{};
    }
    if (method == QStringLiteral("EnableSession")
        || method == QStringLiteral("DisableSession")) {
        QDBusReply<QVariantMap> reply =
            iface.call(method, sessionPath, options);
        return reply.isValid() ? reply.value() : QVariantMap{};
    }
    if (method == QStringLiteral("ReleaseSession")) {
        QDBusReply<QVariantMap> reply =
            iface.call(method, sessionPath, reason);
        return reply.isValid() ? reply.value() : QVariantMap{};
    }
    return {};
}
}

ImplPortalService::ImplPortalService(PortalManager *manager,
                                     Slm::PortalAdapter::PortalBackendService *backend,
                                     QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_backend(backend)
{
    using namespace Slm::Permissions;
    m_store.open();
    m_audit.setStore(&m_store);
    m_engine.setStore(&m_store);
    m_broker.setStore(&m_store);
    m_broker.setPolicyEngine(&m_engine);
    m_broker.setAuditLogger(&m_audit);

    m_guard.setTrustResolver(&m_resolver);
    m_guard.setPermissionBroker(&m_broker);
    // m_guard.setEnabled(false); // Re-enabled (implicitly true by default, but let's be sure)
    m_guard.setEnabled(true);
    const QString iface = QStringLiteral("org.freedesktop.impl.portal.desktop.slm");
    m_guard.registerMethodCapability(iface, QStringLiteral("FileChooser"),       Capability::PortalFileChooser);
    m_guard.registerMethodCapability(iface, QStringLiteral("OpenFile"),          Capability::PortalFileChooser);
    m_guard.registerMethodCapability(iface, QStringLiteral("SaveFile"),          Capability::PortalFileChooser);
    m_guard.registerMethodCapability(iface, QStringLiteral("OpenURI"),           Capability::PortalOpenURI);
    m_guard.registerMethodCapability(iface, QStringLiteral("Screenshot"),        Capability::PortalScreenshot);
    m_guard.registerMethodCapability(iface, QStringLiteral("CreateSession"),     Capability::PortalScreencastManage);
    m_guard.registerMethodCapability(iface, QStringLiteral("SelectSources"),     Capability::PortalScreencastManage);
    m_guard.registerMethodCapability(iface, QStringLiteral("Start"),             Capability::PortalScreencastManage);
    m_guard.registerMethodCapability(iface, QStringLiteral("Stop"),              Capability::PortalScreencastManage);
    m_guard.registerMethodCapability(iface, QStringLiteral("BindShortcuts"),     Capability::GlobalShortcutsRegister);
    m_guard.registerMethodCapability(iface, QStringLiteral("ListBindings"),      Capability::GlobalShortcutsList);
    m_guard.registerMethodCapability(iface, QStringLiteral("Activate"),          Capability::GlobalShortcutsActivate);
    m_guard.registerMethodCapability(iface, QStringLiteral("Deactivate"),        Capability::GlobalShortcutsDeactivate);
    m_guard.registerMethodCapability(iface, QStringLiteral("SetPointerBarriers"), Capability::InputCaptureSetPointerBarriers);
    m_guard.registerMethodCapability(iface, QStringLiteral("Enable"),             Capability::InputCaptureEnable);
    m_guard.registerMethodCapability(iface, QStringLiteral("Disable"),            Capability::InputCaptureDisable);
    m_guard.registerMethodCapability(iface, QStringLiteral("Release"),            Capability::InputCaptureRelease);
    m_guard.registerMethodCapability(iface, QStringLiteral("AddNotification"),   Capability::PortalNotificationSend);
    m_guard.registerMethodCapability(iface, QStringLiteral("Inhibit"),           Capability::PortalInhibit);
    m_guard.registerMethodCapability(iface, QStringLiteral("QueryHandlers"),     Capability::PortalOpenWith);
    m_guard.registerMethodCapability(iface, QStringLiteral("OpenFileWith"),      Capability::PortalOpenWith);
    m_guard.registerMethodCapability(iface, QStringLiteral("OpenURIWith"),       Capability::PortalOpenWith);

    new ImplPortalFileChooserAdaptor(this);
    new ImplPortalOpenURIAdaptor(this);
    new ImplPortalScreenshotAdaptor(this);
    new ImplPortalScreenCastAdaptor(this);
    new ImplPortalGlobalShortcutsAdaptor(this);
    new ImplPortalInputCaptureAdaptor(this);
    new ImplPortalSettingsAdaptor(this);
    new ImplPortalNotificationAdaptor(this);
    new ImplPortalInhibitAdaptor(this);
    new ImplPortalOpenWithAdaptor(this);
    new ImplPortalDocumentsAdaptor(this);
    new ImplPortalTrashAdaptor(this);
    new ImplPortalPrintAdaptor(this);
    registerDbusService();
}

ImplPortalService::~ImplPortalService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool ImplPortalService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString ImplPortalService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap ImplPortalService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("apiVersion"), apiVersion()},
    };
}

QVariantMap ImplPortalService::GetBackendInfo() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("path"), QString::fromLatin1(kPath)},
        {QStringLiteral("apiVersion"), apiVersion()},
        {QStringLiteral("bridge"), QStringLiteral("org.slm.Desktop.Portal")},
    };
}

QVariantMap ImplPortalService::FileChooser(const QString &handle,
                                           const QString &appId,
                                           const QString &parentWindow,
                                           const QVariantMap &options)
{
    return BridgeFileChooser(handle, appId, parentWindow, options);
}

QVariantMap ImplPortalService::OpenURI(const QString &handle,
                                       const QString &appId,
                                       const QString &parentWindow,
                                       const QString &uri,
                                       const QVariantMap &options)
{
    return BridgeOpenURI(handle, appId, parentWindow, uri, options);
}

QVariantMap ImplPortalService::Screenshot(const QString &handle,
                                          const QString &appId,
                                          const QString &parentWindow,
                                          const QVariantMap &options)
{
    return BridgeScreenshot(handle, appId, parentWindow, options);
}

QVariantMap ImplPortalService::ScreenCast(const QString &handle,
                                          const QString &appId,
                                          const QString &parentWindow,
                                          const QVariantMap &options)
{
    return BridgeScreenCastCreateSession(handle, appId, parentWindow, options);
}

QVariantMap ImplPortalService::BridgeFileChooser(const QString &handle,
                                                 const QString &appId,
                                                 const QString &parentWindow,
                                                 const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalFileChooser);
        if (!decision.isAllowed()) {
            return normalizeFileChooserResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("FileChooser")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizeFileChooserResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("FileChooser"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizeFileChooserResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("FileChooser"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    const QString mode = ctx.value(QStringLiteral("mode"), QStringLiteral("open"))
                             .toString()
                             .trimmed()
                             .toLower();
    if (mode != QStringLiteral("open") && mode != QStringLiteral("save")) {
        return normalizeFileChooserResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("FileChooser"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-mode")},
                 {QStringLiteral("mode"), mode}}));
    }

    if (!m_manager) {
        return normalizeFileChooserResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("FileChooser")));
    }
    return normalizeFileChooserResult(m_manager->FileChooser(ctx));
}

QVariantMap ImplPortalService::BridgeOpenURI(const QString &handle,
                                             const QString &appId,
                                             const QString &parentWindow,
                                             const QString &uri,
                                             const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalOpenURI);
        if (!decision.isAllowed()) {
            QVariantMap out = normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("OpenURI")));
            QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
            results.insert(QStringLiteral("handled"), false);
            results.insert(QStringLiteral("uri"), QString());
            out.insert(QStringLiteral("results"), results);
            return out;
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenURI"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("handled"), false);
        results.insert(QStringLiteral("uri"), QString());
        out.insert(QStringLiteral("results"), results);
        return out;
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenURI"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("handled"), false);
        results.insert(QStringLiteral("uri"), QString());
        out.insert(QStringLiteral("results"), results);
        return out;
    }
    const QString normalizedUri = uri.trimmed();
    if (normalizedUri.isEmpty() || !QUrl(normalizedUri).isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenURI"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-uri")},
                 {QStringLiteral("uri"), normalizedUri}}));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("handled"), false);
        results.insert(QStringLiteral("uri"), normalizedUri);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    if (!m_manager) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("OpenURI")));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("handled"), false);
        if (!results.contains(QStringLiteral("uri"))) {
            results.insert(QStringLiteral("uri"), QString());
        }
        out.insert(QStringLiteral("results"), results);
        return out;
    }
    QVariantMap out = normalizePortalLikeResult(m_manager->OpenURI(normalizedUri, ctx));
    QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
    const bool handled = out.value(QStringLiteral("ok")).toBool()
                         && (out.value(QStringLiteral("launched")).toBool()
                             || out.value(QStringLiteral("dryRun")).toBool());
    results.insert(QStringLiteral("handled"), handled);
    if (!results.contains(QStringLiteral("uri"))) {
        results.insert(QStringLiteral("uri"), QString());
    }
    out.insert(QStringLiteral("results"), results);
    return out;
}

QVariantMap ImplPortalService::GetScreencastState() const
{
    QStringList activeSessions = m_activeScreencastSessions.values();
    std::sort(activeSessions.begin(), activeSessions.end());
    QStringList activeApps;
    QVariantList activeSessionItems;
    activeApps.reserve(activeSessions.size());
    for (const QString &session : activeSessions) {
        const QString appId = m_screencastSessionOwners.value(session).trimmed();
        const QVariantMap appPresentation = appPresentationForDesktopId(appId);
        QVariantMap row{
            {QStringLiteral("session"), session},
            {QStringLiteral("app_id"), appId},
            {QStringLiteral("app_label"), appPresentation.value(QStringLiteral("app_label"))},
            {QStringLiteral("icon_name"), appPresentation.value(QStringLiteral("icon_name"))},
        };
        activeSessionItems.push_back(row);
        if (appId.isEmpty()) {
            continue;
        }
        if (!activeApps.contains(appId)) {
            activeApps.push_back(appId);
        }
    }
    QVariantMap results;
    results.insert(QStringLiteral("active"), !m_activeScreencastSessions.isEmpty());
    results.insert(QStringLiteral("active_count"), m_activeScreencastSessions.size());
    results.insert(QStringLiteral("active_sessions"), activeSessions);
    results.insert(QStringLiteral("active_apps"), activeApps);
    results.insert(QStringLiteral("active_session_items"), activeSessionItems);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("service"), QString::fromLatin1(kService));
    out.insert(QStringLiteral("apiVersion"), QString::fromLatin1(kApiVersion));
    out.insert(QStringLiteral("results"), results);
    return out;
}

QVariantMap ImplPortalService::CloseScreencastSession(const QString &sessionHandle)
{
    const QString normalized = sessionHandle.trimmed();
    if (normalized.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("CloseScreencastSession"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-handle")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CloseScreencastSession")));
    }
    QVariantMap closeOut;
    const bool invoked = QMetaObject::invokeMethod(m_backend,
                                                    "closeSession",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, closeOut),
                                                    Q_ARG(QString, normalized));
    if (!invoked) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("CloseScreencastSession"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")},
                 {QStringLiteral("session_handle"), normalized}}));
    }

    const bool ok = closeOut.value(QStringLiteral("ok")).toBool();
    if (ok) {
        m_activeScreencastSessions.remove(normalized);
        const QString appId = m_screencastSessionOwners.take(normalized);
        notifyCaptureSessionLifecycle(normalized, false, QString());
        emit ScreencastSessionStateChanged(normalized, appId, false, m_activeScreencastSessions.size());
    }
    QVariantMap results;
    results.insert(QStringLiteral("closed"), ok);
    results.insert(QStringLiteral("session_handle"), normalized);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), ok);
    out.insert(QStringLiteral("response"), ok ? 0u : 3u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::RevokeScreencastSession(const QString &sessionHandle,
                                                       const QString &reason)
{
    const QString normalized = sessionHandle.trimmed();
    if (normalized.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("RevokeScreencastSession"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-handle")}}));
    }
    const QString revokeReason = reason.trimmed().isEmpty() ? QStringLiteral("revoked-by-policy")
                                                            : reason.trimmed();
    QVariantMap revokeOut;
    bool invoked = false;
    if (m_backend) {
        invoked = QMetaObject::invokeMethod(m_backend,
                                            "revokeSession",
                                            Qt::DirectConnection,
                                            Q_RETURN_ARG(QVariantMap, revokeOut),
                                            Q_ARG(QString, normalized),
                                            Q_ARG(QString, revokeReason));
        if (!invoked) {
            // Compatibility fallback: when revoke operation is unavailable, close session instead.
            invoked = QMetaObject::invokeMethod(m_backend,
                                                "closeSession",
                                                Qt::DirectConnection,
                                                Q_RETURN_ARG(QVariantMap, revokeOut),
                                                Q_ARG(QString, normalized));
            revokeOut.insert(QStringLiteral("reason"), revokeReason);
            revokeOut.insert(QStringLiteral("state"), QStringLiteral("Revoked"));
        }
    }

    if (!invoked) {
        if (m_screencastSessionOwners.contains(normalized)) {
            revokeOut = {{QStringLiteral("ok"), true},
                         {QStringLiteral("revoked"), true},
                         {QStringLiteral("reason"), revokeReason},
                         {QStringLiteral("state"), QStringLiteral("Revoked")}};
            invoked = true;
        }
    }

    if (!invoked) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("RevokeScreencastSession"),
                {{QStringLiteral("reason"), m_backend ? QStringLiteral("backend-invoke-failed") : QStringLiteral("backend-unavailable")},
                 {QStringLiteral("session_handle"), normalized}}));
    }

    const bool ok = revokeOut.value(QStringLiteral("ok")).toBool();
    if (ok) {
        m_activeScreencastSessions.remove(normalized);
        const QString appId = m_screencastSessionOwners.take(normalized);
        notifyCaptureSessionLifecycle(normalized, true, revokeReason);
        emit ScreencastSessionStateChanged(normalized, appId, false, m_activeScreencastSessions.size());
        emit ScreencastSessionRevoked(normalized,
                                      appId,
                                      revokeReason,
                                      m_activeScreencastSessions.size());
    }

    QVariantMap results;
    results.insert(QStringLiteral("revoked"), ok);
    results.insert(QStringLiteral("session_handle"), normalized);
    results.insert(QStringLiteral("reason"), revokeReason);
    results.insert(QStringLiteral("state"), ok ? QStringLiteral("Revoked") : QStringLiteral("Failed"));

    QVariantMap out;
    out.insert(QStringLiteral("ok"), ok);
    out.insert(QStringLiteral("response"), ok ? 0u : 3u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::CloseAllScreencastSessions()
{
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CloseAllScreencastSessions")));
    }

    QStringList sessions = m_activeScreencastSessions.values();
    std::sort(sessions.begin(), sessions.end());
    int closed = 0;
    QVariantList failed;
    for (const QString &session : sessions) {
        QVariantMap closeOut;
        const bool invoked = QMetaObject::invokeMethod(m_backend,
                                                        "closeSession",
                                                        Qt::DirectConnection,
                                                        Q_RETURN_ARG(QVariantMap, closeOut),
                                                        Q_ARG(QString, session));
        const bool ok = invoked && closeOut.value(QStringLiteral("ok")).toBool();
        if (ok) {
            ++closed;
            m_activeScreencastSessions.remove(session);
            const QString appId = m_screencastSessionOwners.take(session);
            notifyCaptureSessionLifecycle(session, false, QString());
            emit ScreencastSessionStateChanged(session, appId, false, m_activeScreencastSessions.size());
        } else {
            failed.push_back(session);
        }
    }
    QVariantMap results;
    results.insert(QStringLiteral("closed_count"), closed);
    results.insert(QStringLiteral("failed_sessions"), failed);
    results.insert(QStringLiteral("remaining_count"), m_activeScreencastSessions.size());

    QVariantMap out;
    out.insert(QStringLiteral("ok"), failed.isEmpty());
    out.insert(QStringLiteral("response"), failed.isEmpty() ? 0u : 3u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeScreenshot(const QString &handle,
                                                const QString &appId,
                                                const QString &parentWindow,
                                                const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalScreenshot);
        if (!decision.isAllowed()) {
            return normalizeScreenshotResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("Screenshot")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizeScreenshotResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Screenshot"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizeScreenshotResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Screenshot"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }

    if (!m_manager) {
        return normalizeScreenshotResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("Screenshot")));
    }
    if (m_backend) {
        QVariantMap out;
        const bool ok = QMetaObject::invokeMethod(m_backend,
                                                   "handleRequest",
                                                   Qt::DirectConnection,
                                                   Q_RETURN_ARG(QVariantMap, out),
                                                   Q_ARG(QString, QStringLiteral("CaptureScreen")),
                                                   Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                       : QDBusMessage()),
                                                   Q_ARG(QVariantMap, ctx));
        if (ok) {
            out.insert(QStringLiteral("implPortalMethod"), QStringLiteral("Screenshot"));
            return normalizeScreenshotResult(out);
        }
    }
    return normalizeScreenshotResult(m_manager->Screenshot(ctx));
}

QVariantMap ImplPortalService::BridgeScreenCastCreateSession(const QString &handle,
                                                             const QString &appId,
                                                             const QString &parentWindow,
                                                             const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalScreencastManage);
        if (!decision.isAllowed()) {
            return normalizeScreencastResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("ScreenCast.CreateSession")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreenCast.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreenCast.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }

    if (!m_manager) {
        return normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreenCast")));
    }
    if (m_backend) {
        QVariantMap out;
        const bool ok = QMetaObject::invokeMethod(m_backend,
                                                   "handleSessionRequest",
                                                   Qt::DirectConnection,
                                                   Q_RETURN_ARG(QVariantMap, out),
                                                   Q_ARG(QString, QStringLiteral("StartScreencastSession")),
                                                   Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                       : QDBusMessage()),
                                                   Q_ARG(QVariantMap, ctx));
        if (ok) {
            out.insert(QStringLiteral("implPortalMethod"), QStringLiteral("ScreenCast"));
            QVariantMap normalized = normalizeScreencastResult(out);
            const QString sessionHandle =
                normalized.value(QStringLiteral("session_handle")).toString().trimmed();
            if (normalized.value(QStringLiteral("ok")).toBool() && !sessionHandle.isEmpty()) {
                const QString owner = ctx.value(QStringLiteral("appId")).toString().trimmed();
                if (!owner.isEmpty()) {
                    m_screencastSessionOwners.insert(sessionHandle, owner);
                }
            }
            return normalized;
        }
    }
    QVariantMap normalized = normalizeScreencastResult(m_manager->ScreenCast(ctx));
    const QString sessionHandle = normalized.value(QStringLiteral("session_handle")).toString().trimmed();
    if (normalized.value(QStringLiteral("ok")).toBool() && !sessionHandle.isEmpty()) {
        const QString owner = ctx.value(QStringLiteral("appId")).toString().trimmed();
        if (!owner.isEmpty()) {
            m_screencastSessionOwners.insert(sessionHandle, owner);
        }
    }
    return normalized;
}

QVariantMap ImplPortalService::BridgeScreenCastSelectSources(const QString &handle,
                                                             const QString &appId,
                                                             const QString &parentWindow,
                                                             const QString &sessionPath,
                                                             const QVariantMap &options)
{
    QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastSelectSources"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("sources_selected"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastSelectSources"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("sources_selected"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    if (sessionPath.trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastSelectSources"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("sources_selected"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }

    if (!m_backend) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastSelectSources")),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("sources_selected"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleSessionOperation",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, QStringLiteral("ScreencastSelectSources")),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QString, sessionPath),
                                               Q_ARG(QVariantMap, ctx),
                                               Q_ARG(bool, false));
    if (!ok) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("ScreencastSelectSources"),
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("sources_selected"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    out.insert(QStringLiteral("implPortalMethod"), QStringLiteral("ScreenCast.SelectSources"));
    QVariantMap normalized = normalizeScreencastResult(out, sessionPath);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("sources_selected"), normalized.value(QStringLiteral("ok")).toBool());
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeScreenCastStart(const QString &handle,
                                                     const QString &appId,
                                                     const QString &parentWindow,
                                                     const QString &sessionPath,
                                                     const QVariantMap &options)
{
    QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString normalizedAppId = ctx.value(QStringLiteral("appId")).toString().trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStart"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        if (!results.contains(QStringLiteral("streams"))) {
            results.insert(QStringLiteral("streams"), QVariantList{});
        }
        normalized.insert(QStringLiteral("results"), results);
        applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
        return normalized;
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStart"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        if (!results.contains(QStringLiteral("streams"))) {
            results.insert(QStringLiteral("streams"), QVariantList{});
        }
        normalized.insert(QStringLiteral("results"), results);
        applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
        return normalized;
    }
    if (sessionPath.trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStart"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        if (!results.contains(QStringLiteral("streams"))) {
            results.insert(QStringLiteral("streams"), QVariantList{});
        }
        normalized.insert(QStringLiteral("results"), results);
        applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
        return normalized;
    }

    if (!m_backend) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastStart")),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        if (!results.contains(QStringLiteral("streams"))) {
            results.insert(QStringLiteral("streams"), QVariantList{});
        }
        normalized.insert(QStringLiteral("results"), results);
        applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
        return normalized;
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleSessionOperation",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, QStringLiteral("ScreencastStart")),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QString, sessionPath),
                                               Q_ARG(QVariantMap, ctx),
                                               Q_ARG(bool, true));
    if (!ok) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("ScreencastStart"),
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        if (!results.contains(QStringLiteral("streams"))) {
            results.insert(QStringLiteral("streams"), QVariantList{});
        }
        normalized.insert(QStringLiteral("results"), results);
        applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
        return normalized;
    }
    out.insert(QStringLiteral("implPortalMethod"), QStringLiteral("ScreenCast.Start"));
    QVariantMap normalized = normalizeScreencastResult(out, sessionPath);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    const bool started = normalized.value(QStringLiteral("ok")).toBool();
    const QString sessionHandle =
        results.value(QStringLiteral("session_handle")).toString().trimmed().isEmpty()
            ? sessionPath.trimmed()
            : results.value(QStringLiteral("session_handle")).toString().trimmed();
    if (started && !sessionHandle.isEmpty()) {
        m_activeScreencastSessions.insert(sessionHandle);
        const QString appId = m_screencastSessionOwners.value(sessionHandle);
        emit ScreencastSessionStateChanged(sessionHandle,
                                           appId,
                                           true,
                                           m_activeScreencastSessions.size());
    }
    if (!results.contains(QStringLiteral("streams"))) {
        results.insert(QStringLiteral("streams"), QVariantList{});
    }
    normalized.insert(QStringLiteral("results"), results);
    applyCaptureProviderStreamsIfMissing(normalized, sessionPath, normalizedAppId, ctx);
    const QVariantList finalStreams =
        unmarshalVariantMap(normalized.value(QStringLiteral("results"))).value(QStringLiteral("streams")).toList();
    upsertCaptureSessionStreams(sessionHandle, finalStreams);
    return normalized;
}

QVariantMap ImplPortalService::BridgeScreenCastStop(const QString &handle,
                                                    const QString &appId,
                                                    const QString &parentWindow,
                                                    const QString &sessionPath,
                                                    const QVariantMap &options)
{
    QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStop"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("stopped"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStop"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("stopped"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    if (sessionPath.trimmed().isEmpty()) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("ScreencastStop"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("stopped"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }

    if (!m_backend) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastStop")),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("stopped"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleSessionOperation",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, QStringLiteral("ScreencastStop")),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QString, sessionPath),
                                               Q_ARG(QVariantMap, ctx),
                                               Q_ARG(bool, false));
    if (!ok) {
        QVariantMap normalized = normalizeScreencastResult(
            SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("ScreencastStop"),
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}),
            sessionPath);
        QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
        results.insert(QStringLiteral("stopped"), false);
        normalized.insert(QStringLiteral("results"), results);
        return normalized;
    }
    out.insert(QStringLiteral("implPortalMethod"), QStringLiteral("ScreenCast.Stop"));
    QVariantMap normalized = normalizeScreencastResult(out, sessionPath);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("stopped"), normalized.value(QStringLiteral("ok")).toBool());
    const bool closeSession = options.value(QStringLiteral("closeSession"), true).toBool();
    bool sessionClosed = results.value(QStringLiteral("session_closed")).toBool();
    if (!sessionClosed && normalized.value(QStringLiteral("ok")).toBool() && closeSession) {
        QVariantMap closeOut;
        const bool closeInvoked = QMetaObject::invokeMethod(m_backend,
                                                            "closeSession",
                                                            Qt::DirectConnection,
                                                            Q_RETURN_ARG(QVariantMap, closeOut),
                                                            Q_ARG(QString, sessionPath));
        sessionClosed = closeInvoked && closeOut.value(QStringLiteral("ok")).toBool();
    }
    const QString sessionHandle =
        results.value(QStringLiteral("session_handle")).toString().trimmed().isEmpty()
            ? sessionPath.trimmed()
            : results.value(QStringLiteral("session_handle")).toString().trimmed();
    if (normalized.value(QStringLiteral("ok")).toBool() && !sessionHandle.isEmpty()) {
        m_activeScreencastSessions.remove(sessionHandle);
        const QString appId = m_screencastSessionOwners.value(sessionHandle);
        emit ScreencastSessionStateChanged(sessionHandle,
                                           appId,
                                           false,
                                           m_activeScreencastSessions.size());
        if (sessionClosed) {
            m_screencastSessionOwners.remove(sessionHandle);
            notifyCaptureSessionLifecycle(sessionHandle, false, QString());
        }
    }
    results.insert(QStringLiteral("session_closed"), sessionClosed);
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeGlobalShortcutsCreateSession(const QString &handle,
                                                                  const QString &appId,
                                                                  const QString &parentWindow,
                                                                  const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::GlobalShortcutsCreateSession);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.CreateSession")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcuts.CreateSession")));
    }

    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                              "handleSessionRequest",
                                              Qt::DirectConnection,
                                              Q_RETURN_ARG(QVariantMap, out),
                                              Q_ARG(QString, QStringLiteral("GlobalShortcutsCreateSession")),
                                              Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                  : QDBusMessage()),
                                              Q_ARG(QVariantMap, ctx));
    if (!ok) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("GlobalShortcuts.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }

    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    const QString sessionHandle =
        results.value(QStringLiteral("session_handle"),
                      results.value(QStringLiteral("sessionHandle"),
                                    results.value(QStringLiteral("sessionPath")))).toString().trimmed();
    if (normalized.value(QStringLiteral("ok")).toBool() && !sessionHandle.isEmpty()) {
        const QString owner = ctx.value(QStringLiteral("appId")).toString().trimmed();
        if (!owner.isEmpty()) {
            m_globalShortcutSessionOwners.insert(sessionHandle, owner);
        }
    }
    if (options.value(QStringLiteral("restorePersisted"), true).toBool()) {
        QSettings s = globalShortcutsSettings();
        const QVariantList persisted =
            s.value(shortcutPersistenceKey(ctx.value(QStringLiteral("appId")).toString())).toList();
        if (!persisted.isEmpty()) {
            results.insert(QStringLiteral("persisted_bindings"), persisted);
        }
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeGlobalShortcutsBindShortcuts(const QString &handle,
                                                                  const QString &appId,
                                                                  const QString &parentWindow,
                                                                  const QString &sessionPath,
                                                                  const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::GlobalShortcutsRegister);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.BindShortcuts")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcuts.BindShortcuts")));
    }
    const QString owner = m_globalShortcutSessionOwners.value(session).trimmed();
    const QString callerApp = ctx.value(QStringLiteral("appId")).toString().trimmed();
    if (!owner.isEmpty() && !callerApp.isEmpty() && owner != callerApp) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.BindShortcuts")));
    }

    const QVariantList bindings = normalizedBindingList(options);
    if (bindings.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                {{QStringLiteral("reason"), QStringLiteral("empty-bindings")}}));
    }

    QVariantList conflicts;
    for (const QVariant &value : bindings) {
        const QVariantMap b = unmarshalVariantMap(value);
        const QString accelKey = b.value(QStringLiteral("accelerator_key")).toString();
        if (accelKey.isEmpty()) {
            continue;
        }
        const QString claimedBy = m_globalShortcutAcceleratorOwners.value(accelKey);
        if (!claimedBy.isEmpty() && claimedBy != session) {
            conflicts.push_back(QVariantMap{
                {QStringLiteral("id"), b.value(QStringLiteral("id"))},
                {QStringLiteral("accelerator"), b.value(QStringLiteral("accelerator"))},
                {QStringLiteral("session"), claimedBy},
            });
        }
    }
    if (!conflicts.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                QStringLiteral("shortcut-conflict"),
                {{QStringLiteral("conflicts"), conflicts}}));
    }

    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("GlobalShortcutsBindShortcuts")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, true));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("GlobalShortcuts.BindShortcuts"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    if (!normalized.value(QStringLiteral("ok")).toBool()) {
        return normalized;
    }

    const QVariantList oldBindings = m_globalShortcutSessionBindings.value(session);
    for (const QVariant &oldValue : oldBindings) {
        const QVariantMap oldBinding = unmarshalVariantMap(oldValue);
        const QString key = normalizeAcceleratorToken(oldBinding.value(QStringLiteral("accelerator")).toString());
        if (!key.isEmpty() && m_globalShortcutAcceleratorOwners.value(key) == session) {
            m_globalShortcutAcceleratorOwners.remove(key);
        }
    }
    m_globalShortcutSessionBindings.insert(session, bindings);
    for (const QVariant &value : bindings) {
        const QVariantMap b = unmarshalVariantMap(value);
        const QString accelKey = b.value(QStringLiteral("accelerator_key")).toString();
        if (!accelKey.isEmpty()) {
            m_globalShortcutAcceleratorOwners.insert(accelKey, session);
        }
    }
    if (options.value(QStringLiteral("persist"), false).toBool()) {
        QSettings s = globalShortcutsSettings();
        s.setValue(shortcutPersistenceKey(callerApp), bindings);
    }

    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("bound"), true);
    results.insert(QStringLiteral("bindings"), bindings);
    results.insert(QStringLiteral("conflicts"), QVariantList{});
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeGlobalShortcutsListBindings(const QString &handle,
                                                                 const QString &appId,
                                                                 const QString &parentWindow,
                                                                 const QString &sessionPath,
                                                                 const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::GlobalShortcutsList);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.ListBindings")));
        }
    }
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.ListBindings"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.ListBindings"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}));
    }
    const QString owner = m_globalShortcutSessionOwners.value(session).trimmed();
    const QString callerApp = ctx.value(QStringLiteral("appId")).toString().trimmed();
    if (!owner.isEmpty() && !callerApp.isEmpty() && owner != callerApp) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.ListBindings")));
    }

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), QVariantMap{
                                         {QStringLiteral("session_handle"), session},
                                         {QStringLiteral("bindings"),
                                          m_globalShortcutSessionBindings.value(session)},
                                     });
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeGlobalShortcutsActivate(const QString &handle,
                                                             const QString &appId,
                                                             const QString &parentWindow,
                                                             const QString &sessionPath,
                                                             const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::GlobalShortcutsActivate);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.Activate")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString session = sessionPath.trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty() || session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.Activate"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-request")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcuts.Activate")));
    }
    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("GlobalShortcutsActivate")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, true));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("GlobalShortcuts.Activate"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("activated"), normalized.value(QStringLiteral("ok")).toBool());
    results.insert(QStringLiteral("session_handle"), session);
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeGlobalShortcutsDeactivate(const QString &handle,
                                                               const QString &appId,
                                                               const QString &parentWindow,
                                                               const QString &sessionPath,
                                                               const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::GlobalShortcutsDeactivate);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("GlobalShortcuts.Deactivate")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString session = sessionPath.trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty() || session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("GlobalShortcuts.Deactivate"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-request")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcuts.Deactivate")));
    }

    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("GlobalShortcutsDeactivate")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, false));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("GlobalShortcuts.Deactivate"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }

    const QVariantList bindings = m_globalShortcutSessionBindings.value(session);
    for (const QVariant &value : bindings) {
        const QVariantMap binding = unmarshalVariantMap(value);
        const QString key = normalizeAcceleratorToken(binding.value(QStringLiteral("accelerator")).toString());
        if (!key.isEmpty() && m_globalShortcutAcceleratorOwners.value(key) == session) {
            m_globalShortcutAcceleratorOwners.remove(key);
        }
    }
    m_globalShortcutSessionBindings.remove(session);

    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("deactivated"), normalized.value(QStringLiteral("ok")).toBool());
    results.insert(QStringLiteral("session_handle"), session);
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeInputCaptureCreateSession(const QString &handle,
                                                               const QString &appId,
                                                               const QString &parentWindow,
                                                               const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::InputCaptureCreateSession);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("InputCapture.CreateSession")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("InputCapture.CreateSession")));
    }

    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                              "handleSessionRequest",
                                              Qt::DirectConnection,
                                              Q_RETURN_ARG(QVariantMap, out),
                                              Q_ARG(QString, QStringLiteral("InputCaptureCreateSession")),
                                              Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                  : QDBusMessage()),
                                              Q_ARG(QVariantMap, ctx));
    if (!ok) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.CreateSession"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    const QString sessionHandle =
        results.value(QStringLiteral("session_handle"),
                      results.value(QStringLiteral("sessionHandle"),
                                    results.value(QStringLiteral("sessionPath")))).toString().trimmed();
    if (normalized.value(QStringLiteral("ok")).toBool() && !sessionHandle.isEmpty()) {
        const QString owner = ctx.value(QStringLiteral("appId")).toString().trimmed();
        if (!owner.isEmpty()) {
            m_inputCaptureSessionOwners.insert(sessionHandle, owner);
        }
    }
    QVariantMap host = callInputCaptureService(QStringLiteral("CreateSession"),
                                               sessionHandle,
                                               {},
                                               ctx.value(QStringLiteral("appId")).toString().trimmed());
    if (host.contains(QStringLiteral("ok")) && !host.value(QStringLiteral("ok")).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.CreateSession"),
                {{QStringLiteral("reason"), host.value(QStringLiteral("reason")).toString()},
                 {QStringLiteral("host"), host}}));
    }
    results.insert(QStringLiteral("host_synced"), host.value(QStringLiteral("ok")).toBool());
    if (host.value(QStringLiteral("ok")).toBool()) {
        results.insert(QStringLiteral("host"), unmarshalVariantMap(host.value(QStringLiteral("results"))));
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeInputCaptureSetPointerBarriers(const QString &handle,
                                                                    const QString &appId,
                                                                    const QString &parentWindow,
                                                                    const QString &sessionPath,
                                                                    const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::InputCaptureSetPointerBarriers);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(
                    QStringLiteral("InputCapture.SetPointerBarriers")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.SetPointerBarriers"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.SetPointerBarriers"),
                {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}}));
    }
    const QString owner = m_inputCaptureSessionOwners.value(session).trimmed();
    const QString callerApp = ctx.value(QStringLiteral("appId")).toString().trimmed();
    if (!owner.isEmpty() && !callerApp.isEmpty() && owner != callerApp) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(
                QStringLiteral("InputCapture.SetPointerBarriers")));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.SetPointerBarriers")));
    }

    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("InputCaptureSetPointerBarriers")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, true));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.SetPointerBarriers"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("session_handle"), session);
    results.insert(QStringLiteral("barriers_set"), normalized.value(QStringLiteral("ok")).toBool());
    QVariantList barriers = options.value(QStringLiteral("barriers")).toList();
    QVariantMap host = callInputCaptureService(QStringLiteral("SetPointerBarriers"), session, barriers);
    if (host.contains(QStringLiteral("ok")) && !host.value(QStringLiteral("ok")).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(
                QStringLiteral("InputCapture.SetPointerBarriers"),
                {{QStringLiteral("reason"), host.value(QStringLiteral("reason")).toString()},
                 {QStringLiteral("host"), host}}));
    }
    results.insert(QStringLiteral("host_synced"), host.value(QStringLiteral("ok")).toBool());
    if (host.value(QStringLiteral("ok")).toBool()) {
        results.insert(QStringLiteral("host"), unmarshalVariantMap(host.value(QStringLiteral("results"))));
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeInputCaptureEnable(const QString &handle,
                                                        const QString &appId,
                                                        const QString &parentWindow,
                                                        const QString &sessionPath,
                                                        const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::InputCaptureEnable);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("InputCapture.Enable")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString session = sessionPath.trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty() || session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.Enable"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-request")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("InputCapture.Enable")));
    }
    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("InputCaptureEnable")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, true));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.Enable"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("enabled"), normalized.value(QStringLiteral("ok")).toBool());
    results.insert(QStringLiteral("session_handle"), session);
    QVariantMap host = callInputCaptureService(QStringLiteral("EnableSession"),
                                               session,
                                               {},
                                               QString(),
                                               QString(),
                                               options);
    const bool wantsPreempt =
        options.value(QStringLiteral("allow_preempt")).toBool()
        || options.value(QStringLiteral("allowPreempt")).toBool();
    const bool canPreempt = canAttemptInputCapturePreempt(ctx, options);
    QVariantMap mediationPolicyDetails;
    const auto persistMediationDecision = [&](const QVariantMap &consent) -> QVariantMap {
        using namespace Slm::Permissions;
        QVariantMap out;
        const QString app = ctx.value(QStringLiteral("appId")).toString().trimmed();
        const QString decisionText =
            consent.value(QStringLiteral("decision")).toString().trimmed().toLower();
        if (app.isEmpty() || decisionText.isEmpty()) {
            return out;
        }

        DecisionType decision = decisionTypeFromString(decisionText);
        if (decision == DecisionType::Deny && decisionText == QStringLiteral("allow_once")) {
            decision = DecisionType::AllowOnce;
        } else if (decision == DecisionType::Deny && decisionText == QStringLiteral("allow_always")) {
            decision = DecisionType::AllowAlways;
        } else if (decision == DecisionType::Deny && decisionText == QStringLiteral("deny_always")) {
            decision = DecisionType::DenyAlways;
        }

        const bool persistRequested = consent.value(QStringLiteral("persist")).toBool();
        const bool persistable = persistRequested
            || decision == DecisionType::AllowAlways
            || decision == DecisionType::DenyAlways;
        const QString scopeRaw = consent.value(QStringLiteral("scope")).toString().trimmed().toLower();
        QString scope = scopeRaw;
        if (scope.isEmpty()) {
            scope = persistable ? QStringLiteral("persistent") : QStringLiteral("session");
        }
        if (scope == QStringLiteral("always")) {
            scope = QStringLiteral("persistent");
        } else if (scope == QStringLiteral("once")) {
            scope = QStringLiteral("session");
        }

        QString resourceType;
        QString resourceId;
        if (scope == QStringLiteral("resource_specific") || scope == QStringLiteral("session")) {
            resourceType = QStringLiteral("inputcapture-session");
            resourceId = session;
        }

        if (persistable) {
            const bool saved = m_store.savePermission(app,
                                                      Capability::InputCaptureEnable,
                                                      decision,
                                                      scope,
                                                      resourceType,
                                                      resourceId);
            out.insert(QStringLiteral("persisted"), saved);
            out.insert(QStringLiteral("persist_scope"), scope);
            if (!resourceType.isEmpty()) {
                out.insert(QStringLiteral("persist_resource_type"), resourceType);
                out.insert(QStringLiteral("persist_resource_id"), resourceId);
            }
        }

        m_store.appendAudit(app,
                            Capability::InputCaptureEnable,
                            decision,
                            QStringLiteral("inputcapture-session"),
                            QStringLiteral("inputcapture-preempt-consent:%1")
                                .arg(consent.value(QStringLiteral("reason")).toString().left(256)));
        out.insert(QStringLiteral("decision"), decisionText);
        return out;
    };
    QString hostReason = host.value(QStringLiteral("reason")).toString().trimmed();
    if (host.contains(QStringLiteral("ok"))
        && !host.value(QStringLiteral("ok")).toBool()
        && hostReason == QStringLiteral("conflict-active-session")
        && wantsPreempt
        && canPreempt) {
        QVariantMap preemptOptions = options;
        preemptOptions.insert(QStringLiteral("allow_preempt"), true);
        if (!preemptOptions.contains(QStringLiteral("preempt_reason"))) {
            preemptOptions.insert(QStringLiteral("preempt_reason"),
                                  QStringLiteral("portal-mediated-preempt"));
        }
        host = callInputCaptureService(QStringLiteral("EnableSession"),
                                       session,
                                       {},
                                       QString(),
                                       QString(),
                                       preemptOptions);
        results.insert(QStringLiteral("host_preempt_retry"), true);
        hostReason = host.value(QStringLiteral("reason")).toString().trimmed();
    } else if (host.contains(QStringLiteral("ok"))
               && !host.value(QStringLiteral("ok")).toBool()
               && hostReason == QStringLiteral("conflict-active-session")
               && wantsPreempt
               && !canPreempt) {
        const QVariantMap hostResults = unmarshalVariantMap(host.value(QStringLiteral("results")));
        const QVariantMap consent = requestTrustedInputCapturePreemptConsent(ctx, session, hostResults);
        results.insert(QStringLiteral("host_preempt_mediation_requested"), true);
        results.insert(QStringLiteral("host_preempt_mediation"), consent);
        mediationPolicyDetails = persistMediationDecision(consent);
        if (!mediationPolicyDetails.isEmpty()) {
            results.insert(QStringLiteral("host_preempt_mediation_policy"), mediationPolicyDetails);
        }
        if (consent.value(QStringLiteral("ok")).toBool()) {
            QVariantMap preemptOptions = options;
            preemptOptions.insert(QStringLiteral("allow_preempt"), true);
            preemptOptions.insert(QStringLiteral("initiatedFromOfficialUI"), true);
            preemptOptions.insert(QStringLiteral("mediatedByTrustedUi"), true);
            if (!preemptOptions.contains(QStringLiteral("preempt_reason"))) {
                preemptOptions.insert(QStringLiteral("preempt_reason"),
                                      QStringLiteral("portal-ui-consented-preempt"));
            }
            host = callInputCaptureService(QStringLiteral("EnableSession"),
                                           session,
                                           {},
                                           QString(),
                                           QString(),
                                           preemptOptions);
            results.insert(QStringLiteral("host_preempt_retry"), true);
            results.insert(QStringLiteral("host_preempt_retry_via_consent"), true);
            hostReason = host.value(QStringLiteral("reason")).toString().trimmed();
        } else {
            hostReason = QStringLiteral("preempt-requires-trusted-mediation");
        }
    }
    if (host.contains(QStringLiteral("ok")) && !host.value(QStringLiteral("ok")).toBool()) {
        const QVariantMap hostResults = unmarshalVariantMap(host.value(QStringLiteral("results")));
        QString reason = hostReason.isEmpty()
            ? host.value(QStringLiteral("error")).toString()
            : hostReason;
        const bool requiresMediation =
            reason == QStringLiteral("preempt-requires-trusted-mediation")
                ? true
                : hostResults.value(QStringLiteral("requires_mediation")).toBool();
        const QString mediationAction =
            hostResults.value(QStringLiteral("mediation_action")).toString().trimmed().isEmpty()
                ? QStringLiteral("inputcapture-preempt-consent")
                : hostResults.value(QStringLiteral("mediation_action")).toString();
        const QString mediationScope =
            hostResults.value(QStringLiteral("mediation_scope")).toString().trimmed().isEmpty()
                ? QStringLiteral("session-conflict")
                : hostResults.value(QStringLiteral("mediation_scope")).toString();
        const QString conflictSession =
            hostResults.value(QStringLiteral("conflict_session")).toString();
        if (hostReason == QStringLiteral("conflict-active-session")
            && wantsPreempt
            && !canPreempt) {
            reason = QStringLiteral("preempt-requires-trusted-mediation");
        }
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(
                QStringLiteral("InputCapture.Enable"),
                {{QStringLiteral("reason"), reason},
                 {QStringLiteral("host"), host},
                 {QStringLiteral("requires_mediation"), requiresMediation},
                 {QStringLiteral("mediation_action"), mediationAction},
                 {QStringLiteral("mediation_scope"), mediationScope},
                 {QStringLiteral("conflict_session"), conflictSession},
                 {QStringLiteral("host_preempt_mediation_policy"), mediationPolicyDetails},
                 {QStringLiteral("mediation"),
                  QVariantMap{
                      {QStringLiteral("required"), requiresMediation},
                      {QStringLiteral("action"), mediationAction},
                      {QStringLiteral("scope"), mediationScope},
                      {QStringLiteral("reason"), reason},
                      {QStringLiteral("conflict_session"), conflictSession},
                  }}}));
    }
    results.insert(QStringLiteral("host_synced"), host.value(QStringLiteral("ok")).toBool());
    if (host.value(QStringLiteral("ok")).toBool()) {
        results.insert(QStringLiteral("host"), unmarshalVariantMap(host.value(QStringLiteral("results"))));
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeInputCaptureDisable(const QString &handle,
                                                         const QString &appId,
                                                         const QString &parentWindow,
                                                         const QString &sessionPath,
                                                         const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::InputCaptureDisable);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("InputCapture.Disable")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString session = sessionPath.trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty() || session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.Disable"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-request")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("InputCapture.Disable")));
    }
    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("InputCaptureDisable")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, true));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.Disable"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("disabled"), normalized.value(QStringLiteral("ok")).toBool());
    results.insert(QStringLiteral("session_handle"), session);
    QVariantMap host = callInputCaptureService(QStringLiteral("DisableSession"),
                                               session,
                                               {},
                                               QString(),
                                               QString(),
                                               options);
    if (host.contains(QStringLiteral("ok")) && !host.value(QStringLiteral("ok")).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(
                QStringLiteral("InputCapture.Disable"),
                {{QStringLiteral("reason"), host.value(QStringLiteral("reason")).toString()},
                 {QStringLiteral("host"), host}}));
    }
    results.insert(QStringLiteral("host_synced"), host.value(QStringLiteral("ok")).toBool());
    if (host.value(QStringLiteral("ok")).toBool()) {
        results.insert(QStringLiteral("host"), unmarshalVariantMap(host.value(QStringLiteral("results"))));
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeInputCaptureRelease(const QString &handle,
                                                         const QString &appId,
                                                         const QString &parentWindow,
                                                         const QString &sessionPath,
                                                         const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::InputCaptureRelease);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("InputCapture.Release")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    const QString session = sessionPath.trimmed();
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty() || session.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("InputCapture.Release"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-request")}}));
    }
    if (!m_backend) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("InputCapture.Release")));
    }
    QVariantMap out;
    const bool invokeOk = QMetaObject::invokeMethod(m_backend,
                                                    "handleSessionOperation",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariantMap, out),
                                                    Q_ARG(QString, QStringLiteral("InputCaptureRelease")),
                                                    Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                        : QDBusMessage()),
                                                    Q_ARG(QString, session),
                                                    Q_ARG(QVariantMap, ctx),
                                                    Q_ARG(bool, false));
    if (!invokeOk) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(
                QStringLiteral("InputCapture.Release"),
                {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}}));
    }
    QVariantMap normalized = normalizePortalLikeResult(out);
    if (normalized.value(QStringLiteral("ok")).toBool()) {
        m_inputCaptureSessionOwners.remove(session);
    }
    QVariantMap results = unmarshalVariantMap(normalized.value(QStringLiteral("results")));
    results.insert(QStringLiteral("released"), normalized.value(QStringLiteral("ok")).toBool());
    results.insert(QStringLiteral("session_handle"), session);
    QVariantMap host = callInputCaptureService(QStringLiteral("ReleaseSession"),
                                               session,
                                               {},
                                               QString(),
                                               options.value(QStringLiteral("reason")).toString().trimmed());
    if (host.contains(QStringLiteral("ok")) && !host.value(QStringLiteral("ok")).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::permissionDenied(
                QStringLiteral("InputCapture.Release"),
                {{QStringLiteral("reason"), host.value(QStringLiteral("reason")).toString()},
                 {QStringLiteral("host"), host}}));
    }
    results.insert(QStringLiteral("host_synced"), host.value(QStringLiteral("ok")).toBool());
    if (host.value(QStringLiteral("ok")).toBool()) {
        results.insert(QStringLiteral("host"), unmarshalVariantMap(host.value(QStringLiteral("results"))));
    }
    normalized.insert(QStringLiteral("results"), results);
    return normalized;
}

QVariantMap ImplPortalService::BridgeSettingsReadAll(const QStringList &namespaces) const
{
    const QVariantMap snapshot = buildSettingsSnapshot();
    if (namespaces.isEmpty()) {
        return snapshot;
    }

    QVariantMap out;
    for (const QString &ns : namespaces) {
        const QString key = ns.trimmed();
        if (key.isEmpty()) {
            continue;
        }
        if (snapshot.contains(key)) {
            out.insert(key, snapshot.value(key));
        }
    }
    return out;
}

QVariant ImplPortalService::BridgeSettingsRead(const QString &settingNamespace, const QString &key) const
{
    const QString ns = settingNamespace.trimmed();
    const QString settingKey = key.trimmed();
    if (ns.isEmpty() || settingKey.isEmpty()) {
        return QVariant{};
    }
    const QVariantMap snapshot = buildSettingsSnapshot();
    return unmarshalVariantMap(snapshot.value(ns)).value(settingKey);
}

QVariantMap ImplPortalService::BridgeAddNotification(const QString &appId,
                                                     const QString &id,
                                                     const QVariantMap &notification)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalNotificationSend);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("AddNotification")));
        }
    }
    const QString normalizedAppId = appId.trimmed().isEmpty() ? QStringLiteral("unknown.desktop")
                                                               : appId.trimmed();
    if (id.trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("AddNotification"),
                {{QStringLiteral("reason"), QStringLiteral("missing-id")},
                 {QStringLiteral("appId"), normalizedAppId}}));
    }
    if (!allowNotificationForApp(normalizedAppId)) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(
                QStringLiteral("AddNotification"),
                QStringLiteral("rate-limited"),
                {{QStringLiteral("appId"), normalizedAppId},
                 {QStringLiteral("limitWindowMs"), kNotificationRateWindowMs},
                 {QStringLiteral("limitCount"), kNotificationRateMaxPerWindow}}));
    }

    QDBusInterface iface(QString::fromLatin1(kNotificationsService),
                         QString::fromLatin1(kNotificationsPath),
                         QString::fromLatin1(kNotificationsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("AddNotification")));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("delivered"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    const QString title = notification.value(QStringLiteral("title")).toString();
    const QString body = notification.value(QStringLiteral("body")).toString();
    const QString icon = notification.value(QStringLiteral("icon")).toString();
    const QVariantMap hints = unmarshalVariantMap(notification.value(QStringLiteral("hints")));
    const int timeout = notification.value(QStringLiteral("expire_timeout"), -1).toInt();
    const QStringList actions = notification.value(QStringLiteral("actions")).toStringList();

    QDBusReply<uint> reply = iface.call(QStringLiteral("Notify"),
                                        normalizedAppId,
                                        0u,
                                        icon,
                                        title,
                                        body,
                                        actions,
                                        hints,
                                        timeout);
    if (!reply.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::uiCallFailed(
                QStringLiteral("AddNotification"),
                {{QStringLiteral("appId"), normalizedAppId}}));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("delivered"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    QVariantMap results;
    results.insert(QStringLiteral("delivered"), true);
    results.insert(QStringLiteral("notification_id"), reply.value());
    results.insert(QStringLiteral("app_id"), normalizedAppId);
    results.insert(QStringLiteral("id"), id.trimmed());

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeRemoveNotification(const QString &appId, const QString &id)
{
    const QString normalizedAppId = appId.trimmed().isEmpty() ? QStringLiteral("unknown.desktop")
                                                               : appId.trimmed();
    bool okId = false;
    const uint numericId = id.trimmed().toUInt(&okId);
    if (!okId) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("RemoveNotification"),
                {{QStringLiteral("reason"), QStringLiteral("id-not-numeric")},
                 {QStringLiteral("appId"), normalizedAppId},
                 {QStringLiteral("id"), id}}));
    }

    QDBusInterface iface(QString::fromLatin1(kNotificationsService),
                         QString::fromLatin1(kNotificationsPath),
                         QString::fromLatin1(kNotificationsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("RemoveNotification")));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("removed"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    QDBusReply<void> reply = iface.call(QStringLiteral("CloseNotification"), numericId);
    if (!reply.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::uiCallFailed(
                QStringLiteral("RemoveNotification"),
                {{QStringLiteral("appId"), normalizedAppId},
                 {QStringLiteral("id"), id.trimmed()}}));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("removed"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    QVariantMap results;
    results.insert(QStringLiteral("removed"), true);
    results.insert(QStringLiteral("id"), id.trimmed());
    results.insert(QStringLiteral("app_id"), normalizedAppId);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeInhibit(const QString &handle,
                                             const QString &appId,
                                             const QString &window,
                                             uint flags,
                                             const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalInhibit);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("Inhibit")));
        }
    }
    QVariantMap ctx = enrichCallContext(handle, appId, window, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Inhibit"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Inhibit"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }

    QDBusInterface iface(QString::fromLatin1(kSessionStateService),
                         QString::fromLatin1(kSessionStatePath),
                         QString::fromLatin1(kSessionStateIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("Inhibit")));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("inhibit_cookie"), 0u);
        results.insert(QStringLiteral("inhibited"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    const QString reason = options.value(QStringLiteral("reason")).toString().trimmed().isEmpty()
                               ? QStringLiteral("portal-inhibit")
                               : options.value(QStringLiteral("reason")).toString().trimmed();
    QDBusReply<uint> reply = iface.call(QStringLiteral("Inhibit"), reason, flags);
    if (!reply.isValid()) {
        QVariantMap out = normalizePortalLikeResult(
            SlmPortalResponseBuilder::uiCallFailed(QStringLiteral("Inhibit")));
        QVariantMap results = unmarshalVariantMap(out.value(QStringLiteral("results")));
        results.insert(QStringLiteral("inhibit_cookie"), 0u);
        results.insert(QStringLiteral("inhibited"), false);
        out.insert(QStringLiteral("results"), results);
        return out;
    }

    QVariantMap results;
    results.insert(QStringLiteral("inhibit_cookie"), reply.value());
    results.insert(QStringLiteral("inhibited"), reply.value() != 0u);
    results.insert(QStringLiteral("flags"), flags);
    results.insert(QStringLiteral("reason"), reason);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeOpenWithQueryHandlers(const QString &handle,
                                                           const QString &appId,
                                                           const QString &parentWindow,
                                                           const QString &path,
                                                           const QVariantMap &options)
{
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.QueryHandlers"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.QueryHandlers"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }

    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.QueryHandlers"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-path")},
                 {QStringLiteral("path"), normalizedPath}}));
    }

    const int limit = qBound(1, options.value(QStringLiteral("limit"), 24).toInt(), 256);
    const QVariantList handlers = queryOpenWithHandlers(normalizedPath, limit);
    QString defaultHandler;
    for (const QVariant &v : handlers) {
        const QVariantMap row = unmarshalVariantMap(v);
        if (row.value(QStringLiteral("default")).toBool()) {
            defaultHandler = row.value(QStringLiteral("id")).toString();
            break;
        }
    }

    QVariantMap results;
    results.insert(QStringLiteral("path"), normalizedPath);
    results.insert(QStringLiteral("handlers"), handlers);
    results.insert(QStringLiteral("default_handler"), defaultHandler);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeOpenWithOpenFile(const QString &handle,
                                                      const QString &appId,
                                                      const QString &parentWindow,
                                                      const QString &path,
                                                      const QString &handlerId,
                                                      const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalOpenWith);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("OpenWith.OpenFileWith")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenFileWith"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenFileWith"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    const QString normalizedPath = path.trimmed();
    const QString handler = handlerId.trimmed();
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath) || handler.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenFileWith"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-input")},
                 {QStringLiteral("path"), normalizedPath},
                 {QStringLiteral("handler"), handler}}));
    }

    const bool dryRun = options.value(QStringLiteral("dryRun")).toBool();
    const bool launched = dryRun ? false : launchWithDesktopApp(handler, normalizedPath, false);
    if (!dryRun && !launched) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(
                QStringLiteral("OpenWith.OpenFileWith"),
                QString::fromLatin1(SlmPortalError::kLaunchFailed),
                {{QStringLiteral("path"), normalizedPath},
                 {QStringLiteral("handler"), handler}}));
    }

    QVariantMap results;
    results.insert(QStringLiteral("path"), normalizedPath);
    results.insert(QStringLiteral("handler"), handler);
    results.insert(QStringLiteral("launched"), launched);
    results.insert(QStringLiteral("dryRun"), dryRun);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeOpenWithOpenUri(const QString &handle,
                                                     const QString &appId,
                                                     const QString &parentWindow,
                                                     const QString &uri,
                                                     const QString &handlerId,
                                                     const QVariantMap &options)
{
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.check(message(), Capability::PortalOpenWith);
        if (!decision.isAllowed()) {
            return normalizePortalLikeResult(
                SlmPortalResponseBuilder::permissionDenied(QStringLiteral("OpenWith.OpenUriWith")));
        }
    }
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenUriWith"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenUriWith"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    const QString normalizedUri = uri.trimmed();
    const QString handler = handlerId.trimmed();
    if (normalizedUri.isEmpty() || !QUrl(normalizedUri).isValid() || handler.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("OpenWith.OpenUriWith"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-input")},
                 {QStringLiteral("uri"), normalizedUri},
                 {QStringLiteral("handler"), handler}}));
    }

    const bool dryRun = options.value(QStringLiteral("dryRun")).toBool();
    const bool launched = dryRun ? false : launchWithDesktopApp(handler, normalizedUri, true);
    if (!dryRun && !launched) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(
                QStringLiteral("OpenWith.OpenUriWith"),
                QString::fromLatin1(SlmPortalError::kLaunchFailed),
                {{QStringLiteral("uri"), normalizedUri},
                 {QStringLiteral("handler"), handler}}));
    }

    QVariantMap results;
    results.insert(QStringLiteral("uri"), normalizedUri);
    results.insert(QStringLiteral("handler"), handler);
    results.insert(QStringLiteral("launched"), launched);
    results.insert(QStringLiteral("dryRun"), dryRun);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeDocumentsAdd(const QString &handle,
                                                  const QString &appId,
                                                  const QString &parentWindow,
                                                  const QStringList &uris,
                                                  const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Add"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    if (!ctx.value(QStringLiteral("parentWindowValid"), true).toBool()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Add"),
                {{QStringLiteral("reason"), QStringLiteral("invalid-parent-window")}}));
    }
    if (uris.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Add"),
                {{QStringLiteral("reason"), QStringLiteral("empty-uris")}}));
    }

    const QString appKey = normalizeAppKey(ctx.value(QStringLiteral("appId")).toString());
    QVariantList docs;
    for (const QString &uri : uris) {
        const QString normalizedUri = uri.trimmed();
        if (normalizedUri.isEmpty()) {
            continue;
        }
        const QString token = makeDocumentToken(appKey);
        DocumentTokenEntry entry;
        entry.token = token;
        entry.appId = appKey;
        entry.uri = normalizedUri;
        entry.createdAtMs = QDateTime::currentMSecsSinceEpoch();
        documentTokenStore().insert(token, entry);
        docs.push_back(QVariantMap{
            {QStringLiteral("token"), token},
            {QStringLiteral("uri"), normalizedUri},
            {QStringLiteral("app_id"), appKey},
            {QStringLiteral("created_at"), entry.createdAtMs},
        });
    }

    return normalizePortalLikeResult({
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), QVariantMap{
             {QStringLiteral("documents"), docs},
             {QStringLiteral("count"), docs.size()},
        }},
    });
}

QVariantMap ImplPortalService::BridgeDocumentsResolve(const QString &handle,
                                                      const QString &appId,
                                                      const QString &parentWindow,
                                                      const QString &token,
                                                      const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Resolve"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    const QString tokenKey = token.trimmed();
    if (tokenKey.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Resolve"),
                {{QStringLiteral("reason"), QStringLiteral("empty-token")}}));
    }

    const auto it = documentTokenStore().constFind(tokenKey);
    if (it == documentTokenStore().constEnd()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(QStringLiteral("Documents.Resolve"),
                                                QStringLiteral("not-found")));
    }
    const QString appKey = normalizeAppKey(ctx.value(QStringLiteral("appId")).toString());
    if (it->appId != appKey) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(QStringLiteral("Documents.Resolve"),
                                                QStringLiteral("permission-denied")));
    }

    QVariantMap results;
    results.insert(QStringLiteral("token"), it->token);
    results.insert(QStringLiteral("uri"), it->uri);
    results.insert(QStringLiteral("app_id"), it->appId);
    results.insert(QStringLiteral("created_at"), it->createdAtMs);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeDocumentsList(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.List"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }

    const QString appKey = normalizeAppKey(ctx.value(QStringLiteral("appId")).toString());
    QVariantList docs;
    for (auto it = documentTokenStore().constBegin(); it != documentTokenStore().constEnd(); ++it) {
        if (it->appId != appKey) {
            continue;
        }
        docs.push_back(QVariantMap{
            {QStringLiteral("token"), it->token},
            {QStringLiteral("uri"), it->uri},
            {QStringLiteral("app_id"), it->appId},
            {QStringLiteral("created_at"), it->createdAtMs},
        });
    }
    return normalizePortalLikeResult({
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), QVariantMap{
             {QStringLiteral("documents"), docs},
             {QStringLiteral("count"), docs.size()},
        }},
    });
}

QVariantMap ImplPortalService::BridgeDocumentsRemove(const QString &handle,
                                                     const QString &appId,
                                                     const QString &parentWindow,
                                                     const QString &token,
                                                     const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Remove"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    const QString tokenKey = token.trimmed();
    if (tokenKey.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Documents.Remove"),
                {{QStringLiteral("reason"), QStringLiteral("empty-token")}}));
    }
    const auto it = documentTokenStore().constFind(tokenKey);
    if (it == documentTokenStore().constEnd()) {
    QVariantMap results;
    results.insert(QStringLiteral("removed"), false);
    results.insert(QStringLiteral("token"), tokenKey);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
    }
    const QString appKey = normalizeAppKey(ctx.value(QStringLiteral("appId")).toString());
    if (it->appId != appKey) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::withError(QStringLiteral("Documents.Remove"),
                                                QStringLiteral("permission-denied")));
    }
    documentTokenStore().remove(tokenKey);
    QVariantMap results;
    results.insert(QStringLiteral("removed"), true);
    results.insert(QStringLiteral("token"), tokenKey);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeTrashFile(const QString &handle,
                                               const QString &appId,
                                               const QString &parentWindow,
                                               const QString &path,
                                               const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Trash.TrashFile"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }
    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Trash.TrashFile"),
                {{QStringLiteral("reason"), QStringLiteral("empty-path")}}));
    }

    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("Trash.TrashFile")));
    }
    const QString uri = QUrl::fromLocalFile(normalizedPath).toString();
    QDBusReply<QString> reply = iface.call(QStringLiteral("Trash"), QStringList{uri});
    if (!reply.isValid()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::uiCallFailed(QStringLiteral("Trash.TrashFile")));
    }
    QVariantMap results;
    results.insert(QStringLiteral("job_id"), reply.value());
    results.insert(QStringLiteral("path"), normalizedPath);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::BridgeTrashEmpty(const QString &handle,
                                                const QString &appId,
                                                const QString &parentWindow,
                                                const QVariantMap &options)
{
    Q_UNUSED(options)
    const QVariantMap ctx = enrichCallContext(handle, appId, parentWindow, options);
    if (ctx.value(QStringLiteral("requestHandle")).toString().trimmed().isEmpty()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::invalidArgument(
                QStringLiteral("Trash.EmptyTrash"),
                {{QStringLiteral("reason"), QStringLiteral("missing-handle")}}));
    }

    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("Trash.EmptyTrash")));
    }
    QDBusReply<QString> reply = iface.call(QStringLiteral("EmptyTrash"));
    if (!reply.isValid()) {
        return normalizePortalLikeResult(
            SlmPortalResponseBuilder::uiCallFailed(QStringLiteral("Trash.EmptyTrash")));
    }
    QVariantMap results;
    results.insert(QStringLiteral("job_id"), reply.value());
    results.insert(QStringLiteral("emptying"), true);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("response"), 0u);
    out.insert(QStringLiteral("results"), results);
    return normalizePortalLikeResult(out);
}

QVariantMap ImplPortalService::enrichCallContext(const QString &handle,
                                                 const QString &appId,
                                                 const QString &parentWindow,
                                                 const QVariantMap &options) const
{
    const Slm::PortalAdapter::ResolvedPortalAppContext resolved =
        Slm::PortalAdapter::PortalAppContextResolver::resolve(
            calledFromDBus() ? message() : QDBusMessage(),
            appId,
            parentWindow);

    QVariantMap out = options;
    out.insert(QStringLiteral("requestHandle"), handle);
    out.insert(QStringLiteral("appId"), resolved.appId);
    out.insert(QStringLiteral("desktopFileId"), resolved.desktopFileId);
    out.insert(QStringLiteral("callerBusName"), resolved.busName);
    out.insert(QStringLiteral("callerExecutablePath"), resolved.executablePath);
    out.insert(QStringLiteral("appIdFromCaller"), resolved.appIdFromCaller);
    out.insert(QStringLiteral("parentWindow"), resolved.parentWindowRaw);
    out.insert(QStringLiteral("parentWindowType"), resolved.parentWindowType);
    out.insert(QStringLiteral("parentWindowId"), resolved.parentWindowId);
    out.insert(QStringLiteral("parentWindowValid"), resolved.parentWindowValid);
    if (!resolved.parentWindowValid) {
        out.insert(QStringLiteral("invalidParentWindow"), true);
    }
    out.insert(QStringLiteral("implPortal"), true);
    out.insert(QStringLiteral("implApiVersion"), apiVersion());
    return out;
}

bool ImplPortalService::canAttemptInputCapturePreempt(const QVariantMap &ctx,
                                                      const QVariantMap &options) const
{
    const bool requested = options.value(QStringLiteral("allow_preempt")).toBool()
        || options.value(QStringLiteral("allowPreempt")).toBool();
    if (!requested) {
        return false;
    }

    const bool gesture = ctx.value(QStringLiteral("initiatedByUserGesture")).toBool()
        || options.value(QStringLiteral("initiatedByUserGesture")).toBool();
    if (!gesture) {
        return false;
    }

    const bool mediated = ctx.value(QStringLiteral("initiatedFromOfficialUI")).toBool()
        || options.value(QStringLiteral("initiatedFromOfficialUI")).toBool()
        || options.value(QStringLiteral("mediatedByTrustedUi")).toBool();
    if (!mediated) {
        return false;
    }

    if (!calledFromDBus()) {
        return true;
    }

    using namespace Slm::Permissions;
    CallerIdentity caller = resolveCallerIdentityFromDbus(message());
    caller = m_resolver.resolveTrust(caller);
    return caller.isOfficialComponent
        || caller.trustLevel == TrustLevel::CoreDesktopComponent
        || caller.trustLevel == TrustLevel::PrivilegedDesktopService;
}

void ImplPortalService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties |
                                           QDBusConnection::ExportAdaptors);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

// ── Print portal bridge ───────────────────────────────────────────────────────

QVariantMap ImplPortalService::BridgePrintPreparePrint(const QString &handle,
                                                        const QString &appId,
                                                        const QString &parentWindow,
                                                        const QString &title,
                                                        const QVariantMap &settings,
                                                        const QVariantMap &pageSetup,
                                                        const QVariantMap &options)
{
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    Q_UNUSED(title)
    Q_UNUSED(options)

    // Store the settings keyed by handle for retrieval by Print.
    // Merge page-setup into the settings map for convenience.
    QVariantMap stored = settings;
    if (!pageSetup.isEmpty()) {
        stored.insert(QStringLiteral("_page_setup"), pageSetup);
    }
    m_printSettings.insert(handle, stored);

    // Return response=0 (success) with the settings and a token equal to
    // the request handle so the caller can correlate with the Print call.
    QVariantMap results;
    results.insert(QStringLiteral("settings"),   settings);
    results.insert(QStringLiteral("page-setup"), pageSetup);
    results.insert(QStringLiteral("token"),      handle);

    QVariantMap response;
    response.insert(QStringLiteral("response"), 0u);
    response.insert(QStringLiteral("results"),  results);
    return response;
}

// Maps a GTK print settings value from the portal to an lp(1) argument list.
static QStringList lpArgsFromPortalSettings(const QVariantMap &settings)
{
    QStringList args;

    const QString printer = settings.value(QStringLiteral("printer")).toString().trimmed();
    if (!printer.isEmpty()) {
        args << QStringLiteral("-d") << printer;
    }

    const int copies = settings.value(QStringLiteral("n-copies"), 1).toInt();
    if (copies > 1) {
        args << QStringLiteral("-n") << QString::number(qMax(1, copies));
    }

    const auto pushOption = [&](const QString &key, const QString &value) {
        if (value.trimmed().isEmpty()) return;
        args << QStringLiteral("-o") << QStringLiteral("%1=%2").arg(key, value.trimmed());
    };

    // Paper size (GTK uses e.g. "iso-a4", "na-letter").
    const QString paperFormat = settings.value(QStringLiteral("paper-format")).toString();
    if (!paperFormat.isEmpty()) {
        pushOption(QStringLiteral("media"), paperFormat);
    }

    // Color mode.
    const bool useColor = settings.value(QStringLiteral("use-color"), true).toBool();
    pushOption(QStringLiteral("print-color-mode"),
               useColor ? QStringLiteral("color") : QStringLiteral("monochrome"));

    // Duplex: GTK uses "simplex"/"horizontal"/"vertical"
    const QString duplex = settings.value(QStringLiteral("duplex")).toString();
    if (duplex == QLatin1String("horizontal")) {
        pushOption(QStringLiteral("sides"), QStringLiteral("two-sided-long-edge"));
    } else if (duplex == QLatin1String("vertical")) {
        pushOption(QStringLiteral("sides"), QStringLiteral("two-sided-short-edge"));
    } else {
        pushOption(QStringLiteral("sides"), QStringLiteral("one-sided"));
    }

    // Resolution.
    const int resolution = settings.value(QStringLiteral("resolution")).toInt();
    if (resolution > 0) {
        pushOption(QStringLiteral("printer-resolution"),
                   QStringLiteral("%1dpi").arg(resolution));
    }

    // Page ranges (portal format: array of structs {start, end} — stored as
    // a variant list; fall back to "print-pages" string if present).
    const QString printPages = settings.value(QStringLiteral("print-pages")).toString();
    if (printPages == QLatin1String("ranges")) {
        // page-ranges is a QVariantList of QVariantMaps with "start"/"end" keys.
        const QVariantList ranges = settings.value(QStringLiteral("page-ranges")).toList();
        if (!ranges.isEmpty()) {
            QStringList parts;
            for (const QVariant &r : ranges) {
                const QVariantMap m = r.toMap();
                const int s = m.value(QStringLiteral("start"), 1).toInt();
                const int e = m.value(QStringLiteral("end"), s).toInt();
                parts << (s == e ? QString::number(s)
                                 : QStringLiteral("%1-%2").arg(s).arg(e));
            }
            if (!parts.isEmpty()) {
                pushOption(QStringLiteral("page-ranges"), parts.join(QLatin1Char(',')));
            }
        }
    }

    return args;
}

QVariantMap ImplPortalService::BridgePrintPrint(const QString &handle,
                                                 const QString &appId,
                                                 const QString &parentWindow,
                                                 const QString &title,
                                                 const QDBusUnixFileDescriptor &fd,
                                                 const QVariantMap &options)
{
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    Q_UNUSED(title)

    // Retrieve stored settings. Prefer the handle from options["token"] to
    // support cases where the Print call uses a different handle than PreparePrint.
    const QString token = options.value(QStringLiteral("token"), handle).toString();
    const QVariantMap settings = m_printSettings.value(token.isEmpty() ? handle : token);
    m_printSettings.remove(token);
    m_printSettings.remove(handle);

    // Copy the document from the Unix FD to a temp file so lp can read it.
    if (!fd.isValid()) {
        return {{QStringLiteral("response"), 2u},
                {QStringLiteral("error"),    QStringLiteral("invalid-fd")}};
    }

    const QString tmpPath = QStringLiteral("/tmp/slm-print-%1.pdf")
                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QFile src;
        src.open(fd.fileDescriptor(), QIODevice::ReadOnly, QFile::DontCloseHandle);
        QFile dst(tmpPath);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return {{QStringLiteral("response"), 2u},
                    {QStringLiteral("error"),    QStringLiteral("cannot-write-temp-file")}};
        }
        while (!src.atEnd()) {
            const QByteArray chunk = src.read(65536);
            if (chunk.isEmpty()) break;
            dst.write(chunk);
        }
    }

    // Build lp argument list from portal print settings.
    QStringList lpArgs = lpArgsFromPortalSettings(settings);
    lpArgs << tmpPath;

    QProcess lp;
    lp.setProgram(QStringLiteral("lp"));
    lp.setArguments(lpArgs);
    lp.setProcessChannelMode(QProcess::MergedChannels);
    lp.start();

    const bool finished = lp.waitForFinished(15000);
    const QString output = QString::fromUtf8(lp.readAllStandardOutput()).trimmed();

    // Clean up the temp file regardless of outcome.
    QFile::remove(tmpPath);

    if (!finished || lp.exitCode() != 0) {
        const QString err = output.isEmpty()
                            ? QStringLiteral("lp failed (exit %1)").arg(lp.exitCode())
                            : output;
        return {{QStringLiteral("response"), 2u},
                {QStringLiteral("error"),    err}};
    }

    return {{QStringLiteral("response"), 0u},
            {QStringLiteral("results"),  QVariantMap{}}};
}
