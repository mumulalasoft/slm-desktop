#include "appcommandrouter.h"

#include "appexecutiongate.h"
#include "../workspace/workspacemanager.h"
#include "../workspace/windowingbackendmanager.h"
#include "../../../screenshotmanager.h"

#include <QDebug>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

namespace {
bool parseGlobalMenuMeta(const QVariantMap &payload, const QString &source, int &menuIdOut, int &itemIdOut)
{
    menuIdOut = payload.value(QStringLiteral("__menuId"), -1).toInt();
    itemIdOut = payload.value(QStringLiteral("__itemId"), -1).toInt();
    if (menuIdOut >= 0 || itemIdOut >= 0) {
        return true;
    }

    if (!source.startsWith(QStringLiteral("global-menu"))) {
        return false;
    }

    QString suffix = source.mid(QStringLiteral("global-menu").size());
    if (suffix.startsWith(QLatin1Char(':'))) {
        suffix.remove(0, 1);
    }
    if (suffix.isEmpty()) {
        return false;
    }

    const QStringList parts = suffix.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (parts.size() >= 1) {
        menuIdOut = parts.at(0).toInt();
    }
    if (parts.size() >= 2) {
        itemIdOut = parts.at(1).toInt();
    }
    return menuIdOut >= 0 || itemIdOut >= 0;
}

QString globalMenuTelemetryCategory(int menuId, int itemId)
{
    if (menuId >= 0 && itemId >= 0) {
        return QStringLiteral("menu:%1:%2").arg(menuId).arg(itemId);
    }
    if (menuId >= 0) {
        return QStringLiteral("menu:%1").arg(menuId);
    }
    return QString();
}
} // namespace

AppCommandRouter::AppCommandRouter(AppExecutionGate *gate,
                                   ScreenshotManager *screenshotManager,
                                   QObject *desktopSettings,
                                   WorkspaceManager *workspaceManager,
                                   WindowingBackendManager *windowingBackend,
                                   QObject *parent)
    : QObject(parent)
    , m_gate(gate)
    , m_desktopSettings(desktopSettings)
    , m_screenshotManager(screenshotManager)
    , m_workspaceManager(workspaceManager)
    , m_windowingBackend(windowingBackend)
{
}

QStringList AppCommandRouter::supportedActions() const
{
    return {
        QStringLiteral("filemanager.open"),
        QStringLiteral("terminal.exec"),
        QStringLiteral("app.desktopid"),
        QStringLiteral("app.entry"),
        QStringLiteral("app.desktopentry"),
        QStringLiteral("workspace.presentview"),
        QStringLiteral("workspace.toggle"),
        QStringLiteral("workspace.split_left"),
        QStringLiteral("workspace.split_right"),
        QStringLiteral("workspace.pin_current"),
        QStringLiteral("storage.mount"),
        QStringLiteral("storage.unmount"),
        QStringLiteral("screenshot.fullscreen"),
        QStringLiteral("screenshot.window"),
        QStringLiteral("screenshot.area"),
    };
}

bool AppCommandRouter::isSupportedAction(const QString &action) const
{
    const QString op = action.trimmed().toLower();
    return supportedActions().contains(op);
}

bool AppCommandRouter::route(const QString &action, const QVariantMap &payload, const QString &source)
{
    return routeWithResult(action, payload, source).value(QStringLiteral("ok")).toBool();
}

