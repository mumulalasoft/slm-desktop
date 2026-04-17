#include "contextservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QThread>
#include <QTimeZone>
#include <QtMath>

namespace Slm::Context {

namespace {
constexpr const char kService[] = "org.desktop.Context";
constexpr const char kPath[] = "/org/desktop/Context";
constexpr const char kUpowerService[] = "org.freedesktop.UPower";
constexpr const char kUpowerPath[] = "/org/freedesktop/UPower/devices/DisplayDevice";
constexpr const char kUpowerIface[] = "org.freedesktop.UPower.Device";
constexpr const char kSettingsdService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsdPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsdIface[] = "org.slm.Desktop.Settings";

constexpr const char kKWinService[] = "org.kde.KWin";
constexpr const char kKWinPath[] = "/KWin";
constexpr const char kKWinIface[] = "org.kde.KWin";
constexpr const char kKWinWindowIface[] = "org.kde.KWin.Window";
constexpr const char kNetworkManagerService[] = "org.freedesktop.NetworkManager";
constexpr const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
constexpr const char kNetworkManagerIface[] = "org.freedesktop.NetworkManager";
constexpr const char kScreenSaverService[] = "org.freedesktop.ScreenSaver";
constexpr const char kScreenSaverPath[] = "/org/freedesktop/ScreenSaver";
constexpr const char kScreenSaverIface[] = "org.freedesktop.ScreenSaver";

bool readFullscreenFromKWin(bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        return false;
    }

    const QVariant activeWindowVar = kwin.property("activeWindow");
    const QDBusObjectPath activePath = qvariant_cast<QDBusObjectPath>(activeWindowVar);
    if (activePath.path().isEmpty() || activePath.path() == QLatin1String("/")) {
        return false;
    }

    QDBusInterface activeWindowIface(QString::fromLatin1(kKWinService),
                                     activePath.path(),
                                     QString::fromLatin1(kKWinWindowIface),
                                     QDBusConnection::sessionBus());
    if (!activeWindowIface.isValid()) {
        return false;
    }

    QVariant fullscreenVar = activeWindowIface.property("fullScreen");
    if (!fullscreenVar.isValid()) {
        fullscreenVar = activeWindowIface.property("fullscreen");
    }
    if (!fullscreenVar.isValid()) {
        return false;
    }

    if (ok) {
        *ok = true;
    }
    return fullscreenVar.toBool();
}

QString readActiveAppIdFromKWin(bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        return QString();
    }

    const QVariant activeWindowVar = kwin.property("activeWindow");
    const QDBusObjectPath activePath = qvariant_cast<QDBusObjectPath>(activeWindowVar);
    if (activePath.path().isEmpty() || activePath.path() == QLatin1String("/")) {
        return QString();
    }

    QDBusInterface activeWindowIface(QString::fromLatin1(kKWinService),
                                     activePath.path(),
                                     QString::fromLatin1(kKWinWindowIface),
                                     QDBusConnection::sessionBus());
    if (!activeWindowIface.isValid()) {
        return QString();
    }

    QString appId = activeWindowIface.property("desktopFileName").toString().trimmed();
    if (appId.isEmpty()) {
        appId = activeWindowIface.property("resourceClass").toString().trimmed();
    }
    if (appId.isEmpty()) {
        appId = activeWindowIface.property("resourceName").toString().trimmed();
    }
    if (appId.isEmpty()) {
        return QString();
    }
    if (ok) {
        *ok = true;
    }
    return appId;
}

