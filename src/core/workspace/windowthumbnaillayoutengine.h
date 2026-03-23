#pragma once
#include <QObject>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief Logic for calculating window thumbnail positions in overview.
 */
class WindowThumbnailLayoutEngine : public QObject {
    Q_OBJECT
public:
    explicit WindowThumbnailLayoutEngine(QObject *parent = nullptr);

    /**
     * @brief Calculates a grid layout for the given windows.
     * @param windows List of window metadata maps (must contain viewId, width, height).
     * @param containerWidth Total width available.
     * @param containerHeight Total height available.
     * @param spacing Spacing between thumbnails.
     * @return List of maps with {viewId: string, x: double, y: double, width: double, height: double}
     */
    Q_INVOKABLE QVariantList calculateLayout(const QVariantList &windows, 
                                            double containerWidth, 
                                            double containerHeight, 
                                            double spacing = 24.0);
};
