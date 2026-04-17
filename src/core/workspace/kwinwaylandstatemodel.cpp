#include "kwinwaylandstatemodel.h"

#include "kwinsupportinfoparser.h"
#include "kwinwaylandipcclient.h"

#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDateTime>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
constexpr auto kKWinService = "org.kde.KWin";
constexpr auto kKWinPath = "/KWin";
constexpr auto kKWinIface = "org.kde.KWin";
constexpr auto kVdmPath = "/VirtualDesktopManager";
constexpr auto kVdmIface = "org.kde.KWin.VirtualDesktopManager";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
constexpr auto kWindowIfaceA = "org.kde.KWin.Window";
constexpr auto kWindowIfaceB = "org.kde.kwin.Window";

QVariantMap readWindowPropertyMap(const QString &path, const char *iface)
{
    QDBusMessage req = QDBusMessage::createMethodCall(QString::fromLatin1(kKWinService),
                                                      path,
                                                      QString::fromLatin1(kPropsIface),
                                                      QStringLiteral("GetAll"));
    req << QString::fromLatin1(iface);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(req, QDBus::Block, 220);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return qdbus_cast<QVariantMap>(reply.arguments().at(0));
}

static QString normalizeLine(const QString &line)
{
    QString s = line.trimmed();
    s.replace('\t', ' ');
    return s;
}

QString fromUtf8(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

QString normalizeToken(const QString &value)
{
    QString out = value.trimmed().toLower();
    out.remove(QRegularExpression(QStringLiteral("[^a-z0-9._-]")));
    return out;
}

void insertTokenVariants(QSet<QString> &tokens, const QString &raw)
{
    const QString normalized = normalizeToken(raw);
    if (normalized.isEmpty()) {
        return;
    }
    tokens.insert(normalized);
    const QStringList parts = normalized.split(QRegularExpression(QStringLiteral("[._-]+")),
                                               Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.size() >= 3) {
            tokens.insert(part);
        }
    }
}

QString iconNameFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return fromUtf8(names[0]);
        }
    }
    return QString();
}

QString iconSourceFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_FILE_ICON(icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        if (!file) {
            return QString();
        }
        gchar *path = g_file_get_path(file);
        const QString result = fromUtf8(path);
        g_free(path);
        if (!result.isEmpty() && QFileInfo::exists(result)) {
            return QUrl::fromLocalFile(result).toString();
        }
    }
    return QString();
}

QString canonicalAppId(QString raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    if (s.contains('/')) {
        s = QFileInfo(s).fileName();
    }
    if (s.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        s.chop(8);
    }
    return s.trimmed();
}

QVariant readProperty(const char *path, const char *iface, const char *property)
{
    QDBusMessage request = QDBusMessage::createMethodCall(QString::fromLatin1(kKWinService),
                                                          QString::fromLatin1(path),
                                                          QString::fromLatin1(kPropsIface),
                                                          QStringLiteral("Get"));
    request << QString::fromLatin1(iface) << QString::fromLatin1(property);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 180);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QDBusVariant wrapped = qvariant_cast<QDBusVariant>(reply.arguments().at(0));
    return wrapped.variant();
}

int readIntProperty(const char *path, const char *iface, const char *property)
{
    bool ok = false;
    const int value = readProperty(path, iface, property).toInt(&ok);
    return ok ? value : -1;
}

void applyActiveWorkspaceVisibility(QVector<QVariantMap> &windows, int activeSpace)
{
    const int currentSpace = qMax(1, activeSpace);
    for (QVariantMap &w : windows) {
        const int windowSpace = qMax(1, w.value(QStringLiteral("space"), currentSpace).toInt());
        const bool mapped = w.value(QStringLiteral("mapped"), true).toBool();
        // Workspace visibility contract:
        // keep full window metadata in the model (for focus/present logic),
        // but mark non-active-space windows as unmapped so shell rendering and
        // overlays can skip them without building per-space scene graphs.
        w.insert(QStringLiteral("mapped"), mapped && (windowSpace == currentSpace));
    }
}
}

