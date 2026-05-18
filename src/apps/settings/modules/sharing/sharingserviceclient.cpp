#include "sharingserviceclient.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>

namespace {
constexpr const char kService[]       = "org.slm.Sharing";
constexpr const char kRootPath[]      = "/org/slm/Sharing";
constexpr const char kRootIface[]     = "org.slm.Sharing";
constexpr const char kNearbyPath[]    = "/org/slm/Sharing/Nearby";
constexpr const char kNearbyIface[]   = "org.slm.Sharing.Nearby";
constexpr const char kTrustPath[]     = "/org/slm/Sharing/Trust";
constexpr const char kTrustIface[]    = "org.slm.Sharing.Trust";
}

// ── DBus type normalisation (same helper as FirewallServiceClient) ────────────

static QVariantMap normalizeDbusMap(const QVariantMap &input);
static QVariantList normalizeDbusList(const QVariantList &input);

static QVariant normalizeDbusVariant(const QVariant &v)
{
    if (v.userType() == qMetaTypeId<QDBusVariant>())
        return normalizeDbusVariant(qvariant_cast<QDBusVariant>(v).variant());
    if (v.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = qvariant_cast<QDBusArgument>(v);
        // QDBusArgument's read cursor is consumed by qdbus_cast, so we must
        // pick the right cast based on the signature instead of trying both —
        // the second cast would read past the end and libdbus aborts with
        // "You can't recurse into an empty array or off the end of a message
        // body" (which crashes slm-settings before any window appears).
        const QString sig = arg.currentSignature();
        if (sig.startsWith(QLatin1String("a{"))) {
            return normalizeDbusMap(qdbus_cast<QVariantMap>(arg));
        }
        if (sig.startsWith(QLatin1Char('a'))) {
            return normalizeDbusList(qdbus_cast<QVariantList>(arg));
        }
        return v;
    }
    if (v.userType() == QMetaType::QVariantMap)
        return normalizeDbusMap(v.toMap());
    if (v.userType() == QMetaType::QVariantList)
        return normalizeDbusList(v.toList());
    return v;
}

static QVariantMap normalizeDbusMap(const QVariantMap &input)
{
    QVariantMap out;
    for (auto it = input.begin(); it != input.end(); ++it)
        out.insert(it.key(), normalizeDbusVariant(it.value()));
    return out;
}

static QVariantList normalizeDbusList(const QVariantList &input)
{
    QVariantList out;
    out.reserve(input.size());
    for (const auto &v : input)
        out.append(normalizeDbusVariant(v));
    return out;
}

// ── Constructor / destructor ──────────────────────────────────────────────────

SharingServiceClient::SharingServiceClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(onNameOwnerChanged(QString, QString, QString)));

    ensureIface();
    refresh();
}

SharingServiceClient::~SharingServiceClient() = default;

// ── Properties ────────────────────────────────────────────────────────────────

bool SharingServiceClient::available() const           { return m_available; }
bool SharingServiceClient::fileSharingEnabled() const      { return m_fileSharingEnabled; }
bool SharingServiceClient::nearbySharingEnabled() const    { return m_nearbySharingEnabled; }
bool SharingServiceClient::screenSharingEnabled() const    { return m_screenSharingEnabled; }
bool SharingServiceClient::printerSharingEnabled() const   { return m_printerSharingEnabled; }
bool SharingServiceClient::remoteAccessEnabled() const     { return m_remoteAccessEnabled; }
bool SharingServiceClient::mediaSharingEnabled() const     { return m_mediaSharingEnabled; }
bool SharingServiceClient::clipboardSharingEnabled() const { return m_clipboardSharingEnabled; }
QVariantList SharingServiceClient::nearbyDevices() const   { return m_nearbyDevices; }
QVariantList SharingServiceClient::trustedDevices() const  { return m_trustedDevices; }
QVariantList SharingServiceClient::sharedFolders() const   { return m_sharedFolders; }
QVariantList SharingServiceClient::activeTransfers() const { return m_activeTransfers; }
bool SharingServiceClient::nearbyDiscovering() const       { return m_nearbyDiscovering; }