QVariantMap AppCommandRouter::routeWithResult(const QString &action, const QVariantMap &payload,
                                              const QString &source)
{
    QElapsedTimer timer;
    timer.start();
    QVariantMap result;
    const QString op = action.trimmed().toLower();
    result.insert(QStringLiteral("action"), op);
    result.insert(QStringLiteral("source"), source);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), QString());
    int globalMenuId = -1;
    int globalMenuItemId = -1;
    if (parseGlobalMenuMeta(payload, source, globalMenuId, globalMenuItemId)) {
        if (globalMenuId >= 0) {
            result.insert(QStringLiteral("menuId"), globalMenuId);
        }
        if (globalMenuItemId >= 0) {
            result.insert(QStringLiteral("itemId"), globalMenuItemId);
        }
    }
    const bool knownAction = isSupportedAction(op);
    if (!knownAction) {
        if (verboseLoggingEnabled()) {
            qWarning().noquote() << "AppCommandRouter.route: unknown action=" << op
                                 << "source=" << source;
        }
        const qint64 durationMs = timer.elapsed();
        recordEvent(op, source, false, QStringLiteral("unknown-action"), durationMs, payload);
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("unknown-action"));
        result.insert(QStringLiteral("durationMs"), durationMs);
        emit routedDetailed(result);
        return result;
    }

    QString detail;
    if (op == QStringLiteral("filemanager.open")) {
        const QString target = payload.value(QStringLiteral("target")).toString().trimmed();
        if (target.isEmpty()) {
            detail = QStringLiteral("missing-target");
        }
    } else if (op == QStringLiteral("terminal.exec")) {
        const QString command = payload.value(QStringLiteral("command")).toString().trimmed();
        if (command.isEmpty()) {
            detail = QStringLiteral("missing-command");
        }
    } else if (op == QStringLiteral("app.desktopid")) {
        const QString desktopId = payload.value(QStringLiteral("desktopId")).toString().trimmed();
        if (desktopId.isEmpty()) {
            detail = QStringLiteral("missing-desktopid");
        }
    } else if (op == QStringLiteral("app.desktopentry")) {
        const QString desktopFile = payload.value(QStringLiteral("desktopFile")).toString().trimmed();
        const QString executable = payload.value(QStringLiteral("executable")).toString().trimmed();
        if (desktopFile.isEmpty() && executable.isEmpty()) {
            detail = QStringLiteral("missing-desktopfile-and-executable");
        }
    } else if (op == QStringLiteral("workspace.presentview")) {
        const QString viewId = payload.value(QStringLiteral("viewId")).toString().trimmed();
        if (viewId.isEmpty()) {
            detail = QStringLiteral("missing-viewid");
        }
    } else if (op == QStringLiteral("storage.mount") || op == QStringLiteral("storage.unmount")) {
        const QString devicePath = payload.value(QStringLiteral("devicePath")).toString().trimmed();
        if (devicePath.isEmpty()) {
            detail = QStringLiteral("missing-device-path");
        }
    } else if (op == QStringLiteral("screenshot.window")) {
        const int width = payload.value(QStringLiteral("width")).toInt();
        const int height = payload.value(QStringLiteral("height")).toInt();
        if (width <= 0 || height <= 0) {
            detail = QStringLiteral("missing-window-geometry");
        }
    } else if (op == QStringLiteral("screenshot.area")) {
        const bool hasW = payload.contains(QStringLiteral("width"));
        const bool hasH = payload.contains(QStringLiteral("height"));
        if (hasW || hasH) {
            const int width = payload.value(QStringLiteral("width")).toInt();
            const int height = payload.value(QStringLiteral("height")).toInt();
            if (width <= 0 || height <= 0) {
                detail = QStringLiteral("missing-area-geometry");
            }
        }
    }

    if (!detail.isEmpty()) {
        if (verboseLoggingEnabled()) {
            qInfo().noquote() << "AppCommandRouter.route:"
                              << "source=" << source
                              << "action=" << op
                              << "ok=" << false
                              << "detail=" << detail;
        }
        const qint64 durationMs = timer.elapsed();
        recordEvent(op, source, false, detail, durationMs, payload);
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), detail);
        result.insert(QStringLiteral("durationMs"), durationMs);
        emit routedDetailed(result);
        return result;
    }

    if (op.startsWith(QStringLiteral("screenshot.")) && m_screenshotManager == nullptr) {
        const qint64 durationMs = timer.elapsed();
        recordEvent(op, source, false, QStringLiteral("screenshot-manager-unavailable"), durationMs, payload);
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("screenshot-manager-unavailable"));
        result.insert(QStringLiteral("durationMs"), durationMs);
        emit routedDetailed(result);
        return result;
    }

    if (!m_gate &&
        op != QStringLiteral("screenshot.fullscreen") &&
        op != QStringLiteral("screenshot.window") &&
        op != QStringLiteral("screenshot.area") &&
        op != QStringLiteral("workspace.presentview") &&
        op != QStringLiteral("workspace.toggle") &&
        op != QStringLiteral("workspace.split_left") &&
        op != QStringLiteral("workspace.split_right") &&
        op != QStringLiteral("workspace.pin_current") &&
        op != QStringLiteral("storage.mount") &&
        op != QStringLiteral("storage.unmount")) {
        const qint64 durationMs = timer.elapsed();
        recordEvent(op, source, false, QStringLiteral("gate-unavailable"), durationMs, payload);
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("gate-unavailable"));
        result.insert(QStringLiteral("durationMs"), durationMs);
        emit routedDetailed(result);
        return result;
    }

    bool ok = false;
    if (op == QStringLiteral("filemanager.open")) {
        ok = m_gate->launchFromFileManager(payload.value(QStringLiteral("target")).toString().trimmed());
    } else if (op == QStringLiteral("terminal.exec")) {
        ok = m_gate->launchFromTerminal(payload.value(QStringLiteral("command")).toString().trimmed(),
                                        payload.value(QStringLiteral("cwd")).toString());
    } else if (op == QStringLiteral("app.desktopid")) {
        ok = m_gate->launchDesktopId(payload.value(QStringLiteral("desktopId")).toString().trimmed(), source);
    } else if (op == QStringLiteral("app.entry")) {
        ok = m_gate->launchEntryMap(payload, source);
    } else if (op == QStringLiteral("app.desktopentry")) {
        ok = m_gate->launchDesktopEntry(payload.value(QStringLiteral("desktopFile")).toString().trimmed(),
                                        payload.value(QStringLiteral("executable")).toString().trimmed(),
                                        payload.value(QStringLiteral("name")).toString(),
                                        payload.value(QStringLiteral("iconName")).toString(),
                                        payload.value(QStringLiteral("iconSource")).toString(),
                                        source);
    } else if (op == QStringLiteral("workspace.presentview")) {
        if (m_workspaceManager && m_workspaceManager->PresentView(
                    payload.value(QStringLiteral("viewId")).toString().trimmed())) {
            ok = true;
        } else {
            detail = QStringLiteral("workspace-manager-unavailable");
        }
    } else if (op == QStringLiteral("workspace.toggle")) {
        if (m_workspaceManager) {
            m_workspaceManager->ToggleWorkspace();
            ok = true;
        } else if (m_windowingBackend) {
            ok = m_windowingBackend->sendCommand(QStringLiteral("workspace toggle"))
                 || m_windowingBackend->sendCommand(QStringLiteral("overview toggle"));
            if (!ok) {
                detail = QStringLiteral("workspace-command-failed");
            }
        } else {
            detail = QStringLiteral("workspace-manager-unavailable");
        }
    } else if (op == QStringLiteral("workspace.split_left")
               || op == QStringLiteral("workspace.split_right")
               || op == QStringLiteral("workspace.pin_current")) {
        if (!m_windowingBackend) {
            detail = QStringLiteral("windowing-backend-unavailable");
        } else {
            QString command;
            if (op == QStringLiteral("workspace.split_left")) {
                command = QStringLiteral("workspace split-left");
            } else if (op == QStringLiteral("workspace.split_right")) {
                command = QStringLiteral("workspace split-right");
            } else {
                command = QStringLiteral("workspace pin-current");
            }
            ok = m_windowingBackend->sendCommand(command);
            if (!ok) {
                detail = QStringLiteral("workspace-command-failed");
            }
        }
    } else if (op == QStringLiteral("storage.mount")
               || op == QStringLiteral("storage.unmount")) {
        static const QString service = QStringLiteral("org.slm.Desktop.Storage");
        static const QString path = QStringLiteral("/org/slm/Desktop/Storage");
        static const QString ifaceName = QStringLiteral("org.slm.Desktop.Storage");
        QDBusInterface storageIface(service, path, ifaceName, QDBusConnection::sessionBus());
        if (!storageIface.isValid()) {
            detail = QStringLiteral("storage-service-unavailable");
        } else {
            const QString dev = payload.value(QStringLiteral("devicePath")).toString().trimmed();
            const QString method = (op == QStringLiteral("storage.mount"))
                                       ? QStringLiteral("Mount")
                                       : QStringLiteral("Eject");
            QDBusReply<QVariantMap> reply = storageIface.call(method, dev);
            if (!reply.isValid()) {
                detail = QStringLiteral("storage-dbus-error");
            } else {
                const QVariantMap map = reply.value();
                ok = map.value(QStringLiteral("ok")).toBool();
                if (!ok) {
                    detail = map.value(QStringLiteral("error")).toString().trimmed();
                    if (detail.isEmpty()) {
                        detail = QStringLiteral("storage-action-failed");
                    }
                }
                result.insert(QStringLiteral("payload"), map);
            }
        }
    } else if (op == QStringLiteral("screenshot.fullscreen")) {
        const QVariantMap shot = m_screenshotManager->captureFullscreen(
            payload.value(QStringLiteral("outputPath")).toString());
        ok = shot.value(QStringLiteral("ok")).toBool();
        if (!ok && detail.isEmpty()) {
            detail = shot.value(QStringLiteral("error")).toString();
        }
        result.insert(QStringLiteral("payload"), shot);
    } else if (op == QStringLiteral("screenshot.window")) {
        const QVariantMap shot = m_screenshotManager->captureWindow(
            payload.value(QStringLiteral("x")).toInt(),
            payload.value(QStringLiteral("y")).toInt(),
            payload.value(QStringLiteral("width")).toInt(),
            payload.value(QStringLiteral("height")).toInt(),
            payload.value(QStringLiteral("outputPath")).toString());
        ok = shot.value(QStringLiteral("ok")).toBool();
        if (!ok && detail.isEmpty()) {
            detail = shot.value(QStringLiteral("error")).toString();
        }
        result.insert(QStringLiteral("payload"), shot);
    } else if (op == QStringLiteral("screenshot.area")) {
        QVariantMap shot;
        const bool hasW = payload.contains(QStringLiteral("width"));
        const bool hasH = payload.contains(QStringLiteral("height"));
        if (hasW || hasH) {
            shot = m_screenshotManager->captureAreaRect(
                payload.value(QStringLiteral("x")).toInt(),
                payload.value(QStringLiteral("y")).toInt(),
                payload.value(QStringLiteral("width")).toInt(),
                payload.value(QStringLiteral("height")).toInt(),
                payload.value(QStringLiteral("outputPath")).toString());
        } else {
            shot = m_screenshotManager->captureArea(
                payload.value(QStringLiteral("outputPath")).toString());
        }
        ok = shot.value(QStringLiteral("ok")).toBool();
        if (!ok && detail.isEmpty()) {
            detail = shot.value(QStringLiteral("error")).toString();
        }
        result.insert(QStringLiteral("payload"), shot);
    }

    if (detail.isEmpty() && !ok) {
        detail = QStringLiteral("action-failed");
    }

    if (verboseLoggingEnabled()) {
        qInfo().noquote() << "AppCommandRouter.route:"
                          << "source=" << source
                          << "action=" << op
                          << "ok=" << ok
                          << "detail=" << detail;
    }
    const qint64 durationMs = timer.elapsed();
    recordEvent(op, source, ok, detail, durationMs, payload);
    emit routed(op, source, ok);
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("error"), detail);
    result.insert(QStringLiteral("durationMs"), durationMs);
    emit routedDetailed(result);
    return result;
}