KWinWaylandStateModel::KWinWaylandStateModel(QObject *parent)
    : QObject(parent)
    , m_spaceProbeTimer(new QTimer(this))
    , m_windowProbeTimer(new QTimer(this))
    , m_spaceRefreshDebounceTimer(new QTimer(this))
    , m_windowRefreshDebounceTimer(new QTimer(this))
    , m_profileLogTimer(new QTimer(this))
{
    m_spaceProbeTimer->setSingleShot(false);
    m_spaceProbeTimer->setInterval(m_spacePollIntervalMs);
    connect(m_spaceProbeTimer, &QTimer::timeout, this, &KWinWaylandStateModel::onSpacePollTick);

    m_windowProbeTimer->setSingleShot(false);
    m_windowProbeTimer->setInterval(m_windowPollIntervalMs);
    connect(m_windowProbeTimer, &QTimer::timeout, this, &KWinWaylandStateModel::onWindowPollTick);

    m_spaceRefreshDebounceTimer->setSingleShot(true);
    m_spaceRefreshDebounceTimer->setInterval(70);
    connect(m_spaceRefreshDebounceTimer, &QTimer::timeout, this, &KWinWaylandStateModel::refreshSpaces);

    m_windowRefreshDebounceTimer->setSingleShot(true);
    m_windowRefreshDebounceTimer->setInterval(90);
    connect(m_windowRefreshDebounceTimer, &QTimer::timeout, this, &KWinWaylandStateModel::refreshWindows);

    m_profileEnabled = qEnvironmentVariableIntValue("DS_KWIN_PROFILE") > 0;
    m_profileLogTimer->setSingleShot(false);
    m_profileLogTimer->setInterval(10000);
    connect(m_profileLogTimer, &QTimer::timeout, this, &KWinWaylandStateModel::maybeLogProfileSnapshot);
    if (m_profileEnabled) {
        m_profileLogTimer->start();
    }
}

bool KWinWaylandStateModel::connected() const
{
    return m_ipc != nullptr && m_ipc->connected();
}

QVariantMap KWinWaylandStateModel::lastEvent() const
{
    return m_lastEvent;
}

bool KWinWaylandStateModel::switcherActive() const
{
    return m_switcherActive;
}

int KWinWaylandStateModel::switcherIndex() const
{
    return m_switcherIndex;
}

int KWinWaylandStateModel::switcherCount() const
{
    return m_switcherCount;
}

int KWinWaylandStateModel::switcherSelectedSlot() const
{
    return m_switcherSelectedSlot;
}

QString KWinWaylandStateModel::switcherViewId() const
{
    return m_switcherViewId;
}

QString KWinWaylandStateModel::switcherTitle() const
{
    return m_switcherTitle;
}

QString KWinWaylandStateModel::switcherAppId() const
{
    return m_switcherAppId;
}

QVariantList KWinWaylandStateModel::switcherEntries() const
{
    return m_switcherEntries;
}

int KWinWaylandStateModel::activeSpace() const
{
    return m_activeSpace;
}

int KWinWaylandStateModel::spaceCount() const
{
    return m_spaceCount;
}

QVariantMap KWinWaylandStateModel::windowAt(int index) const
{
    if (index < 0 || index >= m_windows.size()) {
        return {};
    }
    return m_windows.at(index);
}

int KWinWaylandStateModel::windowCount() const
{
    return m_windows.size();
}

void KWinWaylandStateModel::clear()
{
    m_windows.clear();
    m_switcherActive = false;
    m_switcherIndex = 0;
    m_switcherCount = 0;
    m_switcherSelectedSlot = 0;
    m_switcherViewId.clear();
    m_switcherTitle.clear();
    m_switcherAppId.clear();
    m_switcherEntries.clear();
    emit switcherChanged();
}

void KWinWaylandStateModel::setIpcClient(KWinWaylandIpcClient *client)
{
    if (m_ipc == client) {
        return;
    }
    if (m_ipc != nullptr) {
        disconnect(m_ipc, nullptr, this, nullptr);
    }
    m_ipc = client;
    if (m_ipc != nullptr) {
        connect(m_ipc, &KWinWaylandIpcClient::connectedChanged,
                this, &KWinWaylandStateModel::onConnectedChanged);
        connect(m_ipc, &KWinWaylandIpcClient::eventReceived,
                this, &KWinWaylandStateModel::onEventReceived);
    }
    emit connectedChanged();
    onConnectedChanged();
}

void KWinWaylandStateModel::onConnectedChanged()
{
    if (connected()) {
        m_objectModelMissStreak = 0;
        m_lastSupportFallbackMs = 0;
        m_supportFallbackMinIntervalMs = m_supportFallbackBaseIntervalMs;
        bindDbusSignals();
        reconfigurePollingStrategy();
        if (!m_spaceProbeTimer->isActive()) {
            m_spaceProbeTimer->start();
        }
        if (!m_windowProbeTimer->isActive()) {
            m_windowProbeTimer->start();
        }
        scheduleSpaceRefresh(0);
        scheduleWindowRefresh(0);
    } else {
        unbindDbusSignals();
        reconfigurePollingStrategy();
        m_supportRequestInFlight = false;
        ++m_supportRequestSeq;
        m_supportAppliedSeq = m_supportRequestSeq;
        if (m_spaceProbeTimer->isActive()) {
            m_spaceProbeTimer->stop();
        }
        if (m_windowProbeTimer->isActive()) {
            m_windowProbeTimer->stop();
        }
        m_spaceRefreshDebounceTimer->stop();
        m_windowRefreshDebounceTimer->stop();
        if (!m_windows.isEmpty()) {
            m_windows.clear();
            publishSyntheticEvent(QStringLiteral("windows-cleared"));
        }
        if (m_activeSpace != 1 || m_spaceCount != 1) {
            m_activeSpace = 1;
            m_spaceCount = 1;
            emit spaceChanged();
        }
    }
    emit connectedChanged();
}