QVariantMap readNetworkContextFromNM()
{
    QString state = QStringLiteral("offline");
    bool metered = false;
    QString quality = QStringLiteral("offline");
    QString source = QStringLiteral("fallback");

    QDBusInterface nm(QString::fromLatin1(kNetworkManagerService),
                      QString::fromLatin1(kNetworkManagerPath),
                      QString::fromLatin1(kNetworkManagerIface),
                      QDBusConnection::systemBus());
    if (!nm.isValid()) {
        return {
            {QStringLiteral("state"), state},
            {QStringLiteral("metered"), metered},
            {QStringLiteral("quality"), quality},
            {QStringLiteral("source"), source},
        };
    }

    source = QStringLiteral("networkmanager");
    const uint nmState = nm.property("State").toUInt();
    const uint connectivity = nm.property("Connectivity").toUInt();
    const uint meteredRaw = nm.property("Metered").toUInt();

    // NetworkManager.State:
    // 70 = connected global, 60 = connected site, 50 = connected local.
    if (nmState >= 70 || connectivity >= 4) {
        state = QStringLiteral("online");
        quality = QStringLiteral("normal");
    } else if (nmState >= 50 || connectivity == 3) {
        state = QStringLiteral("online");
        quality = QStringLiteral("limited");
    } else {
        state = QStringLiteral("offline");
        quality = QStringLiteral("offline");
    }

    // Metered non-zero is treated as metered/possibly metered to stay safe.
    metered = (meteredRaw > 0u);
    if (metered && quality == QLatin1String("normal")) {
        quality = QStringLiteral("limited");
    }

    return {
        {QStringLiteral("state"), state},
        {QStringLiteral("metered"), metered},
        {QStringLiteral("quality"), quality},
        {QStringLiteral("source"), source},
        {QStringLiteral("connectivityRaw"), static_cast<int>(connectivity)},
    };
}

QVariantMap readAttentionContextFromSession()
{
    bool idle = false;
    int idleTimeSec = 0;
    QString source = QStringLiteral("fallback");

    QDBusInterface screensaver(QString::fromLatin1(kScreenSaverService),
                               QString::fromLatin1(kScreenSaverPath),
                               QString::fromLatin1(kScreenSaverIface),
                               QDBusConnection::sessionBus());
    if (!screensaver.isValid()) {
        return {
            {QStringLiteral("idle"), idle},
            {QStringLiteral("idleTimeSec"), idleTimeSec},
            {QStringLiteral("source"), source},
        };
    }

    source = QStringLiteral("screensaver");

    QDBusReply<bool> activeReply = screensaver.call(QStringLiteral("GetActive"));
    if (activeReply.isValid()) {
        idle = activeReply.value();
    }

    QDBusReply<uint> idleReply = screensaver.call(QStringLiteral("GetSessionIdleTime"));
    if (idleReply.isValid()) {
        idleTimeSec = static_cast<int>(idleReply.value());
        if (idleTimeSec >= 60) {
            idle = true;
        }
    }

    return {
        {QStringLiteral("idle"), idle},
        {QStringLiteral("idleTimeSec"), idleTimeSec},
        {QStringLiteral("source"), source},
    };
}

bool readLoadAverage(double *loadPerCorePercent, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    if (loadPerCorePercent) {
        *loadPerCorePercent = 0.0;
    }

    QFile file(QStringLiteral("/proc/loadavg"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll()).trimmed();
    const QStringList parts = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }

    bool parsed = false;
    const double load1m = parts.first().toDouble(&parsed);
    if (!parsed || load1m < 0.0) {
        return false;
    }
    const int cores = qMax(1, QThread::idealThreadCount());
    const double percent = qBound(0.0, (load1m / static_cast<double>(cores)) * 100.0, 100.0);
    if (loadPerCorePercent) {
        *loadPerCorePercent = percent;
    }
    if (ok) {
        *ok = true;
    }
    return true;
}

bool readMemoryUsage(double *memUsedPercent, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    if (memUsedPercent) {
        *memUsedPercent = 0.0;
    }

    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    qint64 memTotalKb = -1;
    qint64 memAvailableKb = -1;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            const QStringList p = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (p.size() >= 2) {
                memTotalKb = p.at(1).toLongLong();
            }
        } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
            const QStringList p = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (p.size() >= 2) {
                memAvailableKb = p.at(1).toLongLong();
            }
        }
        if (memTotalKb > 0 && memAvailableKb >= 0) {
            break;
        }
    }
    if (memTotalKb <= 0 || memAvailableKb < 0) {
        return false;
    }

    const double used = qBound(0.0,
                               (1.0 - (static_cast<double>(memAvailableKb) / static_cast<double>(memTotalKb))) * 100.0,
                               100.0);
    if (memUsedPercent) {
        *memUsedPercent = used;
    }
    if (ok) {
        *ok = true;
    }
    return true;
}