bool AppCommandRouter::launchDesktopId(const QString &desktopId, const QString &source)
{
    return route(QStringLiteral("app.desktopId"),
                 QVariantMap{{QStringLiteral("desktopId"), desktopId}}, source);
}

bool AppCommandRouter::openFromFileManager(const QString &targetPathOrUrl)
{
    return route(QStringLiteral("filemanager.open"),
                 QVariantMap{{QStringLiteral("target"), targetPathOrUrl}},
                 QStringLiteral("filemanager"));
}

bool AppCommandRouter::execFromTerminal(const QString &command, const QString &workingDirectory)
{
    return route(QStringLiteral("terminal.exec"),
                 QVariantMap{{QStringLiteral("command"), command},
                             {QStringLiteral("cwd"), workingDirectory}},
                 QStringLiteral("terminal"));
}

bool AppCommandRouter::verboseLoggingEnabled() const
{
    if (m_desktopSettings) {
        QVariant v;
        const bool okInvoke = QMetaObject::invokeMethod(m_desktopSettings, "settingValue",
                                                         Q_RETURN_ARG(QVariant, v),
                                                         Q_ARG(QString, QStringLiteral("debug.verboseLogging")),
                                                         Q_ARG(QVariant, false));
        if (okInvoke) {
            return v.toBool();
        }
    }
    return false;
}

