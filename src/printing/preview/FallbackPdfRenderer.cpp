#include "FallbackPdfRenderer.h"

#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>

namespace Slm::Print {

int FallbackPdfRenderer::queryPageCount(const QString & /*filePath*/)
{
    return 1;
}

QImage FallbackPdfRenderer::renderPage(const QString &filePath,
                                       int pageIndex,
                                       int /*dpi*/,
                                       const QSize &maxViewport)
{
    QSize size = maxViewport.isValid() ? maxViewport : QSize(595, 842); // A4 px at 72dpi

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    // Subtle page border
    p.setPen(QPen(QColor(200, 200, 200), 1));
    p.drawRect(image.rect().adjusted(0, 0, -1, -1));

    // Icon area — simple document glyph
    const int iconSize = qMin(size.width(), size.height()) / 5;
    QRect iconRect(size.width() / 2 - iconSize / 2,
                   size.height() / 2 - iconSize - 8,
                   iconSize, iconSize);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 230, 245));
    p.drawRoundedRect(iconRect, 4, 4);

    p.setPen(QColor(140, 160, 190));
    QFont iconFont = p.font();
    iconFont.setPixelSize(iconSize / 2);
    p.setFont(iconFont);
    p.drawText(iconRect, Qt::AlignCenter, QStringLiteral("📄"));

    // Filename
    p.setPen(QColor(60, 60, 60));
    QFont nameFont = p.font();
    nameFont.setPixelSize(qMax(11, size.height() / 35));
    nameFont.setBold(true);
    p.setFont(nameFont);
    const QString name = QFileInfo(filePath).fileName();
    QRect nameRect(16, iconRect.bottom() + 16, size.width() - 32, nameFont.pixelSize() + 8);
    p.drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, name);

    // Page indicator
    p.setPen(QColor(130, 130, 130));
    QFont pageFont = p.font();
    pageFont.setBold(false);
    pageFont.setPixelSize(qMax(9, size.height() / 45));
    p.setFont(pageFont);
    QRect pageRect(16, nameRect.bottom() + 6, size.width() - 32, pageFont.pixelSize() + 4);
    p.drawText(pageRect, Qt::AlignHCenter | Qt::AlignVCenter,
               QStringLiteral("Page %1").arg(pageIndex + 1));

    // "Preview unavailable" note
    p.setPen(QColor(170, 170, 170));
    QFont noteFont = p.font();
    noteFont.setPixelSize(qMax(8, size.height() / 55));
    noteFont.setItalic(true);
    p.setFont(noteFont);
    QRect noteRect(16, pageRect.bottom() + 4, size.width() - 32, noteFont.pixelSize() + 4);
    p.drawText(noteRect, Qt::AlignHCenter | Qt::AlignVCenter,
               QStringLiteral("Install libpoppler-qt6 for PDF preview"));

    p.end();
    return image;
}

} // namespace Slm::Print
