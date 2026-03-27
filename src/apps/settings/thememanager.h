#pragma once

#include <QObject>
#include <QStringList>

class UIPreferences;

// ThemeManager discovers GTK themes and KDE color schemes installed on the
// system via XDG data directories, exposes the available lists to QML, and
// applies the selected theme to the relevant system configuration files
// (~/.config/gtk-3.0/settings.ini, gtk-4.0/settings.ini, kdeglobals) so
// applications pick up the change immediately.
class ThemeManager : public QObject
{
    Q_OBJECT

    // Available installed themes — populated on construction and on refresh().
    Q_PROPERTY(QStringList gtkThemes READ gtkThemes NOTIFY gtkThemesChanged)
    Q_PROPERTY(QStringList kdeColorSchemes READ kdeColorSchemes NOTIFY kdeColorSchemesChanged)
    Q_PROPERTY(QStringList iconThemes READ iconThemes NOTIFY iconThemesChanged)

    // Active selections — delegated to UIPreferences for persistent storage.
    Q_PROPERTY(QString gtkThemeLight READ gtkThemeLight NOTIFY gtkThemeLightChanged)
    Q_PROPERTY(QString gtkThemeDark READ gtkThemeDark NOTIFY gtkThemeDarkChanged)
    Q_PROPERTY(QString kdeColorSchemeLight READ kdeColorSchemeLight NOTIFY kdeColorSchemeLightChanged)
    Q_PROPERTY(QString kdeColorSchemeDark READ kdeColorSchemeDark NOTIFY kdeColorSchemeDarkChanged)
    Q_PROPERTY(QString gtkIconThemeLight READ gtkIconThemeLight NOTIFY gtkIconThemeLightChanged)
    Q_PROPERTY(QString gtkIconThemeDark READ gtkIconThemeDark NOTIFY gtkIconThemeDarkChanged)
    Q_PROPERTY(QString kdeIconThemeLight READ kdeIconThemeLight NOTIFY kdeIconThemeLightChanged)
    Q_PROPERTY(QString kdeIconThemeDark READ kdeIconThemeDark NOTIFY kdeIconThemeDarkChanged)
    Q_PROPERTY(QString windowControlsSide READ windowControlsSide NOTIFY windowControlsSideChanged)

public:
    explicit ThemeManager(UIPreferences *prefs, QObject *parent = nullptr);

    QStringList gtkThemes() const;
    QStringList kdeColorSchemes() const;
    QStringList iconThemes() const;

    QString gtkThemeLight() const;
    QString gtkThemeDark() const;
    QString kdeColorSchemeLight() const;
    QString kdeColorSchemeDark() const;
    QString gtkIconThemeLight() const;
    QString gtkIconThemeDark() const;
    QString kdeIconThemeLight() const;
    QString kdeIconThemeDark() const;
    QString windowControlsSide() const;

    Q_INVOKABLE void setGtkThemeLight(const QString &theme);
    Q_INVOKABLE void setGtkThemeDark(const QString &theme);
    Q_INVOKABLE void setKdeColorSchemeLight(const QString &scheme);
    Q_INVOKABLE void setKdeColorSchemeDark(const QString &scheme);
    Q_INVOKABLE void setGtkIconThemeLight(const QString &theme);
    Q_INVOKABLE void setGtkIconThemeDark(const QString &theme);
    Q_INVOKABLE void setKdeIconThemeLight(const QString &theme);
    Q_INVOKABLE void setKdeIconThemeDark(const QString &theme);
    Q_INVOKABLE void setWindowControlsSide(const QString &side);

    // Re-scan XDG directories for newly installed themes.
    Q_INVOKABLE void refresh();

signals:
    void gtkThemesChanged();
    void kdeColorSchemesChanged();
    void iconThemesChanged();
    void gtkThemeLightChanged();
    void gtkThemeDarkChanged();
    void kdeColorSchemeLightChanged();
    void kdeColorSchemeDarkChanged();
    void gtkIconThemeLightChanged();
    void gtkIconThemeDarkChanged();
    void kdeIconThemeLightChanged();
    void kdeIconThemeDarkChanged();
    void windowControlsSideChanged();
    void appearanceChanged();

private:
    // Returns the list of XDG data directories to search (XDG_DATA_HOME first,
    // then each entry in XDG_DATA_DIRS).
    static QStringList xdgDataDirs();

    static QStringList scanGtkThemes();
    static QStringList scanKdeColorSchemes();
    static QStringList scanIconThemes();

    // Write gtk-theme-name / gtk-icon-theme-name to ~/.config/gtk-{3,4}.0/settings.ini.
    static void applyGtkTheme(const QString &themeName);
    static void applyGtkIconTheme(const QString &themeName);
    // Write ColorScheme to ~/.config/kdeglobals [General].
    static void applyKdeColorScheme(const QString &schemeName);
    // Write icon theme to ~/.config/kdeglobals [Icons].
    static void applyKdeIconTheme(const QString &themeName);
    static void applyWindowControlsLayout(const QString &side);

    void connectPrefs();

    UIPreferences *m_prefs;
    QStringList m_gtkThemes;
    QStringList m_kdeColorSchemes;
    QStringList m_iconThemes;
};
