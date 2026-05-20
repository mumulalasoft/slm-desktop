#include "displaycontroller.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QDBusInterface>
#include <QDBusConnection>

DisplayController::DisplayController(QObject *parent)
    : QObject(parent)
{
}

QVariantList DisplayController::availableResolutions() const
{
    QVariantList list;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return list;

    const QSize current = screen->size();
    auto addRes = [&](int w, int h) {
        QString label = QStringLiteral("%1 x %2").arg(w).arg(h);
        if (qAbs((static_cast<double>(w)/h) - (16.0/10.0)) < 0.01) label += QStringLiteral(" (16:10)");
        else if (qAbs((static_cast<double>(w)/h) - (16.0/9.0)) < 0.01) label += QStringLiteral(" (16:9)");
        
        list.append(QVariantMap{
            {QStringLiteral("label"), label},
            {QStringLiteral("width"), w},
            {QStringLiteral("height"), h}
        });
    };

    addRes(current.width(), current.height());
    if (current.width() != 2560) addRes(2560, 1600);
    if (current.width() != 1920) addRes(1920, 1200);
    if (current.width() != 1680) addRes(1680, 1050);
    if (current.width() != 1440) addRes(1440, 900);

    return list;
}

QVariantList DisplayController::availableScales() const
{
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("100%")}, {QStringLiteral("scale"), 1.0}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("125%")}, {QStringLiteral("scale"), 1.25}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("150%")}, {QStringLiteral("scale"), 1.5}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("200%")}, {QStringLiteral("scale"), 2.0}}
    };
}

void DisplayController::applyResolution(const QString &resolution)
{
    qDebug() << "[DisplayController] Applying resolution:" << resolution;
}

void DisplayController::applyScaling(double scale)
{
    qDebug() << "[DisplayController] Applying scaling:" << scale;
    
    QDBusInterface settingsIface(QStringLiteral("org.slm.Desktop.Settings"),
                                 QStringLiteral("/org/slm/Desktop/Settings"),
                                 QStringLiteral("org.slm.Desktop.Settings"),
                                 QDBusConnection::sessionBus());
                                 
    if (settingsIface.isValid()) {
        settingsIface.call(QStringLiteral("SetSettingValue"), QStringLiteral("display/scaling"), scale);
    }
}
