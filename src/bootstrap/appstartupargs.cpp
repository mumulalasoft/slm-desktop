#include "appstartupargs.h"

#include <QRegularExpression>

AppStartupArgs parseAppStartupArgs(const QStringList &arguments)
{
    AppStartupArgs out;
    out.startWindowed = arguments.contains(QStringLiteral("--windowed"));

    const QRegularExpression windowSizeRe(QStringLiteral("^--window-size=(\\d{2,5})[xX](\\d{2,5})$"));
    for (const QString &arg : arguments) {
        const QRegularExpressionMatch m = windowSizeRe.match(arg);
        if (!m.hasMatch()) {
            continue;
        }
        out.windowWidth = m.captured(1).toInt();
        out.windowHeight = m.captured(2).toInt();
        break;
    }
    return out;
}

