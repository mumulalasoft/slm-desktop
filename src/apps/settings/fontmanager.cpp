#include "fontmanager.h"
#include "../../core/prefs/uipreferences.h"

#include <QDir>
#include <QFontDatabase>
#include <QSettings>
#include <QStandardPaths>

// ── Spec helpers ──────────────────────────────────────────────────────────────

// Parse "Family,Style,Size" into components.
static void parseSpec(const QString &spec, QString &family, QString &style, int &size)
{
    const QStringList parts = spec.split(u',');
    family = parts.value(0).trimmed();
    style  = parts.value(1, QStringLiteral("Regular")).trimmed();
    size   = parts.value(2, QStringLiteral("11")).toInt();
    if (size <= 0) size = 11;
}

// Map a style name to a QFont weight value.
static int styleToWeight(const QString &style)
{
    const QString s = style.toLower();
    if (s.contains(QLatin1String("black")) || s.contains(QLatin1String("heavy")))
        return 87;
    if (s.contains(QLatin1String("extrabold")) || s.contains(QLatin1String("ultra bold")))
        return 81;
    if (s.contains(QLatin1String("bold")))
        return 75;
    if (s.contains(QLatin1String("demibold")) || s.contains(QLatin1String("semibold")))
        return 63;
    if (s.contains(QLatin1String("medium")))
        return 57;
    if (s.contains(QLatin1String("light")))
        return 25;
    if (s.contains(QLatin1String("thin")))
        return 0;
    return 50; // Normal/Regular
}

static bool styleIsItalic(const QString &style)
{
    const QString s = style.toLower();
    return s.contains(QLatin1String("italic")) || s.contains(QLatin1String("oblique"));
}

// ── String converters ─────────────────────────────────────────────────────────

