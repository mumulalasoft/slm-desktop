#include "windowthumbnaillayoutengine.h"
#include <cmath>

WindowThumbnailLayoutEngine::WindowThumbnailLayoutEngine(QObject *parent)
    : QObject(parent)
{
}

QVariantList WindowThumbnailLayoutEngine::calculateLayout(const QVariantList &windows, 
                                                        double containerWidth, 
                                                        double containerHeight, 
                                                        double spacing)
{
    QVariantList results;
    int count = windows.count();
    if (count <= 0) return results;

    // Calculate grid dimensions (columns and rows)
    int cols = std::ceil(std::sqrt(count));
    int rows = std::ceil((double)count / cols);

    // Initial cell size based on available space minus spacing
    double availableWidth = containerWidth - (spacing * (cols + 1));
    double availableHeight = containerHeight - (spacing * (rows + 1));

    double cellWidth = availableWidth / cols;
    double cellHeight = availableHeight / rows;

    // Maintain a standard aspect ratio (e.g., 16:9) for thumbnails
    const double targetAspect = 16.0 / 9.0;
    if (cellWidth / cellHeight > targetAspect) {
        cellWidth = cellHeight * targetAspect;
    } else {
        cellHeight = cellWidth / targetAspect;
    }

    // Limit maximum size to avoid huge thumbnails when count is low
    const double maxCellWidth = containerWidth * 0.45;
    const double maxCellHeight = containerHeight * 0.45;
    if (cellWidth > maxCellWidth) {
        cellWidth = maxCellWidth;
        cellHeight = cellWidth / targetAspect;
    }
    if (cellHeight > maxCellHeight) {
        cellHeight = maxCellHeight;
        cellWidth = cellHeight * targetAspect;
    }

    // Recalculate total grid size for centering
    double totalGridWidth = (cellWidth * cols) + (spacing * (cols - 1));
    double totalGridHeight = (cellHeight * rows) + (spacing * (rows - 1));

    double startX = (containerWidth - totalGridWidth) / 2.0;
    double startY = (containerHeight - totalGridHeight) / 2.0;

    for (int i = 0; i < count; ++i) {
        int r = i / cols;
        int c = i % cols;

        // In the last row, center the items if they don't fill the row
        int itemsInThisRow = (r == rows - 1) ? (count % cols) : cols;
        if (itemsInThisRow == 0) itemsInThisRow = cols;
        double rowOffsetX = 0;
        if (itemsInThisRow < cols) {
            double rowWidth = (cellWidth * itemsInThisRow) + (spacing * (itemsInThisRow - 1));
            rowOffsetX = (totalGridWidth - rowWidth) / 2.0;
        }

        QVariantMap win = windows.at(i).toMap();
        QVariantMap res;
        res["viewId"] = win["viewId"];
        res["x"] = startX + rowOffsetX + (c * (cellWidth + spacing));
        res["y"] = startY + (r * (cellHeight + spacing));
        res["width"] = cellWidth;
        res["height"] = cellHeight;
        results.append(res);
    }

    return results;
}
