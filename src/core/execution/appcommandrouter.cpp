#include "appcommandrouter.h"

#include "appexecutiongate.h"
#include "../../../screenshotmanager.h"
#include "../prefs/uipreferences.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

AppCommandRouter::AppCommandRouter(AppExecutionGate *gate,
                                   UIPreferences *uiPreferences,
                                   ScreenshotManager *screenshotManager,
                                   QObject *parent)
    : QObject(parent)
    , m_gate(gate)
    , m_uiPreferences(uiPreferences)
    , m_screenshotManager(screenshotManager)
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
    QVariantMap result;
    const QString op = action.trimmed().toLower();
    result.insert(QStringLiteral("action"), op);
    result.insert(QStringLiteral("source"), source);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), QString());
    const bool knownAction = isSupportedAction(op);
    if (!knownAction) {
        if (verboseLoggingEnabled()) {
            qWarning().noquote() << "AppCommandRouter.route: unknown action=" << op
                                 << "source=" << source;
        }
        recordEvent(op, source, false, QStringLiteral("unknown-action"));
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("unknown-action"));
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
        recordEvent(op, source, false, detail);
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), detail);
        emit routedDetailed(result);
        return result;
    }

    if (op.startsWith(QStringLiteral("screenshot.")) && m_screenshotManager == nullptr) {
        recordEvent(op, source, false, QStringLiteral("screenshot-manager-unavailable"));
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("screenshot-manager-unavailable"));
        emit routedDetailed(result);
        return result;
    }

    if (!m_gate &&
        op != QStringLiteral("screenshot.fullscreen") &&
        op != QStringLiteral("screenshot.window") &&
        op != QStringLiteral("screenshot.area")) {
        recordEvent(op, source, false, QStringLiteral("gate-unavailable"));
        emit routed(op, source, false);
        result.insert(QStringLiteral("error"), QStringLiteral("gate-unavailable"));
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
    recordEvent(op, source, ok, detail);
    emit routed(op, source, ok);
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("error"), detail);
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
    return m_uiPreferences ? m_uiPreferences->verboseLogging() : false;
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
        row.insert(QStringLiteral("total"), total);
        row.insert(QStringLiteral("success"), success);
        row.insert(QStringLiteral("failure"), failure);
        stats.insert(e.action, row);
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
                                   const QString &detail)
{
    RouterEvent e;
    e.action = action;
    e.source = source;
    e.success = success;
    e.detail = detail;
    e.epochMs = QDateTime::currentMSecsSinceEpoch();
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