// Returns GTK Pango font description string, e.g. "Noto Sans Bold 11".
QString FontManager::specToGtkString(const QString &spec)
{
    if (spec.trimmed().isEmpty())
        return {};

    QString family, style;
    int size;
    parseSpec(spec, family, style, size);

    // Pango format: "Family [Style modifiers] Size"
    // If style is "Regular", omit it.
    if (style.compare(QLatin1String("Regular"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("%1 %2").arg(family).arg(size);
    return QStringLiteral("%1 %2 %3").arg(family, style).arg(size);
}

// Returns Qt font spec string for kdeglobals, e.g. "Noto Sans,11,-1,5,50,0,0,0,0,0".
QString FontManager::specToQtFontString(const QString &spec)
{
    if (spec.trimmed().isEmpty())
        return {};

    QString family, style;
    int size;
    parseSpec(spec, family, style, size);

    const int weight  = styleToWeight(style);
    const int italic  = styleIsItalic(style) ? 1 : 0;
    const int fixedPitch = (style.toLower().contains(QLatin1String("mono")) ||
                            family.toLower().contains(QLatin1String("mono"))) ? 1 : 0;

    return QStringLiteral("%1,%2,-1,5,%3,%4,0,0,%5,0")
        .arg(family).arg(size).arg(weight).arg(italic).arg(fixedPitch);
}

// ── Apply helpers ─────────────────────────────────────────────────────────────

void FontManager::writeGtkFontSettings(const QString &defaultSpec)
{
    const QString fontStr = specToGtkString(defaultSpec);
    if (fontStr.isEmpty())
        return;

    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    const QStringList gtkDirs = {
        configBase + QStringLiteral("/gtk-3.0"),
        configBase + QStringLiteral("/gtk-4.0"),
    };

    for (const QString &gtkDir : gtkDirs) {
        QDir().mkpath(gtkDir);
        QSettings ini(gtkDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Settings"));
        ini.setValue(QStringLiteral("gtk-font-name"), fontStr);
        ini.endGroup();
        ini.sync();
    }
}

void FontManager::writeKdeFontSettings(const QString &defaultSpec,
                                        const QString &monospaceSpec,
                                        const QString &titlebarSpec)
{
    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    QSettings kdeglobals(configBase + QStringLiteral("/kdeglobals"), QSettings::IniFormat);

    const QString defQt  = specToQtFontString(defaultSpec);
    const QString monoQt = specToQtFontString(monospaceSpec);
    const QString titleQt = specToQtFontString(titlebarSpec);

    kdeglobals.beginGroup(QStringLiteral("General"));
    if (!defQt.isEmpty())  kdeglobals.setValue(QStringLiteral("font"), defQt);
    if (!monoQt.isEmpty()) kdeglobals.setValue(QStringLiteral("fixed"), monoQt);
    kdeglobals.endGroup();

    if (!titleQt.isEmpty()) {
        kdeglobals.beginGroup(QStringLiteral("WM"));
        kdeglobals.setValue(QStringLiteral("activeFont"), titleQt);
        kdeglobals.endGroup();
    }

    kdeglobals.sync();
}

void FontManager::applyFonts()
{
    writeGtkFontSettings(m_prefs->defaultFont());
    writeKdeFontSettings(m_prefs->defaultFont(),
                         m_prefs->monospaceFont(),
                         m_prefs->titlebarFont());
}

// ── Constructor ───────────────────────────────────────────────────────────────

FontManager::FontManager(UIPreferences *prefs, QObject *parent)
    : QObject(parent)
    , m_prefs(prefs)
{
    connectPrefs();
}

void FontManager::connectPrefs()
{
    connect(m_prefs, &UIPreferences::defaultFontChanged,
            this, &FontManager::defaultFontChanged);
    connect(m_prefs, &UIPreferences::documentFontChanged,
            this, &FontManager::documentFontChanged);
    connect(m_prefs, &UIPreferences::monospaceFontChanged,
            this, &FontManager::monospaceFontChanged);
    connect(m_prefs, &UIPreferences::titlebarFontChanged,
            this, &FontManager::titlebarFontChanged);

    auto apply = [this]() { applyFonts(); };
    connect(m_prefs, &UIPreferences::defaultFontChanged,   this, apply);
    connect(m_prefs, &UIPreferences::monospaceFontChanged, this, apply);
    connect(m_prefs, &UIPreferences::titlebarFontChanged,  this, apply);
}

// ── Public API ────────────────────────────────────────────────────────────────

QString FontManager::defaultFont()   const { return m_prefs->defaultFont(); }
QString FontManager::documentFont()  const { return m_prefs->documentFont(); }
QString FontManager::monospaceFont() const { return m_prefs->monospaceFont(); }
QString FontManager::titlebarFont()  const { return m_prefs->titlebarFont(); }

QVariantList FontManager::allFontEntries() const
{
    if (m_cacheBuilt)
        return m_fontCache;

    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        const QStringList styles = QFontDatabase::styles(family);
        for (const QString &style : styles) {
            const QString displayName = style.compare(QLatin1String("Regular"),
                                                      Qt::CaseInsensitive) == 0
                                        ? family
                                        : QStringLiteral("%1 %2").arg(family, style);
            QVariantMap entry;
            entry[QStringLiteral("family")]      = family;
            entry[QStringLiteral("style")]       = style;
            entry[QStringLiteral("weight")]      = styleToWeight(style);
            entry[QStringLiteral("italic")]      = styleIsItalic(style);
            entry[QStringLiteral("displayName")] = displayName;
            m_fontCache.append(entry);
        }
    }

    m_cacheBuilt = true;
    return m_fontCache;
}

QStringList FontManager::stylesForFamily(const QString &family) const
{
    return QFontDatabase::styles(family);
}

QString FontManager::displayLabel(const QString &spec) const
{
    if (spec.trimmed().isEmpty())
        return {};

    QString family, style;
    int size;
    parseSpec(spec, family, style, size);
    return QStringLiteral("%1  %2  %3").arg(family, style).arg(size);
}

void FontManager::setDefaultFont(const QString &spec)
{
    m_prefs->setDefaultFont(spec);
}

void FontManager::setDocumentFont(const QString &spec)
{
    m_prefs->setDocumentFont(spec);
}

void FontManager::setMonospaceFont(const QString &spec)
{
    m_prefs->setMonospaceFont(spec);
}

void FontManager::setTitlebarFont(const QString &spec)
{
    m_prefs->setTitlebarFont(spec);
}

void FontManager::resetToDefaults()
{
    m_prefs->setDefaultFont({});
    m_prefs->setDocumentFont({});
    m_prefs->setMonospaceFont({});
    m_prefs->setTitlebarFont({});
}
