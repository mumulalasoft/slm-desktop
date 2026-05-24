#pragma once

#include <QObject>
#include <QVariantMap>

class TimeDateController : public QObject
{
    Q_OBJECT

public:
    explicit TimeDateController(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList availableTimeZones() const;

    Q_INVOKABLE QVariantMap setDateTime(int year,
                                        int month,
                                        int day,
                                        int hour,
                                        int minute,
                                        int second = 0) const;

    Q_INVOKABLE QVariantMap setTimezone(const QString &timezone) const;

    Q_INVOKABLE QVariantMap setNtp(bool enabled) const;
};