QVariantMap normalizedForSemanticCompare(const QVariantMap &context)
{
    QVariantMap normalized = context;

    QVariantMap system = normalized.value(QStringLiteral("system")).toMap();
    if (!system.isEmpty()) {
        // Avoid noisy emission on minor fluctuations.
        const int cpuBucket = qBound(0, qRound(system.value(QStringLiteral("cpuLoadPercent")).toDouble() / 10.0), 10);
        const int memBucket = qBound(0, qRound(system.value(QStringLiteral("memoryUsedPercent")).toDouble() / 10.0), 10);
        system.insert(QStringLiteral("cpuLoadBucket"), cpuBucket);
        system.insert(QStringLiteral("memoryUsedBucket"), memBucket);
        system.remove(QStringLiteral("cpuLoadPercent"));
        system.remove(QStringLiteral("memoryUsedPercent"));
        normalized.insert(QStringLiteral("system"), system);
    }

    QVariantMap attention = normalized.value(QStringLiteral("attention")).toMap();
    if (!attention.isEmpty()) {
        // Idle time can tick every read; keep coarse minute bucket for semantic compare.
        const int idleSec = attention.value(QStringLiteral("idleTimeSec")).toInt();
        attention.insert(QStringLiteral("idleTimeBucketMin"), qMax(0, idleSec / 60));
        attention.remove(QStringLiteral("idleTimeSec"));
        normalized.insert(QStringLiteral("attention"), attention);
    }

    QVariantMap network = normalized.value(QStringLiteral("network")).toMap();
    if (!network.isEmpty()) {
        // Raw connectivity codes are backend noisy, not semantic.
        network.remove(QStringLiteral("connectivityRaw"));
        normalized.insert(QStringLiteral("network"), network);
    }

    return normalized;
}
}

ContextService::ContextService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(30000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ContextService::refreshContext);
}

ContextService::~ContextService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool ContextService::start(QString *error)
{
    registerDbusService();
    if (!m_serviceRegistered) {
        if (error) {
            *error = QStringLiteral("failed to register DBus service org.desktop.Context on session bus");
        }
        return false;
    }
    m_context = buildContext();
    m_refreshTimer.start();
    return true;
}

bool ContextService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QVariantMap ContextService::GetContext() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("context"), m_context},
    };
}

QVariantMap ContextService::Subscribe() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("signals"), QStringList{
             QStringLiteral("ContextChanged"),
             QStringLiteral("PowerChanged"),
             QStringLiteral("PowerStateChanged"),
             QStringLiteral("TimePeriodChanged"),
             QStringLiteral("ActivityChanged"),
             QStringLiteral("PerformanceChanged"),
         }},
    };
}

QVariantMap ContextService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
    };
}

void ContextService::refreshContext()
{
    const QVariantMap next = buildContext();
    const QVariantMap prev = m_context;
    m_context = next;
    publishContextDelta(prev, next);
}

void ContextService::publishContextDelta(const QVariantMap &prev, const QVariantMap &next)
{
    // Always keep latest snapshot for GetContext(), but emit only on semantic changes.
    if (normalizedForSemanticCompare(next) == normalizedForSemanticCompare(prev)) {
        return;
    }

    emit ContextChanged(m_context);

    const QVariantMap prevPower = prev.value(QStringLiteral("power")).toMap();
    const QVariantMap nextPower = next.value(QStringLiteral("power")).toMap();
    if (prevPower != nextPower) {
        emit PowerChanged(nextPower);
        emit PowerStateChanged(nextPower);
    }

    const QString prevPeriod = prev.value(QStringLiteral("time")).toMap().value(QStringLiteral("period")).toString();
    const QString nextPeriod = next.value(QStringLiteral("time")).toMap().value(QStringLiteral("period")).toString();
    if (prevPeriod != nextPeriod) {
        emit TimePeriodChanged(nextPeriod);
    }

    const QVariantMap prevActivity = prev.value(QStringLiteral("activity")).toMap();
    const QVariantMap nextActivity = next.value(QStringLiteral("activity")).toMap();
    if (prevActivity != nextActivity) {
        emit ActivityChanged(nextActivity);
    }

    const QVariantMap prevPerf = prev.value(QStringLiteral("system")).toMap();
    const QVariantMap nextPerf = next.value(QStringLiteral("system")).toMap();
    if (prevPerf != nextPerf) {
        emit PerformanceChanged(nextPerf);
    }
}