QVariantList AppCommandRouter::recentEvents() const
{
    QVariantList out;
    out.reserve(m_recentEvents.size());
    for (const RouterEvent &e : m_recentEvents) {
        QVariantMap row;
        row.insert(QStringLiteral("action"), e.action);
        row.insert(QStringLiteral("source"), e.source);
        row.insert(QStringLiteral("success"), e.success);
        row.insert(QStringLiteral("detail"), e.detail);
        row.insert(QStringLiteral("epochMs"), e.epochMs);
        row.insert(QStringLiteral("durationMs"), e.durationMs);
        if (e.menuId >= 0) {
            row.insert(QStringLiteral("menuId"), e.menuId);
        }
        if (e.itemId >= 0) {
            row.insert(QStringLiteral("itemId"), e.itemId);
        }
        if (!e.telemetryCategory.isEmpty()) {
            row.insert(QStringLiteral("telemetryCategory"), e.telemetryCategory);
        }
        row.insert(QStringLiteral("isoTime"),
                   QDateTime::fromMSecsSinceEpoch(e.epochMs).toString(Qt::ISODateWithMs));
        out.push_back(row);
    }
    return out;
}

QVariantList AppCommandRouter::recentFailures(int maxItems) const
{
    QVariantList out;
    if (maxItems <= 0) {
        return out;
    }
    for (int i = m_recentEvents.size() - 1; i >= 0; --i) {
        const RouterEvent &e = m_recentEvents.at(i);
        if (e.success) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("action"), e.action);
        row.insert(QStringLiteral("source"), e.source);
        row.insert(QStringLiteral("success"), e.success);
        row.insert(QStringLiteral("detail"), e.detail);
        row.insert(QStringLiteral("epochMs"), e.epochMs);
        row.insert(QStringLiteral("durationMs"), e.durationMs);
        if (e.menuId >= 0) {
            row.insert(QStringLiteral("menuId"), e.menuId);
        }
        if (e.itemId >= 0) {
            row.insert(QStringLiteral("itemId"), e.itemId);
        }
        if (!e.telemetryCategory.isEmpty()) {
            row.insert(QStringLiteral("telemetryCategory"), e.telemetryCategory);
        }
        row.insert(QStringLiteral("isoTime"),
                   QDateTime::fromMSecsSinceEpoch(e.epochMs).toString(Qt::ISODateWithMs));
        out.push_back(row);
        if (out.size() >= maxItems) {
            break;
        }
    }
    return out;
}

