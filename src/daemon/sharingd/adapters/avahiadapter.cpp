#include "avahiadapter.h"

#include <QStandardPaths>

AvahiAdapter::AvahiAdapter(QObject *parent)
    : ISharingAdapter(parent)
{
    m_restartTimer.setSingleShot(true);
    m_restartTimer.setInterval(5000);
    connect(&m_restartTimer, &QTimer::timeout, this, [this] {
        if (m_active)
            startBrowse(QStringLiteral("_slm-sharing._tcp"));
    });
}

AvahiAdapter::~AvahiAdapter()
{
    deactivate();
}

ISharingAdapter::Status AvahiAdapter::probe()
{
    if (!avahiBrowseAvailable())
        return Status::Unavailable;
    return Status::Available;
}

bool AvahiAdapter::activate()
{
    if (probe() == Status::Unavailable)
        return false;
    m_active = true;
    startBrowse(QStringLiteral("_slm-sharing._tcp"));
    emit statusChanged(Status::Available);
    return true;
}

bool AvahiAdapter::deactivate()
{
    m_active = false;
    stopBrowse();
    emit statusChanged(Status::Unavailable);
    return true;
}

bool AvahiAdapter::recover()
{
    emit capabilityEvent(QStringLiteral("recovery-suggestion"), {
        {QStringLiteral("issue"), QStringLiteral("avahi-not-found")},
        {QStringLiteral("package"), QStringLiteral("avahi-daemon")},
        {QStringLiteral("message"), QStringLiteral("Nearby device discovery requires Avahi.")},
    });
    return false;
}

QVariantMap AvahiAdapter::statusDetail() const
{
    return {
        {QStringLiteral("avahiBrowseAvailable"), avahiBrowseAvailable()},
        {QStringLiteral("active"), m_active},
        {QStringLiteral("browseRunning"), m_browseProcess && m_browseProcess->state() == QProcess::Running},
    };
}

bool AvahiAdapter::announceService(const QString &serviceType, const QString &name,
                                    quint16 port, const QVariantMap &txtRecord)
{
    if (!m_active)
        return false;

    QStringList args = {
        QStringLiteral("-s"), name,
        serviceType,
        QString::number(port),
    };
    for (auto it = txtRecord.begin(); it != txtRecord.end(); ++it)
        args << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());

    QProcess p;
    p.start(QStringLiteral("avahi-publish"), args);
    return p.waitForStarted(2000);
}

bool AvahiAdapter::withdrawService(const QString &serviceType, const QString &name)
{
    Q_UNUSED(serviceType)
    Q_UNUSED(name)
    // avahi-publish exits when its process ends; withdrawing means killing the tracked process
    return true;
}

void AvahiAdapter::startBrowse(const QString &serviceType)
{
    stopBrowse();

    m_browseProcess = new QProcess(this);
    connect(m_browseProcess, &QProcess::readyReadStandardOutput, this, &AvahiAdapter::onBrowseOutput);
    connect(m_browseProcess, &QProcess::readyReadStandardError, this, &AvahiAdapter::onBrowseError);
    connect(m_browseProcess, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (m_active)
            m_restartTimer.start();
    });

    m_browseProcess->start(QStringLiteral("avahi-browse"), {
        QStringLiteral("-r"), QStringLiteral("-p"), serviceType
    });
}

void AvahiAdapter::stopBrowse()
{
    m_restartTimer.stop();
    if (m_browseProcess) {
        m_browseProcess->kill();
        m_browseProcess->waitForFinished(1000);
        m_browseProcess->deleteLater();
        m_browseProcess = nullptr;
    }
}

void AvahiAdapter::onBrowseOutput()
{
    if (!m_browseProcess)
        return;
    const auto lines = QString::fromLocal8Bit(m_browseProcess->readAllStandardOutput())
                           .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto &line : lines)
        parseBrowseLine(line);
}

void AvahiAdapter::onBrowseError()
{
    // Silently drain stderr to prevent blocking
    if (m_browseProcess)
        m_browseProcess->readAllStandardError();
}

void AvahiAdapter::parseBrowseLine(const QString &line)
{
    // avahi-browse -p output: =;iface;protocol;name;type;domain;hostname;address;port;txt...
    const auto parts = line.split(QLatin1Char(';'));
    if (parts.size() < 9)
        return;

    const QString event = parts[0]; // '=' found, '+' new, '-' removed
    const QString name = parts[3];
    const QString address = parts[7];
    const int port = parts[8].toInt();

    if (event == QLatin1String("=")) {
        emit capabilityEvent(QStringLiteral("device-resolved"), {
            {QStringLiteral("name"), name},
            {QStringLiteral("address"), address},
            {QStringLiteral("port"), port},
        });
    } else if (event == QLatin1String("+")) {
        emit capabilityEvent(QStringLiteral("device-found"), {
            {QStringLiteral("name"), name},
        });
    } else if (event == QLatin1String("-")) {
        emit capabilityEvent(QStringLiteral("device-lost"), {
            {QStringLiteral("name"), name},
        });
    }
}

bool AvahiAdapter::avahiBrowseAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("avahi-browse")).isEmpty();
}

bool AvahiAdapter::avahiPublishAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("avahi-publish")).isEmpty();
}
