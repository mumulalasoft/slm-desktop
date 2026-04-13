#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class BrightnessManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(int brightness READ brightness NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)

public:
    explicit BrightnessManager(QObject *parent = nullptr);

    bool available() const;
    int brightness() const;
    QString iconName() const;
    QString statusText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setBrightness(int brightness);

signals:
    void changed();

private:
    QString runProgram(const QString &program, const QStringList &args, int timeoutMs = 1200) const;
    bool parseBrightnessctlOutput(const QString &output);
    bool refreshWithBrightnessctl();
    bool refreshWithLight();

    bool m_available = false;
    int m_brightness = 0;
    QString m_iconName = QStringLiteral("display-brightness-symbolic");
    QString m_statusText = QStringLiteral("Display unavailable");
    QString m_preferredBacklightDevice;
    QTimer *m_timer = nullptr;
    bool m_hasBrightnessctl = false;
    bool m_hasLight = false;
};