void KWinWaylandStateModel::onEventReceived(const QString &event, const QVariantMap &payload)
{
    QVariantMap normalizedPayload = payload;
    QString normalizedEvent = normalizedPayload.value(QStringLiteral("event")).toString().trimmed();
    if (normalizedEvent.isEmpty()) {
        normalizedEvent = event.trimmed();
    }
    if (!normalizedEvent.isEmpty()) {
        normalizedPayload.insert(QStringLiteral("event"), normalizedEvent.toLower());
    }
    m_lastEvent = normalizedPayload;
    emit lastEventChanged();

    if (normalizedPayload.value(QStringLiteral("event")).toString() == QStringLiteral("command")) {
        const QString command = normalizedPayload.value(QStringLiteral("command")).toString();
        if (command == QStringLiteral("switcher-next") || command == QStringLiteral("switcher-prev")) {
            scheduleWindowRefresh(10);
        }
    }
}

void KWinWaylandStateModel::refreshSpaces()
{
    ++m_refreshSpacesCount;
    if (!connected()) {
        return;
    }
    const int count = readDesktopCount();
    const int boundedCount = qMax(1, count);
    const int active = readActiveDesktop(boundedCount);
    const int boundedActive = qBound(1, active, boundedCount);
    if (boundedActive == m_activeSpace && boundedCount == m_spaceCount) {
        return;
    }
    m_activeSpace = boundedActive;
    m_spaceCount = boundedCount;
    emit spaceChanged();
    // Ensure workspace-visibility filtering is applied immediately after
    // active space changes so normal mode never renders stale workspace windows.
    scheduleWindowRefresh(0);
}

