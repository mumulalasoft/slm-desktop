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

TimeDateController::TimeDateController(QObject *parent)
    : QObject(parent)
{
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
