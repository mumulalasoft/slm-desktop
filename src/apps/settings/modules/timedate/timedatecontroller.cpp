#include "timedatecontroller.h"

#include <QDate>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QTime>
#include <QTimeZone>
#include <algorithm>

TimeDateController::TimeDateController(QObject *parent)
    : QObject(parent)
{
}

QVariantList TimeDateController::availableTimeZones() const
{
    QVariantList list;
    const QDateTime now = QDateTime::currentDateTime();
    const auto ids = QTimeZone::availableTimeZoneIds();
    for (const auto &id : ids) {
        QTimeZone tz(id);
        if (!tz.isValid()) continue;

        const int offsetSeconds = tz.offsetFromUtc(now);
        const int hours = offsetSeconds / 3600;
        const int minutes = qAbs(offsetSeconds % 3600) / 60;

        const QString offsetStr = QStringLiteral("UTC%1%2:%3")
                                      .arg(hours >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                                      .arg(qAbs(hours), 2, 10, QLatin1Char('0'))
                                      .arg(minutes, 2, 10, QLatin1Char('0'));

        QVariantMap map;
        map[QStringLiteral("id")] = QString::fromUtf8(id);
        map[QStringLiteral("offset")] = offsetStr;
        map[QStringLiteral("offsetSeconds")] = offsetSeconds;

        // Clean up the name for display (e.g. Asia/Jakarta -> Jakarta)
        QString city = QString::fromUtf8(id).split(QLatin1Char('/')).last().replace(QLatin1Char('_'), QLatin1Char(' '));
        map[QStringLiteral("city")] = city;
        map[QStringLiteral("label")] = QStringLiteral("(%1) %2").arg(offsetStr, city);

        list.append(map);
    }

    // Sort by offset primarily, then by ID
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const int oa = ma[QStringLiteral("offsetSeconds")].toInt();
        const int ob = mb[QStringLiteral("offsetSeconds")].toInt();
        if (oa != ob)
            return oa < ob;
        return ma[QStringLiteral("id")].toString() < mb[QStringLiteral("id")].toString();
    });

    return list;
}

QVariantMap TimeDateController::setDateTime(int year,
                                            int month,
                                            int day,
                                            int hour,
                                            int minute,
                                            int second) const
{
    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid() || !time.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-datetime")},
            {QStringLiteral("message"), QStringLiteral("Invalid date or time value.")},
        };
    }

    const QDateTime dt(date, time, QTimeZone::UTC);
    if (!dt.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-datetime")},
            {QStringLiteral("message"), QStringLiteral("Invalid date/time value.")},
        };
    }

    QDBusInterface iface(QStringLiteral("org.freedesktop.timedate1"),
                         QStringLiteral("/org/freedesktop/timedate1"),
                         QStringLiteral("org.freedesktop.timedate1"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("timedate1-unavailable")},
            {QStringLiteral("message"), QStringLiteral("Timedate service is unavailable.")},
        };
    }

    // Disable NTP first to allow manual clock changes; interactive=true lets polkit prompt.
    {
        QDBusReply<void> ntpReply = iface.call(QStringLiteral("SetNTP"), false, true);
        Q_UNUSED(ntpReply);
    }

    const qint64 usecUtc = dt.toMSecsSinceEpoch() * 1000;
    QDBusReply<void> setReply = iface.call(QStringLiteral("SetTime"),
                                           static_cast<qlonglong>(usecUtc),
                                           false,  // relative
                                           true);  // interactive auth
    if (!setReply.isValid()) {
        const QString err = setReply.error().message().trimmed();
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("set-time-failed")},
            {QStringLiteral("message"),
             err.isEmpty() ? QStringLiteral("Failed to apply date/time.") : err},
        };
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("applied"), dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))},
        {QStringLiteral("appliedIso"), dt.toString(Qt::ISODate)},
    };
}

QVariantMap TimeDateController::setTimezone(const QString &timezone) const
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.timedate1"),
                         QStringLiteral("/org/freedesktop/timedate1"),
                         QStringLiteral("org.freedesktop.timedate1"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("timedate1-unavailable")},
        };
    }

    QDBusReply<void> reply = iface.call(QStringLiteral("SetTimezone"), timezone, true);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), reply.error().message()},
        };
    }

    return {{QStringLiteral("ok"), true}};
}

QVariantMap TimeDateController::setNtp(bool enabled) const
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.timedate1"),
                         QStringLiteral("/org/freedesktop/timedate1"),
                         QStringLiteral("org.freedesktop.timedate1"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("timedate1-unavailable")},
        };
    }

    QDBusReply<void> reply = iface.call(QStringLiteral("SetNTP"), enabled, true);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), reply.error().message()},
        };
    }

    return {{QStringLiteral("ok"), true}};
}