void KWinWaylandStateModel::refreshWindows()
{
    ++m_refreshWindowsCount;
    if (!connected()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<QVariantMap> next = readWindowsFromObjectModel();
    if (!next.isEmpty()) {
        ++m_objectModelHitCount;
        m_objectModelMissStreak = 0;
        // Recovery path: when object model is stable, gradually lower fallback throttle.
        if (m_supportFallbackMinIntervalMs > m_supportFallbackBaseIntervalMs) {
            m_supportFallbackMinIntervalMs = qMax(m_supportFallbackBaseIntervalMs,
                                                  m_supportFallbackMinIntervalMs - 1200);
        }
        setWindows(next);
        return;
    }

    ++m_objectModelMissStreak;
    // Degrade path: repeated misses increase fallback spacing to cap expensive probes.
    if (m_objectModelMissStreak >= 6) {
        m_supportFallbackMinIntervalMs = qMin(m_supportFallbackMaxIntervalMs,
                                              m_supportFallbackMinIntervalMs + 1200);
    }
    const bool allowSupportFallback =
        (m_objectModelMissStreak <= 2) ||
        (m_lastSupportFallbackMs <= 0) ||
        ((nowMs - m_lastSupportFallbackMs) >= m_supportFallbackMinIntervalMs);

    if (allowSupportFallback) {
        m_lastSupportFallbackMs = nowMs;
        requestSupportInformationAsync();
    } else {
        ++m_supportFallbackSkipCount;
    }

    // Avoid transient empty snapshots causing UI flicker when DBus object model misses briefly.
    if (!m_windows.isEmpty() && m_objectModelMissStreak < 6) {
        return;
    }
    setWindows(next);
}

void KWinWaylandStateModel::onSpacePollTick()
{
    ++m_pollTriggerSpaceCount;
    scheduleSpaceRefresh(0);
}

void KWinWaylandStateModel::onWindowPollTick()
{
    ++m_pollTriggerWindowCount;
    scheduleWindowRefresh(0);
}

void KWinWaylandStateModel::scheduleSpaceRefresh(int delayMs)
{
    if (delayMs <= 0) {
        if (m_spaceRefreshDebounceTimer->isActive()) {
            m_spaceRefreshDebounceTimer->stop();
        }
        m_spaceRefreshDebounceTimer->start(0);
        return;
    }
    if (!m_spaceRefreshDebounceTimer->isActive()) {
        m_spaceRefreshDebounceTimer->start(delayMs);
        return;
    }
    if (m_spaceRefreshDebounceTimer->remainingTime() > delayMs) {
        m_spaceRefreshDebounceTimer->start(delayMs);
    }
}

void KWinWaylandStateModel::scheduleWindowRefresh(int delayMs)
{
    if (delayMs <= 0) {
        if (m_windowRefreshDebounceTimer->isActive()) {
            m_windowRefreshDebounceTimer->stop();
        }
        m_windowRefreshDebounceTimer->start(0);
        return;
    }
    if (!m_windowRefreshDebounceTimer->isActive()) {
        m_windowRefreshDebounceTimer->start(delayMs);
        return;
    }
    if (m_windowRefreshDebounceTimer->remainingTime() > delayMs) {
        m_windowRefreshDebounceTimer->start(delayMs);
    }
}

void KWinWaylandStateModel::bindDbusSignals()
{
    if (m_dbusSignalsBound) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    const bool kwinProps = bus.connect(QString::fromLatin1(kKWinService),
                                       QString::fromLatin1(kKWinPath),
                                       QString::fromLatin1(kPropsIface),
                                       QStringLiteral("PropertiesChanged"),
                                       this,
                                       SLOT(onKWinPropertiesChanged(QString,QVariantMap,QStringList)));
    const bool vdmProps = bus.connect(QString::fromLatin1(kKWinService),
                                      QString::fromLatin1(kVdmPath),
                                      QString::fromLatin1(kPropsIface),
                                      QStringLiteral("PropertiesChanged"),
                                      this,
                                      SLOT(onVdmPropertiesChanged(QString,QVariantMap,QStringList)));
    m_dbusSignalsBound = kwinProps || vdmProps;
    reconfigurePollingStrategy();
}

void KWinWaylandStateModel::unbindDbusSignals()
{
    if (!m_dbusSignalsBound) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(QString::fromLatin1(kKWinService),
                   QString::fromLatin1(kKWinPath),
                   QString::fromLatin1(kPropsIface),
                   QStringLiteral("PropertiesChanged"),
                   this,
                   SLOT(onKWinPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.disconnect(QString::fromLatin1(kKWinService),
                   QString::fromLatin1(kVdmPath),
                   QString::fromLatin1(kPropsIface),
                   QStringLiteral("PropertiesChanged"),
                   this,
                   SLOT(onVdmPropertiesChanged(QString,QVariantMap,QStringList)));
    m_dbusSignalsBound = false;
    reconfigurePollingStrategy();
}

void KWinWaylandStateModel::reconfigurePollingStrategy()
{
    if (!connected()) {
        m_spacePollIntervalMs = 3200;
        m_windowPollIntervalMs = 3600;
    } else if (m_dbusSignalsBound) {
        // Event-driven path available: keep polling as slow safety net.
        m_spacePollIntervalMs = 9000;
        m_windowPollIntervalMs = 11000;
    } else {
        // No DBus signals; rely more on polling fallback.
        m_spacePollIntervalMs = 3200;
        m_windowPollIntervalMs = 3600;
    }

    m_spaceProbeTimer->setInterval(m_spacePollIntervalMs);
    m_windowProbeTimer->setInterval(m_windowPollIntervalMs);
}

int KWinWaylandStateModel::readDesktopCount() const
{
    int count = readIntProperty(kVdmPath, kVdmIface, "count");
    if (count > 0) {
        return count;
    }
    count = readIntProperty(kKWinPath, kKWinIface, "numberOfDesktops");
    if (count > 0) {
        return count;
    }

    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (kwin.isValid()) {
        QDBusReply<int> reply = kwin.call(QStringLiteral("numberOfDesktops"));
        if (reply.isValid() && reply.value() > 0) {
            return reply.value();
        }
    }
    return 1;
}

int KWinWaylandStateModel::readActiveDesktop(int desktopCount) const
{
    int active = readIntProperty(kKWinPath, kKWinIface, "currentDesktop");
    if (active > 0) {
        return active;
    }
    active = readIntProperty(kVdmPath, kVdmIface, "currentDesktop");
    if (active > 0) {
        return active;
    }

    const QString currentId = readProperty(kVdmPath, kVdmIface, "current").toString().trimmed();
    if (!currentId.isEmpty()) {
        static const QRegularExpression trailingDigits(QStringLiteral("(\\d+)$"));
        const QRegularExpressionMatch match = trailingDigits.match(currentId);
        if (match.hasMatch()) {
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok && value > 0) {
                return value;
            }
        }
    }

    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (kwin.isValid()) {
        QDBusReply<int> reply = kwin.call(QStringLiteral("currentDesktop"));
        if (reply.isValid() && reply.value() > 0) {
            return reply.value();
        }
    }
    Q_UNUSED(desktopCount)
    return 1;
}

QVariantMap KWinWaylandStateModel::readWindowFromObjectPath(const QString &path)
{
    QVariantMap props = readWindowPropertyMap(path, kWindowIfaceA);
    if (props.isEmpty()) {
        props = readWindowPropertyMap(path, kWindowIfaceB);
    }
    if (props.isEmpty()) {
        return {};
    }

    const auto readString = [&](const char *a, const char *b = nullptr) -> QString {
        QString out = props.value(QString::fromLatin1(a)).toString().trimmed();
        if (out.isEmpty() && b != nullptr) {
            out = props.value(QString::fromLatin1(b)).toString().trimmed();
        }
        return out;
    };
    const auto readBool = [&](const char *a, bool fallback) -> bool {
        if (props.contains(QString::fromLatin1(a))) {
            return props.value(QString::fromLatin1(a)).toBool();
        }
        return fallback;
    };

    QVariantList geom = parseGeometryList(props.value(QStringLiteral("frameGeometry")));
    if (geom.size() < 4) {
        geom = parseGeometryList(props.value(QStringLiteral("geometry")));
    }
    const int x = geom.value(0).toInt();
    const int y = geom.value(1).toInt();
    const int w = qMax(0, geom.value(2).toInt());
    const int h = qMax(0, geom.value(3).toInt());

    QString appId = readString("desktopFileName", "resourceClass");
    if (appId.isEmpty()) {
        appId = readString("resourceName");
    }
    appId = canonicalAppId(appId);
    QString title = readString("caption", "title");
    QString viewId = readString("internalId");
    if (viewId.isEmpty()) {
        viewId = path;
    }
    int space = props.value(QStringLiteral("desktop"), m_activeSpace).toInt();
    if (space <= 0) {
        space = m_activeSpace;
    }

    const IconInfo icon = resolveIcon(appId, title);
    QVariantMap item = {
        { QStringLiteral("viewId"), viewId },
        { QStringLiteral("title"), title },
        { QStringLiteral("appId"), appId },
        { QStringLiteral("iconSource"), icon.iconSource },
        { QStringLiteral("iconName"), icon.iconName },
        { QStringLiteral("x"), x },
        { QStringLiteral("y"), y },
        { QStringLiteral("width"), w },
        { QStringLiteral("height"), h },
        { QStringLiteral("mapped"), readBool("mapped", true) },
        { QStringLiteral("minimized"), readBool("minimized", false) },
        { QStringLiteral("focused"), readBool("active", false) },
        { QStringLiteral("space"), space },
        { QStringLiteral("lastEvent"), QStringLiteral("snapshot") },
    };
    return item;
}

QVector<QVariantMap> KWinWaylandStateModel::readWindowsFromObjectModel()
{
    QVector<QVariantMap> out;
    const QVariant windowsRaw = readProperty(kKWinPath, kKWinIface, "windows");
    QVariantList list = windowsRaw.toList();
    if (list.isEmpty()) {
        return out;
    }

    out.reserve(list.size());
    for (const QVariant &entry : list) {
        QString path;
        const QDBusObjectPath objectPath = qvariant_cast<QDBusObjectPath>(entry);
        if (!objectPath.path().isEmpty()) {
            path = objectPath.path();
        } else {
            path = entry.toString().trimmed();
        }
        if (path.isEmpty()) {
            continue;
        }
        const QVariantMap window = readWindowFromObjectPath(path);
        if (!window.isEmpty()) {
            out.push_back(window);
        }
    }
    applyActiveWorkspaceVisibility(out, m_activeSpace);
    return out;
}

QVector<QVariantMap> KWinWaylandStateModel::readWindowsFromSupportInformation()
{
    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        return {};
    }
    QDBusReply<QString> reply = kwin.call(QStringLiteral("supportInformation"));
    if (!reply.isValid()) {
        return {};
    }
    const QString dump = reply.value();
    QVector<QVariantMap> parsed = KWinSupportInfoParser::parseSupportInformationDump(dump, m_activeSpace);
    QVector<QVariantMap> out;
    out.reserve(parsed.size());
    for (const QVariantMap &raw : parsed) {
        QVariantMap item = raw;
        const QString appId = canonicalAppId(item.value(QStringLiteral("appId")).toString());
        const QString title = item.value(QStringLiteral("title")).toString();
        const IconInfo icon = resolveIcon(appId, title);
        item.insert(QStringLiteral("appId"), appId);
        item.insert(QStringLiteral("iconSource"), icon.iconSource);
        item.insert(QStringLiteral("iconName"), icon.iconName);
        out.push_back(item);
    }
    applyActiveWorkspaceVisibility(out, m_activeSpace);
    return out;
}

void KWinWaylandStateModel::requestSupportInformationAsync()
{
    if (!connected() || m_supportRequestInFlight) {
        return;
    }

    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        return;
    }

    m_supportRequestInFlight = true;
    const quint64 requestId = ++m_supportRequestSeq;
    QDBusPendingCall pending = kwin.asyncCall(QStringLiteral("supportInformation"));
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId]() {
        m_supportRequestInFlight = false;
        QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (!reply.isValid()) {
            return;
        }

        const QString dump = reply.value();
        const int active = m_activeSpace;
        auto *futureWatcher = new QFutureWatcher<QVector<QVariantMap>>(this);
        connect(futureWatcher, &QFutureWatcher<QVector<QVariantMap>>::finished,
                this, [this, futureWatcher, requestId]() {
            const QVector<QVariantMap> parsed = futureWatcher->result();
            futureWatcher->deleteLater();
            onSupportInformationReady(requestId, parsed);
        });
        futureWatcher->setFuture(QtConcurrent::run([dump, active]() {
            return KWinSupportInfoParser::parseSupportInformationDump(dump, active);
        }));
    });
}

