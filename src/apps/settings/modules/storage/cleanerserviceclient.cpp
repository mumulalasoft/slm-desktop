#include "cleanerserviceclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Cleaner";
constexpr const char kPath[] = "/org/slm/Desktop/Cleaner";
constexpr const char kIface[] = "org.slm.Desktop.Cleaner";
}

CleanerServiceClient::CleanerServiceClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"),
                QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("NameOwnerChanged"),
                this,
                SLOT(onNameOwnerChanged(QString,QString,QString)));

    refreshAvailability();
}

CleanerServiceClient::~CleanerServiceClient()
{
    delete m_iface;
}

bool CleanerServiceClient::available() const { return m_available; }
bool CleanerServiceClient::scanning() const { return m_scanning; }
bool CleanerServiceClient::cleaning() const { return m_cleaning; }
int CleanerServiceClient::progressPercent() const { return m_progressPercent; }
QString CleanerServiceClient::progressMessage() const { return m_progressMessage; }
QVariantMap CleanerServiceClient::scanResult() const { return m_scanResult; }
QVariantMap CleanerServiceClient::lastResult() const { return m_lastResult; }
QVariantList CleanerServiceClient::appCaches() const { return m_scanResult.value(QStringLiteral("apps")).toList(); }
QVariantList CleanerServiceClient::smartSuggestions() const { return m_scanResult.value(QStringLiteral("smartSuggestions")).toList(); }
QString CleanerServiceClient::lastError() const { return m_lastError; }
bool CleanerServiceClient::autoClean() const { return m_autoClean; }
int CleanerServiceClient::maxCacheSizeMb() const { return m_maxCacheSizeMb; }
int CleanerServiceClient::deleteAfterDays() const { return m_deleteAfterDays; }

void CleanerServiceClient::requestScan()
{
    if (m_scanning) {
        return;
    }
    m_scanning = true;
    emit scanningChanged();
    setLastError(QString());

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher]() {
        const QVariantMap res = watcher->result();
        watcher->deleteLater();
        m_scanning = false;
        emit scanningChanged();
        m_scanResult = res;
        emit scanResultChanged();
        if (!res.value(QStringLiteral("ok"), false).toBool()) {
            setLastError(res.value(QStringLiteral("error")).toString());
        }
    });
    watcher->setFuture(QtConcurrent::run([this]() {
        return callMapMethod(QStringLiteral("Scan"));
    }));
}

void CleanerServiceClient::requestPreview(const QVariantMap &options)
{
    if (m_cleaning) {
        return;
    }
    m_cleaning = true;
    m_progressPercent = 0;
    m_progressMessage = QStringLiteral("Previewing...");
    emit cleaningChanged();
    emit progressChanged();
    setLastError(QString());

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher]() {
        const QVariantMap res = watcher->result();
        watcher->deleteLater();
        m_cleaning = false;
        m_progressPercent = 100;
        m_progressMessage = QStringLiteral("Preview completed");
        emit cleaningChanged();
        emit progressChanged();
        m_lastResult = res;
        emit lastResultChanged();
        if (!res.value(QStringLiteral("ok"), false).toBool()) {
            setLastError(res.value(QStringLiteral("error")).toString());
        }
    });
    watcher->setFuture(QtConcurrent::run([this, options]() {
        QVariantMap opts = options;
        opts.insert(QStringLiteral("preview"), true);
        return callMapMethod(QStringLiteral("PreviewClean"), {opts});
    }));
}

void CleanerServiceClient::requestClean(const QVariantMap &options)
{
    if (m_cleaning) {
        return;
    }
    m_cleaning = true;
    m_progressPercent = 0;
    m_progressMessage = QStringLiteral("Cleaning...");
    emit cleaningChanged();
    emit progressChanged();
    setLastError(QString());

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher]() {
        const QVariantMap res = watcher->result();
        watcher->deleteLater();
        m_cleaning = false;
        m_progressPercent = 100;
        m_progressMessage = QStringLiteral("Done");
        emit cleaningChanged();
        emit progressChanged();
        m_lastResult = res;
        emit lastResultChanged();
        if (!res.value(QStringLiteral("ok"), false).toBool()) {
            setLastError(res.value(QStringLiteral("error")).toString());
        } else {
            requestScan();
        }
    });
    watcher->setFuture(QtConcurrent::run([this, options]() {
        return callMapMethod(QStringLiteral("Clean"), {options});
    }));
}

