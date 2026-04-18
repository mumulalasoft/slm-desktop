#pragma once

#include "cleanertypes.h"

#include <QObject>
#include <QVariantMap>

namespace Slm::Cleaner {

class CleanerEngine : public QObject
{
    Q_OBJECT
public:
    explicit CleanerEngine(QObject *parent = nullptr);

    QVariantMap preview(const CleanRequest &request);
    QVariantMap clean(const CleanRequest &request);

signals:
    void progressChanged(int percent, const QString &message);

private:
    QVariantMap run(const CleanRequest &request, bool dryRun);
};

} // namespace Slm::Cleaner