// ── Public invokables ─────────────────────────────────────────────────────────

bool SharingServiceClient::refresh()
{
    if (!ensureIface())
        return false;

    QDBusReply<QVariantMap> statusReply = m_iface->call(QStringLiteral("GetStatus"));
    if (statusReply.isValid()) {
        const QVariantMap features = normalizeDbusMap(statusReply.value())
                                         .value(QStringLiteral("features")).toMap();
        applyFeatureStates(features);
    }

    refreshSharedFolders();
    refreshNearbyDevices();
    refreshTrustedDevices();
    refreshActiveTransfers();
    return true;
}

bool SharingServiceClient::setFeatureEnabled(const QString &feature, bool enabled)
{
    if (!ensureIface())
        return false;
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetFeatureEnabled"), feature, enabled);
    if (!reply.isValid())
        return false;
    return normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

QVariantMap SharingServiceClient::addSharedFolder(const QString &path, const QVariantMap &options)
{
    if (!ensureIface())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("unavailable")}};
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("AddSharedFolder"), path, options);
    return reply.isValid() ? normalizeDbusMap(reply.value())
                           : QVariantMap{{QStringLiteral("ok"), false}};
}

bool SharingServiceClient::removeSharedFolder(const QString &path)
{
    if (!ensureIface())
        return false;
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("RemoveSharedFolder"), path);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

QVariantMap SharingServiceClient::sendFileTo(const QString &deviceId, const QString &path)
{
    if (!ensureIface())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("unavailable")}};
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SendFile"), deviceId, path);
    return reply.isValid() ? normalizeDbusMap(reply.value())
                           : QVariantMap{{QStringLiteral("ok"), false}};
}

bool SharingServiceClient::cancelTransfer(const QString &transferId)
{
    if (!ensureIface())
        return false;
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("CancelTransfer"), transferId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::startDiscovery()
{
    if (!m_nearbyIface || !m_nearbyIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_nearbyIface->call(QStringLiteral("StartDiscovery"));
    const bool ok = reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
    if (ok && !m_nearbyDiscovering) {
        m_nearbyDiscovering = true;
        emit nearbyDiscoveringChanged();
    }
    return ok;
}

bool SharingServiceClient::stopDiscovery()
{
    if (!m_nearbyIface || !m_nearbyIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_nearbyIface->call(QStringLiteral("StopDiscovery"));
    const bool ok = reply.isValid();
    if (ok && m_nearbyDiscovering) {
        m_nearbyDiscovering = false;
        emit nearbyDiscoveringChanged();
    }
    return ok;
}

QVariantMap SharingServiceClient::pairDevice(const QString &deviceId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("unavailable")}};
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("PairDevice"), deviceId);
    return reply.isValid() ? normalizeDbusMap(reply.value())
                           : QVariantMap{{QStringLiteral("ok"), false}};
}

