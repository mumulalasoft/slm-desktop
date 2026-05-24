#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class DisplayController : public QObject
{
    Q_OBJECT

public:
    explicit DisplayController(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList availableResolutions() const;
    Q_INVOKABLE QVariantList availableScales() const;
    Q_INVOKABLE void applyResolution(const QString &resolution);
    Q_INVOKABLE void applyScaling(double scale);
};
