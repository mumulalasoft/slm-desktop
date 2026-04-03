#include "windowingbackendmanager.h"

#include "kwinwaylandipcclient.h"
#include "kwinwaylandstatemodel.h"

namespace {
QString normalizedEventToken(const QString &value)
{
    return value.trimmed().toLower().replace(QLatin1Char('_'), QLatin1Char('-'));
}

QString normalizeBackendKey(const QString &value)
{
    const QString raw = value.trimmed().toLower();
    if (raw == QStringLiteral("kwin")
            || raw == QStringLiteral("kwin-wayland")
            || raw == QStringLiteral("kde")
            || raw.isEmpty()) {
        return QStringLiteral("kwin-wayland");
    }
    return QStringLiteral("kwin-wayland");
}

QVariantMap kwinWaylandCapabilities()
{
    return {
        { QStringLiteral("window-list"), true },
        { QStringLiteral("spaces"), true },
        { QStringLiteral("switcher"), true },
        { QStringLiteral("overview"), true },
        { QStringLiteral("workspace"), true },
        { QStringLiteral("progress-overlay"), false },
        { QStringLiteral("command.switcher-next"), true },
        { QStringLiteral("command.switcher-prev"), true },
        { QStringLiteral("command.overview"), true },
        { QStringLiteral("command.workspace"), true },
        { QStringLiteral("command.show-desktop"), true },
        { QStringLiteral("command.space-set"), true },
        { QStringLiteral("command.space-count"), true },
        { QStringLiteral("command.focus-view"), true },
        { QStringLiteral("command.close-view"), true },
        { QStringLiteral("command.inputcapture.enable"), false },
        { QStringLiteral("command.inputcapture.disable"), false },
        { QStringLiteral("command.inputcapture.release"), false },
        { QStringLiteral("command.inputcapture.set-barriers"), false },
        { QStringLiteral("command.progress-hide"), false },
    };
}

QString mappedWorkspaceEventFromCommand(const QString &event, const QVariantMap &payload)
{
    if (normalizedEventToken(event) != QStringLiteral("command")) {
        return QString();
    }
    if (!payload.value(QStringLiteral("ok")).toBool()) {
        return QString();
    }

    const QString command = payload.value(QStringLiteral("command")).toString().trimmed().toLower();
    if (command == QStringLiteral("workspace on") || command == QStringLiteral("overview on")) {
        return QStringLiteral("workspace-open");
    }
    if (command == QStringLiteral("workspace off") || command == QStringLiteral("overview off")) {
        return QStringLiteral("workspace-close");
    }
    if (command == QStringLiteral("workspace toggle") || command == QStringLiteral("overview toggle")) {
        return QStringLiteral("workspace-toggle");
    }
    return QString();
}

QString canonicalLifecycleEvent(const QString &event, const QVariantMap &payload)
{
    const QString eventFromPayload = normalizedEventToken(payload.value(QStringLiteral("event")).toString());
    const QString base = eventFromPayload.isEmpty() ? normalizedEventToken(event) : eventFromPayload;
    if (base.isEmpty()) {
        return QString();
    }

    if (base == QStringLiteral("window-activated")
            || base == QStringLiteral("focus-changed")
            || base == QStringLiteral("active-window-changed")) {
        return QStringLiteral("window-focused");
    }
    if (base == QStringLiteral("window-deactivated")) {
        return QStringLiteral("window-unfocused");
    }
    if (base == QStringLiteral("window-opened")
            || base == QStringLiteral("window-added")
            || base == QStringLiteral("window-shown")) {
        return QStringLiteral("window-created");
    }
    if (base == QStringLiteral("window-removed")
            || base == QStringLiteral("window-destroyed")
            || base == QStringLiteral("window-closing")) {
        return QStringLiteral("window-closed");
    }
    if (base == QStringLiteral("window-restored")) {
        return QStringLiteral("window-unminimized");
    }
    if (base == QStringLiteral("workspace-open")
            || base == QStringLiteral("workspace-close")
            || base == QStringLiteral("workspace-toggle")
            || base == QStringLiteral("window-created")
            || base == QStringLiteral("window-closed")
            || base == QStringLiteral("window-minimized")
            || base == QStringLiteral("window-unminimized")
            || base == QStringLiteral("window-focused")
            || base == QStringLiteral("window-unfocused")) {
        return base;
    }
    return QString();
}
}

WindowingBackendManager::WindowingBackendManager(QObject *parent)
    : QObject(parent)
    , m_kwinIpc(new KWinWaylandIpcClient(this))
    , m_kwinState(new KWinWaylandStateModel(this))
{
    m_kwinState->setIpcClient(m_kwinIpc);
    wireSignals();
}

WindowingBackendManager::~WindowingBackendManager() = default;

QVariantMap WindowingBackendManager::baseCapabilitiesForBackend(const QString &backend)
{
    Q_UNUSED(backend);
    return kwinWaylandCapabilities();
}

QString WindowingBackendManager::backend() const
{
    return m_backend;
}

