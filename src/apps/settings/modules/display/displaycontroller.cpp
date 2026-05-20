#include "displaycontroller.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

DisplayController::DisplayController(QObject *parent)
    : QObject(parent)
{
}

QVariantList DisplayController::availableResolutions() const
{
    QVariantList list;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return list;

    // In a real system, we'd query the compositor for supported modes.
    // For now, we'll provide common resolutions based on the current aspect ratio.
    const QSize current = screen->size();
    const double ratio = static_cast<double>(current.width()) / current.height();

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

    // Current first
    addRes(current.width(), current.height());
    
    // Some defaults if not present
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
    // In a real implementation, this would call a DBus service or compositor API.
}

void DisplayController::applyScaling(double scale)
{
    qDebug() << "[DisplayController] Applying scaling:" << scale;
    // This could update a global setting that the shell listens to.
}
