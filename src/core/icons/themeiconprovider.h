#pragma once

#include <QQuickImageProvider>

class ThemeIconProvider : public QQuickImageProvider {
public:
    ThemeIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