void KWinWaylandStateModel::onSupportInformationReady(quint64 requestId,
                                                      const QVector<QVariantMap> &parsed)
{
    if (!connected()) {
        return;
    }
    if (requestId < m_supportAppliedSeq) {
        return;
    }
    m_supportAppliedSeq = requestId;
    if (parsed.isEmpty()) {
        return;
    }

    QVector<QVariantMap> withIcons;
    withIcons.reserve(parsed.size());
    for (const QVariantMap &raw : parsed) {
        QVariantMap item = raw;
        const QString appId = canonicalAppId(item.value(QStringLiteral("appId")).toString());
        const QString title = item.value(QStringLiteral("title")).toString();
        const IconInfo icon = resolveIcon(appId, title);
        item.insert(QStringLiteral("appId"), appId);
        item.insert(QStringLiteral("iconSource"), icon.iconSource);
        item.insert(QStringLiteral("iconName"), icon.iconName);
        withIcons.push_back(item);
    }

    ++m_supportFallbackHitCount;
    if (m_supportFallbackMinIntervalMs > m_supportFallbackBaseIntervalMs + 1000) {
        m_supportFallbackMinIntervalMs = qMax(m_supportFallbackBaseIntervalMs + 1000,
                                              m_supportFallbackMinIntervalMs - 600);
    }
    setWindows(withIcons);
}

