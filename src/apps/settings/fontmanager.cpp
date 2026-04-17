#include "fontmanager.h"
#include "desktopsettingsclient.h"

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

static int specSizeOrDefault(const QString &spec, int fallback = 11)
{
    QString family, style;
    int size = fallback;
    parseSpec(spec, family, style, size);
    return size > 0 ? size : fallback;
}

static QString specWithSize(const QString &spec, int requestedSize, const QString &fallbackFamily)
{
    const int clampedSize = qBound(6, requestedSize, 72);
    QString family, style;
    int currentSize = 11;
    parseSpec(spec, family, style, currentSize);
    if (family.trimmed().isEmpty()) {
        family = fallbackFamily.trimmed().isEmpty() ? QStringLiteral("Noto Sans")
                                                    : fallbackFamily.trimmed();
    }
    if (style.trimmed().isEmpty()) {
        style = QStringLiteral("Regular");
    }
    return QStringLiteral("%1,%2,%3").arg(family, style).arg(clampedSize);
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
                                        const QString &documentSpec,
                                        const QString &monospaceSpec,
                                        const QString &titlebarSpec)
{
    const QString configBase = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    QSettings kdeglobals(configBase + QStringLiteral("/kdeglobals"), QSettings::IniFormat);

    const QString defQt  = specToQtFontString(defaultSpec);
    const QString docQt  = specToQtFontString(documentSpec);
    const QString monoQt = specToQtFontString(monospaceSpec);
    const QString titleQt = specToQtFontString(titlebarSpec);

    kdeglobals.beginGroup(QStringLiteral("General"));
    if (!defQt.isEmpty()) {
        kdeglobals.setValue(QStringLiteral("font"), defQt);
        kdeglobals.setValue(QStringLiteral("menuFont"), defQt);
        kdeglobals.setValue(QStringLiteral("toolBarFont"), defQt);
        kdeglobals.setValue(QStringLiteral("taskbarFont"), defQt);
    }
    if (!docQt.isEmpty())  kdeglobals.setValue(QStringLiteral("smallestReadableFont"), docQt);
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
    writeGtkFontSettings(currentDefaultFont());
    writeKdeFontSettings(currentDefaultFont(),
                         currentDocumentFont(),
                         currentMonospaceFont(),
                         currentTitlebarFont());
}

// ── Constructor ───────────────────────────────────────────────────────────────

FontManager::FontManager(DesktopSettingsClient *desktopSettings,
                         QObject *parent)
    : QObject(parent)
    , m_desktopSettings(desktopSettings)
{
    connectSources();
    applyFonts();
}

void FontManager::connectSources()
{
    auto apply = [this]() { applyFonts(); };

    if (!m_desktopSettings) {
        return;
    }
    connect(m_desktopSettings, &DesktopSettingsClient::defaultFontChanged,
            this, &FontManager::defaultFontChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::documentFontChanged,
            this, &FontManager::documentFontChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::monospaceFontChanged,
            this, &FontManager::monospaceFontChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::titlebarFontChanged,
            this, &FontManager::titlebarFontChanged);

    connect(m_desktopSettings, &DesktopSettingsClient::defaultFontChanged,   this, apply);
    connect(m_desktopSettings, &DesktopSettingsClient::documentFontChanged,  this, apply);
    connect(m_desktopSettings, &DesktopSettingsClient::monospaceFontChanged, this, apply);
    connect(m_desktopSettings, &DesktopSettingsClient::titlebarFontChanged,  this, apply);
}

// ── Public API ────────────────────────────────────────────────────────────────

QString FontManager::defaultFont()   const { return currentDefaultFont(); }
QString FontManager::documentFont()  const { return currentDocumentFont(); }
QString FontManager::monospaceFont() const { return currentMonospaceFont(); }
QString FontManager::titlebarFont()  const { return currentTitlebarFont(); }
int FontManager::defaultFontSize() const { return specSizeOrDefault(currentDefaultFont(), 11); }
int FontManager::documentFontSize() const { return specSizeOrDefault(currentDocumentFont(), 11); }
int FontManager::monospaceFontSize() const { return specSizeOrDefault(currentMonospaceFont(), 11); }
int FontManager::titlebarFontSize() const { return specSizeOrDefault(currentTitlebarFont(), 11); }

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
    setFontByRole(QStringLiteral("fonts.defaultFont"), spec);
}

void FontManager::setDocumentFont(const QString &spec)
{
    setFontByRole(QStringLiteral("fonts.documentFont"), spec);
}

void FontManager::setMonospaceFont(const QString &spec)
{
    setFontByRole(QStringLiteral("fonts.monospaceFont"), spec);
}

void FontManager::setTitlebarFont(const QString &spec)
{
    setFontByRole(QStringLiteral("fonts.titlebarFont"), spec);
}

void FontManager::setDefaultFontSize(int size)
{
    setFontByRole(QStringLiteral("fonts.defaultFont"),
                  specWithSize(currentDefaultFont(), size, QStringLiteral("Noto Sans")));
}

void FontManager::setDocumentFontSize(int size)
{
    setFontByRole(QStringLiteral("fonts.documentFont"),
                  specWithSize(currentDocumentFont(), size, QStringLiteral("Noto Sans")));
}

void FontManager::setMonospaceFontSize(int size)
{
    setFontByRole(QStringLiteral("fonts.monospaceFont"),
                  specWithSize(currentMonospaceFont(), size, QStringLiteral("Noto Sans Mono")));
}

void FontManager::setTitlebarFontSize(int size)
{
    setFontByRole(QStringLiteral("fonts.titlebarFont"),
                  specWithSize(currentTitlebarFont(), size, QStringLiteral("Noto Sans")));
}

void FontManager::resetToDefaults()
{
    setFontByRole(QStringLiteral("fonts.defaultFont"), QString());
    setFontByRole(QStringLiteral("fonts.documentFont"), QString());
    setFontByRole(QStringLiteral("fonts.monospaceFont"), QString());
    setFontByRole(QStringLiteral("fonts.titlebarFont"), QString());
}

QString FontManager::currentDefaultFont() const
{
    return m_desktopSettings ? m_desktopSettings->defaultFont() : QString();
}

QString FontManager::currentDocumentFont() const
{
    return m_desktopSettings ? m_desktopSettings->documentFont() : QString();
}

QString FontManager::currentMonospaceFont() const
{
    return m_desktopSettings ? m_desktopSettings->monospaceFont() : QString();
}

QString FontManager::currentTitlebarFont() const
{
    return m_desktopSettings ? m_desktopSettings->titlebarFont() : QString();
}

bool FontManager::setFontByRole(const QString &rolePath, const QString &spec)
{
    if (!m_desktopSettings) {
        return false;
    }
    if (rolePath == QLatin1String("fonts.defaultFont")) {
        return m_desktopSettings->setDefaultFont(spec);
    }
    if (rolePath == QLatin1String("fonts.documentFont")) {
        return m_desktopSettings->setDocumentFont(spec);
    }
    if (rolePath == QLatin1String("fonts.monospaceFont")) {
        return m_desktopSettings->setMonospaceFont(spec);
    }
    if (rolePath == QLatin1String("fonts.titlebarFont")) {
        return m_desktopSettings->setTitlebarFont(spec);
    }
    return false;
}
