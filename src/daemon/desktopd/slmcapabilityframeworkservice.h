#pragma once

#include "../../core/actions/framework/slmactionframework.h"

#include <QDBusContext>
#include <QObject>

class SlmCapabilityFrameworkService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.SLMCapabilities")

public:
    explicit SlmCapabilityFrameworkService(Slm::Actions::Framework::SlmActionFramework *framework,
                                           QObject *parent = nullptr);

public slots:
    QVariantList ListActions(const QString &capability, const QVariantMap &context);
    QVariantMap InvokeAction(const QString &actionId, const QVariantMap &context);
    QVariantList SearchActions(const QString &query, const QVariantMap &context);
    QVariantMap ResolveDefaultAction(const QString &capability, const QVariantMap &context);

private:
    Slm::Actions::Framework::SlmActionFramework *m_framework = nullptr;
};