QVariantList KWinWaylandStateModel::parseGeometryList(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QVariantList) {
        const QVariantList list = value.toList();
        if (list.size() >= 4) {
            bool ok0 = false, ok1 = false, ok2 = false, ok3 = false;
            const int x = list.at(0).toInt(&ok0);
            const int y = list.at(1).toInt(&ok1);
            const int w = list.at(2).toInt(&ok2);
            const int h = list.at(3).toInt(&ok3);
            if (ok0 && ok1 && ok2 && ok3) {
                return { x, y, w, h };
            }
        }
    }
    const QString raw = value.toString();
    if (raw.isEmpty()) {
        return {};
    }

    QRegularExpressionMatch m = QRegularExpression(
        QStringLiteral("(\\-?\\d+)\\s*,\\s*(\\-?\\d+)\\s*[xX, ]\\s*(\\d+)\\s*[xX, ]\\s*(\\d+)"))
                                    .match(raw);
    if (m.hasMatch()) {
        return { m.captured(1).toInt(), m.captured(2).toInt(),
                 m.captured(3).toInt(), m.captured(4).toInt() };
    }
    m = QRegularExpression(QStringLiteral("(\\-?\\d+)\\D+(\\-?\\d+)\\D+(\\d+)\\D+(\\d+)")).match(raw);
    if (m.hasMatch()) {
        return { m.captured(1).toInt(), m.captured(2).toInt(),
                 m.captured(3).toInt(), m.captured(4).toInt() };
    }
    return {};
}