#ifdef SLM_CONTEXT_TEST_HOOKS
void ContextService::InjectContextForTest(const QVariantMap &context)
{
    const QVariantMap prev = m_context;
    m_context = context;
    publishContextDelta(prev, context);
}
#endif

void ContextService::registerDbusService()
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
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

QVariantMap ContextService::buildContext() const
{
    const QVariantMap settingsSnapshot = fetchSettingsSnapshot();

    QVariantMap context{
        {QStringLiteral("time"), buildTimeContext(settingsSnapshot)},
        {QStringLiteral("power"), buildPowerContext()},
        {QStringLiteral("device"), QVariantMap{
             {QStringLiteral("type"), QStringLiteral("desktop")},
             {QStringLiteral("touchCapable"), false},
         }},
        {QStringLiteral("display"), buildDisplayContext()},
        {QStringLiteral("activity"), buildActivityContext()},
        {QStringLiteral("system"), buildSystemContext()},
        {QStringLiteral("network"), buildNetworkContext()},
        {QStringLiteral("attention"), buildAttentionContext()},
    };

    context.insert(QStringLiteral("rules"),
                   m_ruleEngine.evaluate(context, settingsSnapshot));
    return context;
}

QVariantMap ContextService::buildTimeContext(const QVariantMap &settingsSnapshot) const
{
    const int hour = QTime::currentTime().hour();
    QString period = (hour >= 18 || hour < 6)
        ? QStringLiteral("night")
        : QStringLiteral("day");
    QString source = QStringLiteral("local-hour");

    const QVariantMap timePolicy = settingsSnapshot.value(QStringLiteral("contextTime")).toMap();
    const QString mode = timePolicy.value(QStringLiteral("mode"), QStringLiteral("local"))
                             .toString()
                             .trimmed()
                             .toLower();
    const int sunriseHour = qBound(0, timePolicy.value(QStringLiteral("sunriseHour"), 6).toInt(), 23);
    const int sunsetHour = qBound(0, timePolicy.value(QStringLiteral("sunsetHour"), 18).toInt(), 23);

    if (mode == QLatin1String("sun")) {
        if (sunriseHour != sunsetHour) {
            bool isDay = false;
            if (sunriseHour < sunsetHour) {
                isDay = (hour >= sunriseHour && hour < sunsetHour);
            } else {
                // Wrap-around window for high-latitude/custom configurations.
                isDay = (hour >= sunriseHour || hour < sunsetHour);
            }
            period = isDay ? QStringLiteral("day") : QStringLiteral("night");
            source = QStringLiteral("sun-hours");
        } else {
            source = QStringLiteral("sun-hours-fallback-local-hour");
        }
    }
    return {
        {QStringLiteral("period"), period},
        {QStringLiteral("source"), source},
        {QStringLiteral("mode"), mode == QLatin1String("sun") ? QStringLiteral("sun") : QStringLiteral("local")},
        {QStringLiteral("sunriseHour"), sunriseHour},
        {QStringLiteral("sunsetHour"), sunsetHour},
        {QStringLiteral("hour"), hour},
        {QStringLiteral("timezone"), QString::fromUtf8(QTimeZone::systemTimeZoneId())},
    };
}

QVariantMap ContextService::buildPowerContext() const
{
    QString mode = QStringLiteral("ac");
    int level = 100;
    bool lowPower = false;

    QDBusInterface upower(QString::fromLatin1(kUpowerService),
                          QString::fromLatin1(kUpowerPath),
                          QString::fromLatin1(kUpowerIface),
                          QDBusConnection::systemBus());
    if (upower.isValid()) {
        const QVariant percentageVar = upower.property("Percentage");
        if (percentageVar.isValid()) {
            level = qBound(0, static_cast<int>(qRound(percentageVar.toDouble())), 100);
        }
        const QVariant stateVar = upower.property("State");
        if (stateVar.isValid()) {
            const uint state = stateVar.toUInt();
            // UPower states: 1 Charging, 2 Discharging, 4 FullyCharged, etc.
            mode = (state == 2) ? QStringLiteral("battery") : QStringLiteral("ac");
        }
        lowPower = mode == QLatin1String("battery") && level <= 20;
    }

    return {
        {QStringLiteral("mode"), mode},
        {QStringLiteral("level"), level},
        {QStringLiteral("lowPower"), lowPower},
    };
}

