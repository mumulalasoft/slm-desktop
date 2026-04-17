#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class DesktopSettingsClient;

// FontManager enumerates system fonts via QFontDatabase, exposes them to QML,
// stores the four font-role selections via DesktopSettings, and applies the active
// selection to ~/.config/gtk-3.0/settings.ini, gtk-4.0/settings.ini, and
// ~/.config/kdeglobals whenever a selection or the theme mode changes.
//
// Font specs are stored as "Family,Style,Size", e.g. "Noto Sans,Regular,11".
// An empty spec means "use the system default".
class FontManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString defaultFont   READ defaultFont   NOTIFY defaultFontChanged)
    Q_PROPERTY(QString documentFont  READ documentFont  NOTIFY documentFontChanged)
    Q_PROPERTY(QString monospaceFont READ monospaceFont NOTIFY monospaceFontChanged)
    Q_PROPERTY(QString titlebarFont  READ titlebarFont  NOTIFY titlebarFontChanged)
    Q_PROPERTY(int defaultFontSize   READ defaultFontSize   NOTIFY defaultFontChanged)
    Q_PROPERTY(int documentFontSize  READ documentFontSize  NOTIFY documentFontChanged)
    Q_PROPERTY(int monospaceFontSize READ monospaceFontSize NOTIFY monospaceFontChanged)
    Q_PROPERTY(int titlebarFontSize  READ titlebarFontSize  NOTIFY titlebarFontChanged)

public:
    explicit FontManager(DesktopSettingsClient *desktopSettings,
                         QObject *parent = nullptr);

    QString defaultFont()   const;
    QString documentFont()  const;
    QString monospaceFont() const;
    QString titlebarFont()  const;
    int defaultFontSize() const;
    int documentFontSize() const;
    int monospaceFontSize() const;
    int titlebarFontSize() const;

    // Returns a flat, alphabetically sorted list of {family, style, weight, italic,
    // displayName} QVariantMaps, one entry per family+style combination.
    // Result is cached after the first call.
    Q_INVOKABLE QVariantList allFontEntries() const;

    // Returns the available styles for the given family (from QFontDatabase).
    Q_INVOKABLE QStringList stylesForFamily(const QString &family) const;

    // Converts a "Family,Style,Size" spec to a human-readable label,
    // e.g. "Noto Sans  Regular  11".
    Q_INVOKABLE QString displayLabel(const QString &spec) const;

    Q_INVOKABLE void setDefaultFont(const QString &spec);
    Q_INVOKABLE void setDocumentFont(const QString &spec);
    Q_INVOKABLE void setMonospaceFont(const QString &spec);
    Q_INVOKABLE void setTitlebarFont(const QString &spec);
    Q_INVOKABLE void setDefaultFontSize(int size);
    Q_INVOKABLE void setDocumentFontSize(int size);
    Q_INVOKABLE void setMonospaceFontSize(int size);
    Q_INVOKABLE void setTitlebarFontSize(int size);

    // Resets all four font roles to empty (system default) and re-applies.
    Q_INVOKABLE void resetToDefaults();

signals:
    void defaultFontChanged();
    void documentFontChanged();
    void monospaceFontChanged();
    void titlebarFontChanged();

private:
    void applyFonts();
    void connectSources();
    QString currentDefaultFont() const;
    QString currentDocumentFont() const;
    QString currentMonospaceFont() const;
    QString currentTitlebarFont() const;
    bool setFontByRole(const QString &rolePath, const QString &spec);

    DesktopSettingsClient *m_desktopSettings;
    // Converts "Family,Style,Size" → GTK Pango string, e.g. "Noto Sans Bold 11".
    static QString specToGtkString(const QString &spec);
    // Converts "Family,Style,Size" → Qt font spec string for kdeglobals,
    // e.g. "Noto Sans,11,-1,5,50,0,0,0,0,0".
    static QString specToQtFontString(const QString &spec);

    static void writeGtkFontSettings(const QString &defaultSpec);
    static void writeKdeFontSettings(const QString &defaultSpec,
                                     const QString &documentSpec,
                                     const QString &monospaceSpec,
                                     const QString &titlebarSpec);

    mutable QVariantList  m_fontCache;
    mutable bool          m_cacheBuilt = false;
};