QVariantMap KWinWaylandStateModel::parseSupportWindowBlock(const QStringList &block)
{
    QString title;
    QString appId;
    QString viewId;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int space = m_activeSpace;
    bool minimized = false;
    bool focused = false;
    bool hasUsefulField = false;

    for (const QString &rawLine : block) {
        const QString line = normalizeLine(rawLine);
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed().toLower();
        const QString val = line.mid(colon + 1).trimmed();
        if (key.contains(QStringLiteral("caption")) || key == QStringLiteral("title")) {
            title = val;
            hasUsefulField = true;
        } else if (key.contains(QStringLiteral("resourceclass")) ||
                   key.contains(QStringLiteral("desktopfilename")) ||
                   key == QStringLiteral("appid")) {
            appId = val;
            hasUsefulField = true;
        } else if (key.contains(QStringLiteral("internalid")) || key == QStringLiteral("id")) {
            viewId = val;
        } else if (key.contains(QStringLiteral("geometry"))) {
            const QVariantList geom = parseGeometryList(val);
            if (geom.size() >= 4) {
                x = geom.at(0).toInt();
                y = geom.at(1).toInt();
                width = qMax(0, geom.at(2).toInt());
                height = qMax(0, geom.at(3).toInt());
            }
        } else if (key.contains(QStringLiteral("desktop")) || key.contains(QStringLiteral("workspace"))) {
            bool ok = false;
            const int d = val.toInt(&ok);
            if (ok && d > 0) {
                space = d;
            }
        } else if (key.contains(QStringLiteral("minimized"))) {
            minimized = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                         val == QStringLiteral("1") ||
                         val.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        } else if (key == QStringLiteral("active") || key.contains(QStringLiteral("focused"))) {
            focused = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                       val == QStringLiteral("1") ||
                       val.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0);
        }
    }

    if (!hasUsefulField) {
        return {};
    }
    appId = canonicalAppId(appId);
    if (viewId.isEmpty()) {
        viewId = QStringLiteral("kwin:%1:%2:%3").arg(appId, title).arg(space);
    }

    const IconInfo icon = resolveIcon(appId, title);
    return {
        { QStringLiteral("viewId"), viewId },
        { QStringLiteral("title"), title },
        { QStringLiteral("appId"), appId },
        { QStringLiteral("iconSource"), icon.iconSource },
        { QStringLiteral("iconName"), icon.iconName },
        { QStringLiteral("x"), x },
        { QStringLiteral("y"), y },
        { QStringLiteral("width"), width },
        { QStringLiteral("height"), height },
        { QStringLiteral("mapped"), true },
        { QStringLiteral("minimized"), minimized },
        { QStringLiteral("focused"), focused },
        { QStringLiteral("space"), space },
        { QStringLiteral("lastEvent"), QStringLiteral("snapshot") },
    };
}

void KWinWaylandStateModel::setWindows(const QVector<QVariantMap> &next)
{
    auto fingerprint = [](const QVariantMap &w) -> QString {
        return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
            .arg(w.value(QStringLiteral("viewId")).toString(),
                 w.value(QStringLiteral("title")).toString(),
                 w.value(QStringLiteral("appId")).toString(),
                 QString::number(w.value(QStringLiteral("x")).toInt()),
                 QString::number(w.value(QStringLiteral("y")).toInt()),
                 QString::number(w.value(QStringLiteral("width")).toInt()),
                 QString::number(w.value(QStringLiteral("height")).toInt()),
                 QString::number(w.value(QStringLiteral("space")).toInt()),
                 QString::number(w.value(QStringLiteral("mapped")).toBool() ? 1 : 0),
                 QString::number(w.value(QStringLiteral("minimized")).toBool() ? 1 : 0),
                 QString::number(w.value(QStringLiteral("focused")).toBool() ? 1 : 0));
    };

    if (next.size() == m_windows.size()) {
        bool same = true;
        for (int i = 0; i < next.size(); ++i) {
            if (fingerprint(next.at(i)) != fingerprint(m_windows.at(i))) {
                same = false;
                break;
            }
        }
        if (same) {
            return;
        }
    }

    m_windows = next;
    publishSyntheticEvent(QStringLiteral("windows-snapshot"),
                          { { QStringLiteral("windowCount"), m_windows.size() } });
}

void KWinWaylandStateModel::publishSyntheticEvent(const QString &eventName, const QVariantMap &extra)
{
    QVariantMap payload = {
        { QStringLiteral("channel"), QStringLiteral("windowing") },
        { QStringLiteral("backend"), QStringLiteral("kwin-wayland") },
        { QStringLiteral("event"), eventName },
        { QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch() },
    };
    for (auto it = extra.cbegin(); it != extra.cend(); ++it) {
        payload.insert(it.key(), it.value());
    }
    m_lastEvent = payload;
    emit lastEventChanged();
}

void KWinWaylandStateModel::onKWinPropertiesChanged(const QString &interfaceName,
                                                    const QVariantMap &changedProperties,
                                                    const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)
    if (!connected()) {
        return;
    }
    if (interfaceName != QString::fromLatin1(kKWinIface)) {
        return;
    }

    bool needsWindowRefresh = false;
    bool needsSpaceRefresh = false;
    for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
        const QString key = it.key();
        if (key == QStringLiteral("windows")) {
            needsWindowRefresh = true;
        } else if (key == QStringLiteral("currentDesktop") ||
                   key == QStringLiteral("numberOfDesktops")) {
            needsSpaceRefresh = true;
        }
    }
    if (needsSpaceRefresh) {
        ++m_signalTriggerSpaceCount;
        scheduleSpaceRefresh(25);
    }
    if (needsWindowRefresh || needsSpaceRefresh) {
        ++m_signalTriggerWindowCount;
        scheduleWindowRefresh(35);
    }
}