void CleanerServiceClient::refreshAvailability()
{
    const bool wasAvailable = m_available;
    m_available = ensureIface();
    if (m_available != wasAvailable) {
        emit availableChanged();
    }
    if (m_available) {
        requestPolicy();
    }
}

void CleanerServiceClient::requestPolicy()
{
    const QVariantMap policy = callMapMethod(QStringLiteral("GetPolicy"));
    if (!policy.value(QStringLiteral("ok"), false).toBool()) {
        return;
    }
    const bool autoClean = policy.value(QStringLiteral("auto_clean"), m_autoClean).toBool();
    const int maxMb = policy.value(QStringLiteral("max_cache_size_mb"), m_maxCacheSizeMb).toInt();
    const int days = policy.value(QStringLiteral("delete_after_days"), m_deleteAfterDays).toInt();
    bool changed = false;
    if (m_autoClean != autoClean) {
        m_autoClean = autoClean;
        changed = true;
    }
    if (m_maxCacheSizeMb != maxMb) {
        m_maxCacheSizeMb = maxMb;
        changed = true;
    }
    if (m_deleteAfterDays != days) {
        m_deleteAfterDays = days;
        changed = true;
    }
    if (changed) {
        emit policyChanged();
    }
}

bool CleanerServiceClient::updatePolicy(const QVariantMap &policyPatch)
{
    const QVariantMap result = callMapMethod(QStringLiteral("SetPolicy"), {policyPatch});
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        setLastError(result.value(QStringLiteral("error")).toString());
        return false;
    }
    const bool autoClean = result.value(QStringLiteral("auto_clean"), m_autoClean).toBool();
    const int maxMb = result.value(QStringLiteral("max_cache_size_mb"), m_maxCacheSizeMb).toInt();
    const int days = result.value(QStringLiteral("delete_after_days"), m_deleteAfterDays).toInt();
    bool changed = false;
    if (m_autoClean != autoClean) {
        m_autoClean = autoClean;
        changed = true;
    }
    if (m_maxCacheSizeMb != maxMb) {
        m_maxCacheSizeMb = maxMb;
        changed = true;
    }
    if (m_deleteAfterDays != days) {
        m_deleteAfterDays = days;
        changed = true;
    }
    if (changed) {
        emit policyChanged();
    }
    return true;
}

void CleanerServiceClient::onNameOwnerChanged(const QString &name,
                                              const QString &,
                                              const QString &)
{
    if (name != QLatin1String(kService)) {
        return;
    }
    refreshAvailability();
}

void CleanerServiceClient::onDaemonProgressChanged(int percent, const QString &message)
{
    m_progressPercent = qBound(0, percent, 100);
    m_progressMessage = message;
    emit progressChanged();
}

bool CleanerServiceClient::ensureIface()
{
    QDBusConnectionInterface *busIface = QDBusConnection::sessionBus().interface();
    if (!busIface) {
        return false;
    }
    const bool registered = busIface->isServiceRegistered(QString::fromLatin1(kService)).value();
    if (!registered) {
        delete m_iface;
        m_iface = nullptr;
        return false;
    }
    if (m_iface && m_iface->isValid()) {
        return true;
    }
    delete m_iface;
    m_iface = new QDBusInterface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus(),
                                 this);

    if (m_iface->isValid()) {
        QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                              QString::fromLatin1(kPath),
                                              QString::fromLatin1(kIface),
                                              QStringLiteral("ProgressChanged"),
                                              this,
                                              SLOT(onDaemonProgressChanged(int,QString)));
    }
    return m_iface->isValid();
}

QVariantMap CleanerServiceClient::callMapMethod(const QString &method, const QVariantList &args)
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("cleanerd-unavailable")},
        };
    }
    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), reply.error().message()},
        };
    }
    return reply.value();
}

void CleanerServiceClient::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