QVariantMap AppCommandRouter::actionStats() const
{
    QVariantMap stats;
    for (const RouterEvent &e : m_recentEvents) {
        QVariantMap row = stats.value(e.action).toMap();
        const int total = row.value(QStringLiteral("total")).toInt() + 1;
        const int success = row.value(QStringLiteral("success")).toInt() + (e.success ? 1 : 0);
        const int failure = row.value(QStringLiteral("failure")).toInt() + (e.success ? 0 : 1);
        const qint64 durationTotalMs = row.value(QStringLiteral("durationTotalMs")).toLongLong()
                                       + e.durationMs;
        row.insert(QStringLiteral("total"), total);
        row.insert(QStringLiteral("success"), success);
        row.insert(QStringLiteral("failure"), failure);
        row.insert(QStringLiteral("durationTotalMs"), durationTotalMs);
        row.insert(QStringLiteral("durationAvgMs"),
                   total > 0 ? (durationTotalMs / total) : 0);
        stats.insert(e.action, row);
    }
    return stats;
}

QVariantMap AppCommandRouter::globalMenuStats() const
{
    QVariantMap stats;
    for (const RouterEvent &e : m_recentEvents) {
        if (e.telemetryCategory.isEmpty()) {
            continue;
        }
        QVariantMap row = stats.value(e.telemetryCategory).toMap();
        const int total = row.value(QStringLiteral("total")).toInt() + 1;
        const int success = row.value(QStringLiteral("success")).toInt() + (e.success ? 1 : 0);
        const int failure = row.value(QStringLiteral("failure")).toInt() + (e.success ? 0 : 1);
        const qint64 durationTotalMs = row.value(QStringLiteral("durationTotalMs")).toLongLong()
                                       + e.durationMs;
        row.insert(QStringLiteral("menuId"), e.menuId);
        row.insert(QStringLiteral("itemId"), e.itemId);
        row.insert(QStringLiteral("total"), total);
        row.insert(QStringLiteral("success"), success);
        row.insert(QStringLiteral("failure"), failure);
        row.insert(QStringLiteral("durationTotalMs"), durationTotalMs);
        row.insert(QStringLiteral("durationAvgMs"), total > 0 ? (durationTotalMs / total) : 0);
        row.insert(QStringLiteral("lastAction"), e.action);
        row.insert(QStringLiteral("lastSource"), e.source);
        row.insert(QStringLiteral("lastDetail"), e.detail);
        row.insert(QStringLiteral("lastSuccess"), e.success);
        stats.insert(e.telemetryCategory, row);
    }
    return stats;
}