void KWinWaylandStateModel::onVdmPropertiesChanged(const QString &interfaceName,
                                                   const QVariantMap &changedProperties,
                                                   const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)
    if (!connected()) {
        return;
    }
    if (interfaceName != QString::fromLatin1(kVdmIface)) {
        return;
    }

    bool spaceRelevant = false;
    for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
        const QString key = it.key();
        if (key == QStringLiteral("count") ||
            key == QStringLiteral("currentDesktop") ||
            key == QStringLiteral("current") ||
            key == QStringLiteral("desktops")) {
            spaceRelevant = true;
            break;
        }
    }
    if (spaceRelevant) {
        ++m_signalTriggerSpaceCount;
        ++m_signalTriggerWindowCount;
        scheduleSpaceRefresh(25);
        scheduleWindowRefresh(35);
    }
}

void KWinWaylandStateModel::maybeLogProfileSnapshot()
{
    if (!m_profileEnabled) {
        return;
    }
    qInfo().noquote()
        << "[kwin-profile]"
        << "connected=" << (connected() ? 1 : 0)
        << "winCount=" << m_windows.size()
        << "refreshSpaces=" << m_refreshSpacesCount
        << "refreshWindows=" << m_refreshWindowsCount
        << "signalSpace=" << m_signalTriggerSpaceCount
        << "signalWindow=" << m_signalTriggerWindowCount
        << "pollSpace=" << m_pollTriggerSpaceCount
        << "pollWindow=" << m_pollTriggerWindowCount
        << "objHit=" << m_objectModelHitCount
        << "fallbackHit=" << m_supportFallbackHitCount
        << "fallbackSkip=" << m_supportFallbackSkipCount
        << "missStreak=" << m_objectModelMissStreak
        << "dbusBound=" << (m_dbusSignalsBound ? 1 : 0)
        << "fallbackMinMs=" << m_supportFallbackMinIntervalMs
        << "spacePollMs=" << m_spacePollIntervalMs
        << "windowPollMs=" << m_windowPollIntervalMs;
}

KWinWaylandStateModel::IconInfo KWinWaylandStateModel::resolveIcon(const QString &appId,
                                                                   const QString &title)
{
    ensureIconCache();
    if (m_iconByToken.isEmpty()) {
        return {};
    }

    QSet<QString> candidates;
    insertTokenVariants(candidates, appId);
    insertTokenVariants(candidates, title);
    if (!appId.trimmed().isEmpty()) {
        candidates.insert(normalizeToken(QFileInfo(appId).completeBaseName()));
    }
    for (const QString &token : candidates) {
        const auto it = m_iconByToken.constFind(token);
        if (it != m_iconByToken.cend()) {
            return it.value();
        }
    }
    return {};
}

void KWinWaylandStateModel::ensureIconCache()
{
    if (m_iconCacheReady) {
        return;
    }
    m_iconCacheReady = true;
    m_iconByToken.clear();

    GList *infos = g_app_info_get_all();
    for (GList *it = infos; it != nullptr; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }

        IconInfo iconInfo;
        GIcon *icon = g_app_info_get_icon(info);
        iconInfo.iconName = iconNameFromGIcon(icon);
        iconInfo.iconSource = iconSourceFromGIcon(icon);
        if (iconInfo.iconName.isEmpty() && iconInfo.iconSource.isEmpty()) {
            continue;
        }

        QSet<QString> tokens;
        insertTokenVariants(tokens, fromUtf8(g_app_info_get_id(info)));
        insertTokenVariants(tokens, fromUtf8(g_app_info_get_display_name(info)));
        insertTokenVariants(tokens, fromUtf8(g_app_info_get_name(info)));
        insertTokenVariants(tokens, fromUtf8(g_app_info_get_executable(info)));

        if (G_IS_DESKTOP_APP_INFO(info)) {
            GDesktopAppInfo *desktop = G_DESKTOP_APP_INFO(info);
            insertTokenVariants(tokens, fromUtf8(g_desktop_app_info_get_startup_wm_class(desktop)));
            insertTokenVariants(tokens, fromUtf8(g_desktop_app_info_get_string(desktop, "X-Flatpak")));
            insertTokenVariants(tokens, fromUtf8(g_desktop_app_info_get_string(desktop, "Icon")));
            const QString file = fromUtf8(g_desktop_app_info_get_filename(desktop));
            if (!file.isEmpty()) {
                insertTokenVariants(tokens, QFileInfo(file).completeBaseName());
            }
        }

        for (const QString &token : tokens) {
            if (token.isEmpty() || m_iconByToken.contains(token)) {
                continue;
            }
            m_iconByToken.insert(token, iconInfo);
        }
    }
    g_list_free_full(infos, g_object_unref);
}
