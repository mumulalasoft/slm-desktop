#include "SlmDiskSelectViewStep.h"

#include "SlmInstallerDiskModel.h"

#include <utils/Logger.h>

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QUrl>
#include <QtQml>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmDiskSelectViewStepFactory,
                                    registerPlugin<SlmDiskSelectViewStep>();)

SlmDiskSelectViewStep::SlmDiskSelectViewStep(QObject *parent)
    : Calamares::ViewStep(parent)
{
    // The disk model needs to be visible to QML as a registered type so
    // the `model:` binding can use a `Slm::Installer::SlmInstallerDiskModel*`
    // value. Registering uncreatable lets QML reference the type for casting
    // without allowing it to instantiate one (the ViewStep owns the instance).
    qmlRegisterUncreatableType<Slm::Installer::SlmInstallerDiskModel>(
        "Slm.Installer", 1, 0, "SlmInstallerDiskModel",
        QStringLiteral("Owned by the ViewStep — accessed via config.diskModel"));

    connect(&m_config, &Slm::Installer::DiskSelectConfig::selectedPathChanged,
            this, &SlmDiskSelectViewStep::emitNextStatus);
    connect(&m_config, &Slm::Installer::DiskSelectConfig::commitRequested,
            this, [this](const QString &) { emit nextStatusChanged(true); });
}

SlmDiskSelectViewStep::~SlmDiskSelectViewStep() = default;

QString SlmDiskSelectViewStep::prettyName() const
{
    return tr("Disk Selection");
}

QWidget *SlmDiskSelectViewStep::widget()
{
    if (m_widget) {
        return m_widget;
    }

    m_widget = new QQuickWidget();
    m_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // The QRC layout (resources.qrc) places all six SlmInstaller QML files
    // plus disk-select.qml under a single prefix so sibling resolution and
    // qmldir-driven singleton lookup just work for the loaded scene.
    m_widget->engine()->addImportPath(QStringLiteral("qrc:/qt/qml"));
    m_widget->rootContext()->setContextProperty(QStringLiteral("config"),
                                                &m_config);

    m_widget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/SlmInstaller/disk-select.qml")));

    if (m_widget->status() == QQuickWidget::Error) {
        for (const QQmlError &err : m_widget->errors()) {
            cWarning() << "slm-disk-select QML:" << err.toString();
        }
    }
    return m_widget;
}

bool SlmDiskSelectViewStep::isNextEnabled() const
{
    return !m_config.selectedPath().isEmpty();
}

bool SlmDiskSelectViewStep::isBackEnabled() const
{
    return true;
}

bool SlmDiskSelectViewStep::isAtBeginning() const
{
    // Single-page view step.
    return true;
}

bool SlmDiskSelectViewStep::isAtEnd() const
{
    // Single-page view step.
    return true;
}

Calamares::JobList SlmDiskSelectViewStep::jobs() const
{
    // No jobs of our own — the destructive partition/format/deploy jobs
    // are scheduled separately in settings.conf and read slm.target.disk
    // from GlobalStorage.
    return {};
}

void SlmDiskSelectViewStep::onActivate()
{
    // Refresh disk enumeration each time the user lands on this step, so
    // late-attached storage (USB sticks, hot-plugged drives) shows up.
    m_config.refresh();
}

void SlmDiskSelectViewStep::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}

void SlmDiskSelectViewStep::emitNextStatus()
{
    emit nextStatusChanged(isNextEnabled());
}
