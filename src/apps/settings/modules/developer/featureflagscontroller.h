#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// FeatureFlagsController — manages the experimental/preview feature flag registry.
//
// Flags are hardcoded in the registry; only their enabled state is persisted
// to ~/.config/slm/feature-flags.conf.
//
// Exposed to QML as the "FeatureFlags" context property.
//
class FeatureFlagsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList flags READ flags NOTIFY flagsChanged)

public:
    explicit FeatureFlagsController(QObject *parent = nullptr);

    // Returns the full flag list as [{id, name, description, badge, effectWhen, enabled}]
    QVariantList flags() const { return m_flags; }

    Q_INVOKABLE bool isEnabled(const QString &id) const;
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void resetAll();

signals:
    void flagsChanged();

private:
    struct FlagDef {
        QString id;
        QString name;
        QString description;
        QString badge;       // "preview" | "experimental" | "deprecated"
        QString effectWhen;  // "immediately" | "next-launch" | "restart-compositor" | "restart-session"
        bool    defaultEnabled = false;
    };

    void initRegistry();
    void load();
    void save();

    QList<FlagDef> m_registry;
    QVariantList   m_flags;
    QString        m_configPath;

    void rebuild();
};
