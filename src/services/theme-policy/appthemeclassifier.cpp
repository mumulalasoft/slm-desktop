#include "appthemeclassifier.h"

#include <QFileInfo>

namespace Slm::ThemePolicy {

namespace {

QString normalizedKey(const QString &raw)
{
    return raw.trimmed().toLower();
}

bool containsAnyToken(const QStringList &haystack, const QStringList &needles)
{
    for (const QString &item : haystack) {
        const QString token = normalizedKey(item);
        for (const QString &needle : needles) {
            if (token == normalizedKey(needle)) {
                return true;
            }
        }
    }
    return false;
}

AppThemeCategory findOverride(const AppDescriptor &app, const ClassificationRules &rules)
{
    const QString appId = normalizedKey(app.appId);
    const QString desktopId = normalizedKey(app.desktopFileId);
    const QString execBase = normalizedKey(QFileInfo(app.executable).fileName());

    if (!appId.isEmpty() && rules.byAppId.contains(appId)) {
        return rules.byAppId.value(appId);
    }
    if (!desktopId.isEmpty() && rules.byDesktopFileId.contains(desktopId)) {
        return rules.byDesktopFileId.value(desktopId);
    }
    if (!execBase.isEmpty() && rules.byExecutable.contains(execBase)) {
        return rules.byExecutable.value(execBase);
    }
    return AppThemeCategory::Unknown;
}

} // namespace

AppThemeCategory AppThemeClassifier::classify(const AppDescriptor &app,
                                              const ClassificationRules &rules)
{
    if (const AppThemeCategory ov = findOverride(app, rules);
            ov != AppThemeCategory::Unknown) {
        return ov;
    }

    const QString appId = normalizedKey(app.appId);
    const QString desktopId = normalizedKey(app.desktopFileId);
    const QString execBase = normalizedKey(QFileInfo(app.executable).fileName());

    if (appId.startsWith(QStringLiteral("org.slm.desktop"))
            || desktopId.startsWith(QStringLiteral("slm-desktop"))
            || execBase == QStringLiteral("slm-desktop")) {
        return AppThemeCategory::Shell;
    }

    if (containsAnyToken(app.categories, {QStringLiteral("kde"), QStringLiteral("plasma")})
            || containsAnyToken(app.desktopKeys, {QStringLiteral("x-kde-serviceTypes"),
                                                  QStringLiteral("x-kde-library")})) {
        return AppThemeCategory::Kde;
    }

    if (containsAnyToken(app.categories, {QStringLiteral("gnome"),
                                          QStringLiteral("gtk"),
                                          QStringLiteral("xfce"),
                                          QStringLiteral("mate"),
                                          QStringLiteral("lxqt")})) {
        return AppThemeCategory::Gtk;
    }

    if (containsAnyToken(app.categories, {QStringLiteral("qt"), QStringLiteral("qt5"), QStringLiteral("qt6")})
            || execBase.contains(QStringLiteral("qt"))) {
        return AppThemeCategory::QtGeneric;
    }

    return AppThemeCategory::Unknown;
}

QString AppThemeClassifier::categoryToString(AppThemeCategory category)
{
    switch (category) {
    case AppThemeCategory::Shell:
        return QStringLiteral("shell");
    case AppThemeCategory::Gtk:
        return QStringLiteral("gtk");
    case AppThemeCategory::Kde:
        return QStringLiteral("kde");
    case AppThemeCategory::QtGeneric:
        return QStringLiteral("qt-generic");
    case AppThemeCategory::QtDesktopFallback:
        return QStringLiteral("qt-desktop-fallback");
    case AppThemeCategory::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

AppThemeCategory AppThemeClassifier::categoryFromString(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("shell")) {
        return AppThemeCategory::Shell;
    }
    if (v == QStringLiteral("gtk")) {
        return AppThemeCategory::Gtk;
    }
    if (v == QStringLiteral("kde")) {
        return AppThemeCategory::Kde;
    }
    if (v == QStringLiteral("qt-generic") || v == QStringLiteral("qt_generic")) {
        return AppThemeCategory::QtGeneric;
    }
    if (v == QStringLiteral("qt-desktop-fallback")
            || v == QStringLiteral("qt_desktop_fallback")) {
        return AppThemeCategory::QtDesktopFallback;
    }
    return AppThemeCategory::Unknown;
}

} // namespace Slm::ThemePolicy