QVariantMap ContextService::buildDisplayContext() const
{
    bool fullscreenOk = false;
    const bool fullscreen = readFullscreenFromKWin(&fullscreenOk);

    return {
        {QStringLiteral("multiMonitor"), false},
        {QStringLiteral("fullscreen"), fullscreen},
        {QStringLiteral("fullscreenSource"), fullscreenOk ? QStringLiteral("kwin") : QStringLiteral("fallback")},
        {QStringLiteral("scaleClass"), QStringLiteral("normal")},
        {QStringLiteral("refreshClass"), QStringLiteral("normal")},
    };
}

QVariantMap ContextService::buildActivityContext() const
{
    bool appOk = false;
    const QString appId = readActiveAppIdFromKWin(&appOk);
    return {
        {QStringLiteral("type"), QStringLiteral("normal")},
        {QStringLiteral("appId"), appId},
        {QStringLiteral("workspaceId"), QString()},
        {QStringLiteral("source"), appOk ? QStringLiteral("kwin") : QStringLiteral("fallback")},
    };
}

QVariantMap ContextService::buildSystemContext() const
{
    double cpuLoadPercent = 0.0;
    bool cpuOk = false;
    readLoadAverage(&cpuLoadPercent, &cpuOk);

    double memUsedPercent = 0.0;
    bool memOk = false;
    readMemoryUsage(&memUsedPercent, &memOk);

    QString performance = QStringLiteral("normal");
    if ((cpuOk && cpuLoadPercent >= 90.0) || (memOk && memUsedPercent >= 92.0)) {
        performance = QStringLiteral("low");
    } else if ((cpuOk && cpuLoadPercent <= 35.0) && (memOk && memUsedPercent <= 75.0)) {
        performance = QStringLiteral("high");
    }

    return {
        {QStringLiteral("performance"), performance},
        {QStringLiteral("cpuLoadPercent"), cpuLoadPercent},
        {QStringLiteral("memoryUsedPercent"), memUsedPercent},
        {QStringLiteral("cpuSource"), cpuOk ? QStringLiteral("loadavg") : QStringLiteral("fallback")},
        {QStringLiteral("memorySource"), memOk ? QStringLiteral("meminfo") : QStringLiteral("fallback")},
    };
}

QVariantMap ContextService::buildNetworkContext() const
{
    return readNetworkContextFromNM();
}

QVariantMap ContextService::buildAttentionContext() const
{
    QVariantMap out = readAttentionContextFromSession();
    bool fullscreenOk = false;
    const bool fullscreen = readFullscreenFromKWin(&fullscreenOk);
    out.insert(QStringLiteral("dnd"), false);
    out.insert(QStringLiteral("fullscreenSuppression"), fullscreenOk ? fullscreen : false);
    return {
        {QStringLiteral("idle"), out.value(QStringLiteral("idle"), false).toBool()},
        {QStringLiteral("idleTimeSec"), out.value(QStringLiteral("idleTimeSec"), 0).toInt()},
        {QStringLiteral("dnd"), out.value(QStringLiteral("dnd"), false).toBool()},
        {QStringLiteral("fullscreenSuppression"), out.value(QStringLiteral("fullscreenSuppression"), false).toBool()},
        {QStringLiteral("source"), out.value(QStringLiteral("source"), QStringLiteral("fallback")).toString()},
    };
}

QVariantMap ContextService::fetchSettingsSnapshot() const
{
    QDBusInterface iface(QString::fromLatin1(kSettingsdService),
                         QString::fromLatin1(kSettingsdPath),
                         QString::fromLatin1(kSettingsdIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetSettings"));
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok"), false).toBool()) {
        return {};
    }
    return out.value(QStringLiteral("settings")).toMap();
}

} // namespace Slm::Context