bool WindowingBackendManager::connected() const
{
    return m_kwinIpc != nullptr && m_kwinIpc->connected();
}

QVariantMap WindowingBackendManager::capabilities() const
{
    QVariantMap caps = baseCapabilitiesForBackend(m_backend);
    const bool inputCaptureSupported = m_kwinIpc && m_kwinIpc->supportsInputCaptureCommands();
    caps.insert(QStringLiteral("command.inputcapture.set-barriers"), inputCaptureSupported);
    caps.insert(QStringLiteral("command.inputcapture.enable"), inputCaptureSupported);
    caps.insert(QStringLiteral("command.inputcapture.disable"), inputCaptureSupported);
    caps.insert(QStringLiteral("command.inputcapture.release"), inputCaptureSupported);
    return caps;
}

int WindowingBackendManager::protocolVersion() const
{
    return -1;
}

int WindowingBackendManager::eventSchemaVersion() const
{
    return -1;
}

QStringList WindowingBackendManager::availableBackends() const
{
    return { QStringLiteral("kwin-wayland") };
}

QString WindowingBackendManager::canonicalizeBackend(const QString &value) const
{
    return normalizeBackendKey(value);
}

bool WindowingBackendManager::configureBackend(const QString &requestedBackend)
{
    const QString next = canonicalizeBackend(requestedBackend);
    if (next == m_backend) {
        return true;
    }
    m_backend = next;
    emit backendChanged();
    emit connectedChanged();
    emit capabilitiesChanged();
    emit protocolVersionChanged();
    emit eventSchemaVersionChanged();
    return true;
}

bool WindowingBackendManager::sendCommand(const QString &command)
{
    return m_kwinIpc != nullptr && m_kwinIpc->sendCommand(command);
}

QObject *WindowingBackendManager::compositorStateObject() const
{
    return m_kwinState;
}

bool WindowingBackendManager::hasCapability(const QString &name) const
{
    const QString key = name.trimmed();
    if (key.isEmpty()) {
        return false;
    }
    return capabilities().value(key).toBool();
}

QVariantMap WindowingBackendManager::setInputCapturePointerBarriers(const QString &sessionPath,
                                                                    const QVariantList &barriers,
                                                                    const QVariantMap &options)
{
    if (!m_kwinIpc) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("windowing-backend-unavailable")},
        };
    }
    return m_kwinIpc->setInputCapturePointerBarriers(sessionPath, barriers, options);
}

QVariantMap WindowingBackendManager::enableInputCaptureSession(const QString &sessionPath,
                                                               const QVariantMap &options)
{
    if (!m_kwinIpc) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("windowing-backend-unavailable")},
        };
    }
    return m_kwinIpc->enableInputCaptureSession(sessionPath, options);
}

QVariantMap WindowingBackendManager::disableInputCaptureSession(const QString &sessionPath,
                                                                const QVariantMap &options)
{
    if (!m_kwinIpc) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("windowing-backend-unavailable")},
        };
    }
    return m_kwinIpc->disableInputCaptureSession(sessionPath, options);
}

QVariantMap WindowingBackendManager::releaseInputCaptureSession(const QString &sessionPath,
                                                                const QString &reason)
{
    if (!m_kwinIpc) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("windowing-backend-unavailable")},
        };
    }
    return m_kwinIpc->releaseInputCaptureSession(sessionPath, reason);
}

void WindowingBackendManager::wireSignals()
{
    connect(m_kwinIpc, &KWinWaylandIpcClient::connectedChanged, this, [this]() {
        emit connectedChanged();
    });
    connect(m_kwinIpc, &KWinWaylandIpcClient::eventReceived, this,
            [this](const QString &event, const QVariantMap &payload) {
        emit eventReceived(event, payload);
        const QString mapped = mappedWorkspaceEventFromCommand(event, payload);
        if (!mapped.isEmpty()) {
            QVariantMap mappedPayload = payload;
            mappedPayload.insert(QStringLiteral("event"), mapped);
            mappedPayload.insert(QStringLiteral("legacy_event_alias"),
                                 mapped == QStringLiteral("workspace-open")
                                     ? QStringLiteral("overview-open")
                                     : (mapped == QStringLiteral("workspace-close")
                                            ? QStringLiteral("overview-close")
                                            : QStringLiteral("overview-toggle")));
            emit eventReceived(mapped, mappedPayload);
        }

        const QString canonical = canonicalLifecycleEvent(event, payload);
        if (canonical.isEmpty()) {
            return;
        }
        const QString payloadEvent = normalizedEventToken(payload.value(QStringLiteral("event")).toString());
        const QString sourceEvent = normalizedEventToken(event);
        if (canonical == payloadEvent && canonical == sourceEvent) {
            return;
        }
        QVariantMap canonicalPayload = payload;
        canonicalPayload.insert(QStringLiteral("event"), canonical);
        canonicalPayload.insert(QStringLiteral("canonicalized"), true);
        emit eventReceived(canonical, canonicalPayload);
    });
}
