#include <QtTest/QtTest>

#include "../src/services/theme-policy/appthemeclassifier.h"

using namespace Slm::ThemePolicy;

class AppThemeClassifierTest : public QObject
{
    Q_OBJECT

private slots:
    void classifies_shell()
    {
        AppDescriptor app;
        app.appId = QStringLiteral("org.slm.desktop.filemanager");
        QCOMPARE(AppThemeClassifier::classify(app), AppThemeCategory::Shell);
    }

    void classifies_kde_from_categories()
    {
        AppDescriptor app;
        app.categories = {QStringLiteral("Utility"), QStringLiteral("KDE")};
        QCOMPARE(AppThemeClassifier::classify(app), AppThemeCategory::Kde);
    }

    void classifies_gtk_from_categories()
    {
        AppDescriptor app;
        app.categories = {QStringLiteral("GNOME"), QStringLiteral("Graphics")};
        QCOMPARE(AppThemeClassifier::classify(app), AppThemeCategory::Gtk);
    }

    void classifies_qt_generic_from_exec()
    {
        AppDescriptor app;
        app.executable = QStringLiteral("/usr/bin/qtcreator");
        QCOMPARE(AppThemeClassifier::classify(app), AppThemeCategory::QtGeneric);
    }

    void respects_manual_override()
    {
        AppDescriptor app;
        app.appId = QStringLiteral("org.example.editor");
        app.categories = {QStringLiteral("KDE")};

        ClassificationRules rules;
        rules.byAppId.insert(QStringLiteral("org.example.editor"),
                             AppThemeCategory::QtDesktopFallback);

        QCOMPARE(AppThemeClassifier::classify(app, rules),
                 AppThemeCategory::QtDesktopFallback);
    }
};

QTEST_MAIN(AppThemeClassifierTest)
#include "appthemeclassifier_test.moc"

