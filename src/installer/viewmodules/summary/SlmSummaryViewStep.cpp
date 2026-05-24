#include "SlmSummaryViewStep.h"

#include <utils/Logger.h>
#include <ViewManager.h>

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QUrl>
#include <QtQml>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmSummaryViewStepFactory,
                                    registerPlugin<SlmSummaryViewStep>();)

SlmSummaryViewStep::SlmSummaryViewStep(QObject *parent)
    : Calamares::ViewStep(parent)
{
    // The Install button on the summary screen IS the irreversible commit.
    // When the user clicks it, the Config writes slm.target.confirmed=true
    // and emits commitRequested; the ViewStep enables next, then asks the
    // ViewManager to advance — making the screen's button the single
    // canonical confirmation, with no possibility of advancing via
    // Calamares' chrome button while slm.target.confirmed is still unset.
    connect(&m_config, &Slm::Installer::SummaryConfig::commitRequested,
            this, [this]() {
                m_committed = true;
                emit nextStatusChanged(true);
                if (auto *vm = Calamares::ViewManager::instance()) {
                    vm->next();
                }
            });
}

SlmSummaryViewStep::~SlmSummaryViewStep() = default;

QString SlmSummaryViewStep::prettyName() const
{
    return tr("Confirm installation");
}

QWidget *SlmSummaryViewStep::widget()
{
    if (m_widget) {
        return m_widget;
    }

    m_widget = new QQuickWidget();
    m_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_widget->engine()->addImportPath(QStringLiteral("qrc:/qt/qml"));
    m_widget->rootContext()->setContextProperty(QStringLiteral("config"),
                                                &m_config);

    m_widget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/SlmInstaller/summary.qml")));

    if (m_widget->status() == QQuickWidget::Error) {
        for (const QQmlError &err : m_widget->errors()) {
            cWarning() << "slm-summary QML:" << err.toString();
        }
    }
    return m_widget;
}

bool SlmSummaryViewStep::isNextEnabled() const
{
    // Calamares' own Next button stays disabled until the user explicitly
    // clicks the in-screen Install action. This is the destructive
    // confirmation gate.
    return m_committed;
}

bool SlmSummaryViewStep::isBackEnabled() const
{
    return !m_committed;
}

bool SlmSummaryViewStep::isAtBeginning() const
{
    return true;  // single page
}

bool SlmSummaryViewStep::isAtEnd() const
{
    return true;  // single page
}

Calamares::JobList SlmSummaryViewStep::jobs() const
{
    return {};
}

void SlmSummaryViewStep::onActivate()
{
    m_committed = false;
    m_config.reload();
    emit nextStatusChanged(false);
}

void SlmSummaryViewStep::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