bool SharingServiceClient::unpairDevice(const QString &deviceId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("UnpairDevice"), deviceId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::blockDevice(const QString &deviceId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("BlockDevice"), deviceId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::unblockDevice(const QString &deviceId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("UnblockDevice"), deviceId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::setDevicePermission(const QString &deviceId,
                                                const QString &permission,
                                                bool allowed)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(
        QStringLiteral("SetDevicePermission"), deviceId, permission, allowed);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::acceptPairingRequest(const QString &pairingId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("AcceptPairingRequest"), pairingId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

bool SharingServiceClient::rejectPairingRequest(const QString &pairingId)
{
    if (!m_trustIface || !m_trustIface->isValid())
        return false;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("RejectPairingRequest"), pairingId);
    return reply.isValid() && normalizeDbusMap(reply.value()).value(QStringLiteral("ok"), false).toBool();
}

QVariantMap SharingServiceClient::checkEnvironment()
{
    if (!ensureIface())
        return {{QStringLiteral("ok"), false}};
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("CheckEnvironment"));
    return reply.isValid() ? normalizeDbusMap(reply.value()) : QVariantMap{{QStringLiteral("ok"), false}};
}

// ── DBus signal slots ─────────────────────────────────────────────────────────

void SharingServiceClient::onNameOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner)
{
    if (name != QLatin1String(kService))
        return;
    Q_UNUSED(oldOwner)
    if (!newOwner.isEmpty()) {
        ensureIface();
        refresh();
        return;
    }
    if (m_available) {
        m_available = false;
        emit availableChanged();
    }
    delete m_iface;       m_iface       = nullptr;
    delete m_nearbyIface; m_nearbyIface = nullptr;
    delete m_trustIface;  m_trustIface  = nullptr;

    m_nearbyDevices.clear();    emit nearbyDevicesChanged();
    m_trustedDevices.clear();   emit trustedDevicesChanged();
    m_sharedFolders.clear();    emit sharedFoldersChanged();
    m_activeTransfers.clear();  emit activeTransfersChanged();
}

void SharingServiceClient::onFeatureStateChanged(const QString &feature, bool enabled)
{
    QVariantMap current;
    current.insert(QStringLiteral("file-sharing"),      m_fileSharingEnabled);
    current.insert(QStringLiteral("nearby-sharing"),    m_nearbySharingEnabled);
    current.insert(QStringLiteral("screen-sharing"),    m_screenSharingEnabled);
    current.insert(QStringLiteral("printer-sharing"),   m_printerSharingEnabled);
    current.insert(QStringLiteral("remote-access"),     m_remoteAccessEnabled);
    current.insert(QStringLiteral("media-sharing"),     m_mediaSharingEnabled);
    current.insert(QStringLiteral("clipboard-sharing"), m_clipboardSharingEnabled);
    current.insert(feature, enabled);
    applyFeatureStates(current);
}

void SharingServiceClient::onSharedFolderAdded(const QString &path, const QVariantMap &info)
{
    Q_UNUSED(path)
    Q_UNUSED(info)
    refreshSharedFolders();
}

void SharingServiceClient::onSharedFolderRemoved(const QString &path)
{
    Q_UNUSED(path)
    refreshSharedFolders();
}

void SharingServiceClient::onDeviceFound(const QString &deviceId, const QVariantMap &info)
{
    const QVariantMap normalized = normalizeDbusMap(info);
    // Upsert — remove stale entry for same deviceId if present
    m_nearbyDevices.erase(
        std::remove_if(m_nearbyDevices.begin(), m_nearbyDevices.end(),
                       [&deviceId](const QVariant &v) {
                           return v.toMap().value(QStringLiteral("deviceId")).toString() == deviceId;
                       }),
        m_nearbyDevices.end());
    m_nearbyDevices.append(normalized);
    emit nearbyDevicesChanged();
}

void SharingServiceClient::onDeviceLost(const QString &deviceId)
{
    const int before = m_nearbyDevices.size();
    m_nearbyDevices.erase(
        std::remove_if(m_nearbyDevices.begin(), m_nearbyDevices.end(),
                       [&deviceId](const QVariant &v) {
                           return v.toMap().value(QStringLiteral("deviceId")).toString() == deviceId;
                       }),
        m_nearbyDevices.end());
    if (m_nearbyDevices.size() != before)
        emit nearbyDevicesChanged();
}

void SharingServiceClient::onDeviceUpdated(const QString &deviceId, const QVariantMap &info)
{
    onDeviceFound(deviceId, info);
}

void SharingServiceClient::onTransferStarted(const QString &transferId, const QVariantMap &info)
{
    m_activeTransfers.append(normalizeDbusMap(info));
    Q_UNUSED(transferId)
    emit activeTransfersChanged();
}

void SharingServiceClient::onTransferProgress(const QString &transferId, qint64 transferred, qint64 total)
{
    for (auto &v : m_activeTransfers) {
        QVariantMap entry = v.toMap();
        if (entry.value(QStringLiteral("transferId")).toString() == transferId) {
            entry.insert(QStringLiteral("bytesTransferred"), transferred);
            entry.insert(QStringLiteral("totalBytes"), total);
            v = entry;
        }
    }
    emit activeTransfersChanged();
}

void SharingServiceClient::onTransferCompleted(const QString &transferId, bool success, const QString &error)
{
    Q_UNUSED(success)
    Q_UNUSED(error)
    m_activeTransfers.erase(
        std::remove_if(m_activeTransfers.begin(), m_activeTransfers.end(),
                       [&transferId](const QVariant &v) {
                           return v.toMap().value(QStringLiteral("transferId")).toString() == transferId;
                       }),
        m_activeTransfers.end());
    emit activeTransfersChanged();
}

void SharingServiceClient::onTransferIncomingRequest(const QString &transferId,
                                                      const QString &fromDeviceId,
                                                      const QVariantMap &info)
{
    emit transferIncomingRequest(transferId, fromDeviceId, normalizeDbusMap(info));
}

void SharingServiceClient::onDeviceTrustChanged(const QString &deviceId, const QString &level)
{
    Q_UNUSED(deviceId)
    Q_UNUSED(level)
    refreshTrustedDevices();
}

void SharingServiceClient::onPairingRequested(const QString &pairingId, const QVariantMap &deviceInfo)
{
    emit pairingRequested(pairingId, normalizeDbusMap(deviceInfo));
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool SharingServiceClient::ensureIface()
{
    auto makeIface = [this](const char *path, const char *iface, QDBusInterface *&ptr) {
        if (ptr && ptr->isValid())
            return true;
        delete ptr;
        ptr = new QDBusInterface(QLatin1String(kService), QLatin1String(path),
                                  QLatin1String(iface),
                                  QDBusConnection::sessionBus(), this);
        return ptr->isValid();
    };

    const bool rootOk    = makeIface(kRootPath,   kRootIface,   m_iface);
    const bool nearbyOk  = makeIface(kNearbyPath,  kNearbyIface, m_nearbyIface);
    const bool trustOk   = makeIface(kTrustPath,   kTrustIface,  m_trustIface);
    const bool nowAvail  = rootOk && nearbyOk && trustOk;

    if (nowAvail != m_available) {
        m_available = nowAvail;
        emit availableChanged();
    }

    if (nowAvail) {
        auto connect4 = [](const char *path, const char *iface,
                           const char *sigName, const char *slot, QObject *recv) {
            QDBusConnection::sessionBus().connect(
                QLatin1String(kService), QLatin1String(path), QLatin1String(iface),
                QString::fromLatin1(sigName), recv, slot);
        };
        connect4(kRootPath, kRootIface, "FeatureStateChanged",
                 SLOT(onFeatureStateChanged(QString,bool)),           this);
        connect4(kRootPath, kRootIface, "SharedFolderAdded",
                 SLOT(onSharedFolderAdded(QString,QVariantMap)),      this);
        connect4(kRootPath, kRootIface, "SharedFolderRemoved",
                 SLOT(onSharedFolderRemoved(QString)),                this);
        connect4(kRootPath, kRootIface, "TransferStarted",
                 SLOT(onTransferStarted(QString,QVariantMap)),        this);
        connect4(kRootPath, kRootIface, "TransferProgress",
                 SLOT(onTransferProgress(QString,qlonglong,qlonglong)), this);
        connect4(kRootPath, kRootIface, "TransferCompleted",
                 SLOT(onTransferCompleted(QString,bool,QString)),     this);
        connect4(kRootPath, kRootIface, "TransferIncomingRequest",
                 SLOT(onTransferIncomingRequest(QString,QString,QVariantMap)), this);
        connect4(kNearbyPath, kNearbyIface, "DeviceFound",
                 SLOT(onDeviceFound(QString,QVariantMap)),            this);
        connect4(kNearbyPath, kNearbyIface, "DeviceLost",
                 SLOT(onDeviceLost(QString)),                         this);
        connect4(kNearbyPath, kNearbyIface, "DeviceUpdated",
                 SLOT(onDeviceUpdated(QString,QVariantMap)),          this);
        connect4(kTrustPath,  kTrustIface,  "DeviceTrustChanged",
                 SLOT(onDeviceTrustChanged(QString,QString)),         this);
        connect4(kTrustPath,  kTrustIface,  "PairingRequested",
                 SLOT(onPairingRequested(QString,QVariantMap)),       this);
    }
    return nowAvail;
}

void SharingServiceClient::applyFeatureStates(const QVariantMap &features)
{
    const auto b = [&](const QString &key) { return features.value(key, false).toBool(); };
    m_fileSharingEnabled      = b(QStringLiteral("file-sharing"));
    m_nearbySharingEnabled    = b(QStringLiteral("nearby-sharing"));
    m_screenSharingEnabled    = b(QStringLiteral("screen-sharing"));
    m_printerSharingEnabled   = b(QStringLiteral("printer-sharing"));
    m_remoteAccessEnabled     = b(QStringLiteral("remote-access"));
    m_mediaSharingEnabled     = b(QStringLiteral("media-sharing"));
    m_clipboardSharingEnabled = b(QStringLiteral("clipboard-sharing"));
    emit featureStatesChanged();
}

void SharingServiceClient::refreshNearbyDevices()
{
    if (!m_nearbyIface || !m_nearbyIface->isValid())
        return;
    QDBusReply<QVariantMap> reply = m_nearbyIface->call(QStringLiteral("GetDevices"));
    if (!reply.isValid())
        return;
    const QVariantList devices = normalizeDbusMap(reply.value())
                                     .value(QStringLiteral("devices")).toList();
    if (m_nearbyDevices != devices) {
        m_nearbyDevices = devices;
        emit nearbyDevicesChanged();
    }
}

void SharingServiceClient::refreshTrustedDevices()
{
    if (!m_trustIface || !m_trustIface->isValid())
        return;
    QDBusReply<QVariantMap> reply = m_trustIface->call(QStringLiteral("GetTrustedDevices"));
    if (!reply.isValid())
        return;
    const QVariantList devices = normalizeDbusMap(reply.value())
                                     .value(QStringLiteral("devices")).toList();
    if (m_trustedDevices != devices) {
        m_trustedDevices = devices;
        emit trustedDevicesChanged();
    }
}

void SharingServiceClient::refreshSharedFolders()
{
    if (!m_iface || !m_iface->isValid())
        return;
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ListSharedFolders"));
    if (!reply.isValid())
        return;
    const QVariantMap foldersMap = normalizeDbusMap(reply.value())
                                       .value(QStringLiteral("folders")).toMap();
    QVariantList folders;
    for (auto it = foldersMap.begin(); it != foldersMap.end(); ++it) {
        QVariantMap entry = it.value().toMap();
        entry.insert(QStringLiteral("path"), it.key());
        folders.append(entry);
    }
    if (m_sharedFolders != folders) {
        m_sharedFolders = folders;
        emit sharedFoldersChanged();
    }
}

void SharingServiceClient::refreshActiveTransfers()
{
    if (!m_iface || !m_iface->isValid())
        return;
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ListActiveTransfers"));
    if (!reply.isValid())
        return;
    const QVariantList transfers = normalizeDbusMap(reply.value())
                                       .value(QStringLiteral("transfers")).toList();
    if (m_activeTransfers != transfers) {
        m_activeTransfers = transfers;
        emit activeTransfersChanged();
    }
}
