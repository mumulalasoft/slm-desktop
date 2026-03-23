#pragma once

#include <QObject>
#include <QTimer>

class BatteryManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(int percentage READ percentage NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(QString levelText READ levelText NOTIFY changed)
    Q_PROPERTY(QString durationText READ durationText NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString powerProfile READ powerProfile NOTIFY changed)
    Q_PROPERTY(bool canSetPowerProfile READ canSetPowerProfile NOTIFY changed)

public:
    explicit BatteryManager(QObject *parent = nullptr);

    bool available() const;
    int percentage() const;
    bool charging() const;
    QString levelText() const;
    QString durationText() const;
    QString iconName() const;
    QString powerProfile() const;
    bool canSetPowerProfile() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setPowerProfile(const QString &profile);
    Q_INVOKABLE bool openPowerSettings();

signals:
    void changed();

private:
    void updateBatteryState();
    void updatePowerProfile();
    QString formatDuration(qulonglong seconds) const;
    QString computeIconName() const;

    bool m_available = false;
    int m_percentage = 0;
    bool m_charging = false;
    QString m_levelText;
    QString m_durationText;
    QString m_iconName = QStringLiteral("battery-missing-symbolic");
    QString m_powerProfile = QStringLiteral("balanced");
    bool m_canSetPowerProfile = false;
    QTimer *m_timer = nullptr;
};

