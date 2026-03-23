#pragma once

#include "slmmotiontypes.h"

#include <QObject>

namespace Slm::Motion {

class GestureBinding : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit GestureBinding(QObject *parent = nullptr);

    Q_INVOKABLE void begin(double initialProgress = 0.0);
    Q_INVOKABLE void updateByDistance(double distance, double range);
    Q_INVOKABLE int end(double velocity,
                        const GestureSettleConfig &cfg = GestureSettleConfig{});

    double progress() const;

signals:
    void progressChanged();

private:
    double m_progress = 0.0;
};

} // namespace Slm::Motion
