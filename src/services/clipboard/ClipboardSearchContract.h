#pragma once

#include <QDateTime>
#include <QVariantMap>
#include <QString>
#include <QVariantList>

namespace Slm::Clipboard {

// Summary-only shape used by Search query path.
struct ClipboardSearchSummary {
    QString resultId;
    QString clipboardItemId;
    QString itemType;
    QString previewText;
    QDateTime timestampBucket;
    QString sourceAppLabel;
    bool pinned = false;
    QString iconName;
    double score = 0.0;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("resultId"), resultId},
            {QStringLiteral("clipboardItemId"), clipboardItemId},
            {QStringLiteral("itemType"), itemType},
            {QStringLiteral("previewText"), previewText},
            {QStringLiteral("timestampBucket"), timestampBucket.toString(Qt::ISODate)},
            {QStringLiteral("sourceAppLabel"), sourceAppLabel},
            {QStringLiteral("pinned"), pinned},
            {QStringLiteral("iconName"), iconName},
            {QStringLiteral("score"), score},
        };
    }
};

// Richer result returned only in resolve flow after policy checks.
struct ClipboardResolvedItem {
    QString clipboardItemId;
    QString itemType;
    QString displayMode;
    QString fullText;
    QString fileUri;
    QString imageCachePath;
    bool requiresRecopyBeforePaste = true;
    bool isSensitive = false;
    bool isRedacted = false;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("clipboardItemId"), clipboardItemId},
            {QStringLiteral("itemType"), itemType},
            {QStringLiteral("displayMode"), displayMode},
            {QStringLiteral("fullText"), fullText},
            {QStringLiteral("fileUri"), fileUri},
            {QStringLiteral("imageCachePath"), imageCachePath},
            {QStringLiteral("requiresRecopyBeforePaste"), requiresRecopyBeforePaste},
            {QStringLiteral("isSensitive"), isSensitive},
            {QStringLiteral("isRedacted"), isRedacted},
        };
    }
};

struct ClipboardSearchOptions {
    int limit = 20;
    bool includePinned = true;
    bool includeImages = true;
    bool includeSensitive = false;
    bool trustedCaller = false;
    bool userGesture = false;
    QString callerRole;

    static ClipboardSearchOptions fromVariantMap(const QVariantMap &map)
    {
        ClipboardSearchOptions o;
        o.limit = map.value(QStringLiteral("limit"), o.limit).toInt();
        o.includePinned = map.value(QStringLiteral("includePinned"), o.includePinned).toBool();
        o.includeImages = map.value(QStringLiteral("includeImages"), o.includeImages).toBool();
        o.includeSensitive = map.value(QStringLiteral("includeSensitive"), o.includeSensitive).toBool();
        o.trustedCaller = map.value(QStringLiteral("trustedCaller"), o.trustedCaller).toBool();
        o.userGesture = map.value(QStringLiteral("userGesture"), o.userGesture).toBool();
        o.callerRole = map.value(QStringLiteral("callerRole")).toString();
        return o;
    }
};

struct ClipboardResolveContext {
    bool trustedCaller = false;
    bool userGesture = false;
    QString callerRole;
    QString sourceApp;

    static ClipboardResolveContext fromVariantMap(const QVariantMap &map)
    {
        ClipboardResolveContext c;
        c.trustedCaller = map.value(QStringLiteral("trustedCaller"), c.trustedCaller).toBool();
        c.userGesture = map.value(QStringLiteral("userGesture"), c.userGesture).toBool();
        c.callerRole = map.value(QStringLiteral("callerRole")).toString();
        c.sourceApp = map.value(QStringLiteral("sourceApp")).toString();
        return c;
    }
};

} // namespace Slm::Clipboard
