#include "ClipboardSearchProvider.h"
#include "contentclassifier.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>

namespace Slm::Clipboard {
namespace {
constexpr const char kService[] = "org.desktop.Clipboard";
constexpr const char kPath[] = "/org/desktop/Clipboard";
constexpr const char kIface[] = "org.desktop.Clipboard";

QString clipboardSubtitle(const QVariantMap &row)
{
    const QString type = row.value(QStringLiteral("type")).toString().trimmed().toUpper();
    const QString preview = row.value(QStringLiteral("preview")).toString().trimmed();
    if (type == QStringLiteral("URL")) {
        const QUrl url(preview);
        if (url.isValid() && !url.host().isEmpty()) {
            return url.host();
        }
    }
    if (!preview.isEmpty()) {
        return preview;
    }
    return type;
}

QString normalizedPreview(const QString &raw)
{
    QString text = raw;
    // One-line preview for search rows.
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    text = text.simplified();
    constexpr int kMaxPreviewLen = 80;
    if (text.size() > kMaxPreviewLen) {
        text = text.left(kMaxPreviewLen - 3) + QStringLiteral("...");
    }
    return text;
}

bool isSensitiveRow(const QVariantMap &row)
{
    const QString content = row.value(QStringLiteral("content")).toString();
    const QString preview = row.value(QStringLiteral("preview")).toString();
    return ContentClassifier::isSensitiveText(content) || ContentClassifier::isSensitiveText(preview);
}

int clipboardBaseScore(const QVariantMap &row)
{
    const bool pinned = row.value(QStringLiteral("isPinned")).toBool();
    const qint64 ts = row.value(QStringLiteral("timestamp")).toLongLong();
    const qint64 ageMs = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - ts);
    const int agePenalty = int(qMin<qint64>(ageMs / 60000, 180));
    return (pinned ? 1200 : 1000) - agePenalty;
}

QVariantMap rowMapFromVariant(const QVariant &v)
{
    if (!v.isValid()) {
        return {};
    }
    const QVariantMap direct = v.toMap();
    if (!direct.isEmpty()) {
        return direct;
    }
    if (v.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        QVariantMap map;
        arg >> map;
        return map;
    }
    return {};
}
}

ClipboardSearchProvider::ClipboardSearchProvider(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus(),
                                 this))
{
}

ClipboardSearchProvider::~ClipboardSearchProvider() = default;

bool ClipboardSearchProvider::available() const
{
    return m_iface && m_iface->isValid();
}

QVariantList ClipboardSearchProvider::query(const QString &text, int limit) const
{
    return query(text, limit, QVariantMap{});
}

QVariantList ClipboardSearchProvider::query(const QString &text,
                                            int limit,
                                            const QVariantMap &options) const
{
    QVariantList out;
    if (!available()) {
        return out;
    }

    const bool includePinned = options.value(QStringLiteral("includePinned"), true).toBool();
    const bool includeImages = options.value(QStringLiteral("includeImages"), true).toBool();
    const bool trustedCaller = options.value(QStringLiteral("trustedCaller"), false).toBool();
    const bool userGesture = options.value(QStringLiteral("userGesture"), false).toBool();
    bool includeSensitive = options.value(QStringLiteral("includeSensitive"), false).toBool();
    // Never trust caller flags blindly: sensitive exposure requires trusted + explicit gesture.
    includeSensitive = includeSensitive && trustedCaller && userGesture;

    const QString q = text.trimmed();
    QDBusReply<QVariantList> reply = q.isEmpty()
            ? m_iface->call(QStringLiteral("GetHistory"), qMax(1, limit))
            : m_iface->call(QStringLiteral("Search"), q, qMax(1, limit));
    if (!reply.isValid()) {
        return out;
    }

    const QVariantList rows = reply.value();
    out.reserve(rows.size());
    for (const QVariant &v : rows) {
        const QVariantMap row = rowMapFromVariant(v);
        if (row.isEmpty()) {
            continue;
        }
        const bool sensitive = isSensitiveRow(row);
        // Default privacy posture: exclude sensitive clipboard records from search provider.
        if (sensitive && !includeSensitive) {
            continue;
        }
        QVariantMap mapped;
        const qlonglong id = row.value(QStringLiteral("id")).toLongLong();
        if (id <= 0) {
            continue;
        }
        const QString type = row.value(QStringLiteral("type")).toString().trimmed().toLower();
        const bool pinned = row.value(QStringLiteral("isPinned")).toBool();
        if (!includePinned && pinned) {
            continue;
        }
        if (!includeImages && type == QStringLiteral("image")) {
            continue;
        }
        const QString preview = normalizedPreview(row.value(QStringLiteral("preview")).toString());
        const qint64 ts = row.value(QStringLiteral("timestamp")).toLongLong();
        const qint64 tsBucket = ts > 0 ? (ts / 60000) * 60000 : 0;
        mapped.insert(QStringLiteral("id"), QStringLiteral("clipboard:%1").arg(id));
        mapped.insert(QStringLiteral("provider"), QStringLiteral("clipboard"));
        mapped.insert(QStringLiteral("title"),
                      sensitive ? QStringLiteral("Sensitive item")
                                : (preview.isEmpty() ? QStringLiteral("Clipboard item") : preview));
        mapped.insert(QStringLiteral("subtitle"), normalizedPreview(clipboardSubtitle(row)));
        mapped.insert(QStringLiteral("iconName"),
                      type == QStringLiteral("image") ? QStringLiteral("image-x-generic")
                      : (type == QStringLiteral("url") ? QStringLiteral("internet-web-browser")
                         : (type == QStringLiteral("file") ? QStringLiteral("text-x-generic")
                            : QStringLiteral("edit-paste"))));
        mapped.insert(QStringLiteral("score"), clipboardBaseScore(row));
        mapped.insert(QStringLiteral("clipboardId"), id);
        mapped.insert(QStringLiteral("clipboardType"), type);
        mapped.insert(QStringLiteral("timestampBucket"), tsBucket);
        mapped.insert(QStringLiteral("isSensitive"), sensitive);
        mapped.insert(QStringLiteral("preview"), QVariantMap{
                          {QStringLiteral("type"), QStringLiteral("clipboard")},
                          {QStringLiteral("contentType"), type},
                          {QStringLiteral("preview"), sensitive ? QStringLiteral("Sensitive item") : preview},
                          {QStringLiteral("timestampBucket"), tsBucket},
                          {QStringLiteral("isSensitive"), sensitive},
                      });
        out.push_back(mapped);
    }
    return out;
}

} // namespace Slm::Clipboard
