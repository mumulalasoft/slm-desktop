#include "themeiconprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QStringList>

namespace {
static QStringList effectiveThemeSearchPaths()
{
    QStringList paths = QIcon::themeSearchPaths();
    if (!paths.contains(QStringLiteral("/usr/share/icons"))) {
        paths << QStringLiteral("/usr/share/icons");
    }
    if (!paths.contains(QStringLiteral("/usr/local/share/icons"))) {
        paths << QStringLiteral("/usr/local/share/icons");
    }
    return paths;
}

static QStringList effectiveThemeCandidates()
{
    QStringList themes;
    const QString currentTheme = QIcon::themeName().trimmed();
    const QString fallbackTheme = QIcon::fallbackThemeName().trimmed();
    if (!currentTheme.isEmpty()) {
        themes << currentTheme;
    }
    if (!fallbackTheme.isEmpty() && fallbackTheme != currentTheme) {
        themes << fallbackTheme;
    }
    if (!themes.contains(QStringLiteral("hicolor"))) {
        themes << QStringLiteral("hicolor");
    }
    if (!themes.contains(QStringLiteral("Adwaita"))) {
        themes << QStringLiteral("Adwaita");
    }
    return themes;
}

static QString queryValue(const QString &id, const QString &key)
{
    const int q = id.indexOf('?');
    if (q < 0 || q + 1 >= id.size()) {
        return QString();
    }
    const QString query = id.mid(q + 1);
    const QStringList parts = query.split('&', Qt::SkipEmptyParts);
    const QString needle = key + '=';
    for (const QString &part : parts) {
        if (part.startsWith(needle)) {
            return part.mid(needle.size()).trimmed();
        }
        if (part.trimmed() == key) {
            return QStringLiteral("1");
        }
    }
    return QString();
}

static bool queryEnabled(const QString &id, const QString &key)
{
    const QString val = queryValue(id, key).toLower();
    return val == QStringLiteral("1") || val == QStringLiteral("true") || val == QStringLiteral("yes");
}

static QString findIconInTheme(const QString &themeName,
                               const QStringList &searchPaths,
                               const QString &context,
                               const QString &name,
                               bool preferSvg)
{
    if (themeName.isEmpty()) {
        return QString();
    }
    const QStringList sizeDirs = {
        QStringLiteral("scalable"),
        QStringLiteral("symbolic"),
        QStringLiteral("512x512"),
        QStringLiteral("256x256"),
        QStringLiteral("192x192"),
        QStringLiteral("128x128"),
        QStringLiteral("96x96"),
        QStringLiteral("64x64"),
        QStringLiteral("48x48"),
        QStringLiteral("32x32"),
        QStringLiteral("24x24"),
        QStringLiteral("22x22"),
        QStringLiteral("16x16")
    };
    const QStringList exts = preferSvg
                             ? QStringList{QStringLiteral("svg"), QStringLiteral("png"), QStringLiteral("xpm")}
                             : QStringList{QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("xpm")};

    for (const QString &base : searchPaths) {
        if (base.isEmpty()) {
            continue;
        }
        for (const QString &sizeDir : sizeDirs) {
            for (const QString &ext : exts) {
                const QString candidate = QDir(base).filePath(
                    themeName + QLatin1Char('/') + sizeDir + QLatin1Char('/') + context
                    + QLatin1Char('/') + name + QLatin1Char('.') + ext);
                if (QFileInfo::exists(candidate)) {
                    return candidate;
                }
            }
        }
    }
    return QString();
}

static QString resolvePreferredIconPath(const QString &name, const QString &context, bool preferSvg)
{
    if (name.isEmpty() || context.isEmpty()) {
        return QString();
    }

    const QStringList themes = effectiveThemeCandidates();
    const QStringList searchPaths = effectiveThemeSearchPaths();
    for (const QString &theme : themes) {
        const QString path = findIconInTheme(theme, searchPaths, context, name, preferSvg);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return QString();
}

static bool hasVisiblePixel(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < argb.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) > 0) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ThemeIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString cleanId = id.split('?', Qt::KeepEmptyParts).first()
                              .split('#', Qt::KeepEmptyParts).first()
                              .trimmed();
    const QString context = queryValue(id, QStringLiteral("context")).trimmed();
    const bool preferSvg = queryEnabled(id, QStringLiteral("preferSvg"));
    const int extent = qMax(16, requestedSize.width() > 0 ? requestedSize.width() : 64);
    const QSize iconSize(extent, extent);

    QIcon icon;
    if (!context.isEmpty()) {
        const QString preferredPath = resolvePreferredIconPath(cleanId, context, preferSvg);
        if (!preferredPath.isEmpty()) {
            icon = QIcon(preferredPath);
        }
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(cleanId);
    }
    QPixmap pixmap;
    if (!icon.isNull()) {
        pixmap = icon.pixmap(iconSize);
    }

    if (!pixmap.isNull() && !hasVisiblePixel(pixmap.toImage())) {
        const QStringList fallbackNames = {
            QString(cleanId).replace(QStringLiteral("-secure-symbolic"), QStringLiteral("-secure")),
            QString(cleanId).replace(QStringLiteral("-symbolic"), QString()),
            QString(cleanId).replace(QStringLiteral("-secure-symbolic"), QStringLiteral("-symbolic")),
            QStringLiteral("network-wireless-signal-good-symbolic"),
            QStringLiteral("network-wireless-signal-good"),
            QStringLiteral("network-wireless-signal-none-symbolic")
        };

        for (const QString &name : fallbackNames) {
            if (name.isEmpty() || !QIcon::hasThemeIcon(name)) {
                continue;
            }
            QIcon alt = QIcon::fromTheme(name);
            QPixmap altPixmap = alt.pixmap(iconSize);
            if (altPixmap.isNull()) {
                continue;
            }
            if (hasVisiblePixel(altPixmap.toImage())) {
                pixmap = altPixmap;
                break;
            }
        }
    }

    if (pixmap.isNull() || !hasVisiblePixel(pixmap.toImage())) {
        pixmap = QPixmap(iconSize);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#8aa0b8"));
        painter.drawRoundedRect(QRect(0, 0, iconSize.width(), iconSize.height()), 10, 10);
    }

    if (size) {
        *size = pixmap.size();
    }
    return pixmap.toImage();
}