QVariantMap AppCommandRouter::diagnosticSnapshot() const
{
    QVariantMap out;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    out.insert(QStringLiteral("snapshotEpochMs"), nowMs);
    out.insert(QStringLiteral("snapshotIsoTime"),
               QDateTime::fromMSecsSinceEpoch(nowMs).toString(Qt::ISODateWithMs));
    out.insert(QStringLiteral("supportedActions"), supportedActions());
    out.insert(QStringLiteral("eventCount"), eventCount());
    out.insert(QStringLiteral("failureCount"), failureCount());
    out.insert(QStringLiteral("lastError"), lastError());
    out.insert(QStringLiteral("lastEvent"), lastEvent());
    out.insert(QStringLiteral("actionStats"), actionStats());
    out.insert(QStringLiteral("globalMenuStats"), globalMenuStats());
    out.insert(QStringLiteral("recentFailures"), recentFailures(10));
    return out;
}

QString AppCommandRouter::diagnosticsJson(bool pretty) const
{
    const QJsonObject json = QJsonObject::fromVariantMap(diagnosticSnapshot());
    const auto format = pretty ? QJsonDocument::Indented : QJsonDocument::Compact;
    return QString::fromUtf8(QJsonDocument(json).toJson(format));
}

void AppCommandRouter::clearRecentEvents()
{
    m_recentEvents.clear();
    emit recentEventsChanged();
}

void AppCommandRouter::recordEvent(const QString &action, const QString &source, bool success,
                                   const QString &detail, qint64 durationMs, const QVariantMap &payload)
{
    RouterEvent e;
    e.action = action;
    e.source = source;
    e.success = success;
    e.detail = detail;
    e.epochMs = QDateTime::currentMSecsSinceEpoch();
    e.durationMs = qMax<qint64>(0, durationMs);
    int menuId = -1;
    int itemId = -1;
    parseGlobalMenuMeta(payload, source, menuId, itemId);
    e.menuId = menuId;
    e.itemId = itemId;
    e.telemetryCategory = globalMenuTelemetryCategory(menuId, itemId);
    m_recentEvents.push_back(e);
    static constexpr int kMaxRecentEvents = 120;
    if (m_recentEvents.size() > kMaxRecentEvents) {
        const int removeCount = m_recentEvents.size() - kMaxRecentEvents;
        m_recentEvents.erase(m_recentEvents.begin(), m_recentEvents.begin() + removeCount);
    }
    emit recentEventsChanged();
}

QVariantMap AppCommandRouter::lastEvent() const
{
    if (m_recentEvents.isEmpty()) {
        return {};
    }
    const RouterEvent &e = m_recentEvents.constLast();
    QVariantMap out;
    out.insert(QStringLiteral("action"), e.action);
    out.insert(QStringLiteral("source"), e.source);
    out.insert(QStringLiteral("success"), e.success);
    out.insert(QStringLiteral("detail"), e.detail);
    out.insert(QStringLiteral("epochMs"), e.epochMs);
    out.insert(QStringLiteral("durationMs"), e.durationMs);
    if (e.menuId >= 0) {
        out.insert(QStringLiteral("menuId"), e.menuId);
    }
    if (e.itemId >= 0) {
        out.insert(QStringLiteral("itemId"), e.itemId);
    }
    if (!e.telemetryCategory.isEmpty()) {
        out.insert(QStringLiteral("telemetryCategory"), e.telemetryCategory);
    }
    out.insert(QStringLiteral("isoTime"),
               QDateTime::fromMSecsSinceEpoch(e.epochMs).toString(Qt::ISODateWithMs));
    return out;
}

int AppCommandRouter::eventCount() const
{
    return m_recentEvents.size();
}

int AppCommandRouter::failureCount() const
{
    int failed = 0;
    for (const RouterEvent &e : m_recentEvents) {
        if (!e.success) {
            ++failed;
        }
    }
    return failed;
}

QString AppCommandRouter::lastError() const
{
    for (int i = m_recentEvents.size() - 1; i >= 0; --i) {
        const RouterEvent &e = m_recentEvents.at(i);
        if (!e.success) {
            return e.detail;
        }
    }
    return QString();
}
