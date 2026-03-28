#include "ClipboardService.h"

#include "ClipboardHistory.h"
#include "ClipboardWatcher.h"
#include "StorageManager.h"
#include "clipboardtypes.h"
#include "contentclassifier.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace Slm::Clipboard {
namespace {

QString normalizeUiPreview(const QString &raw)
{
    QString text = raw;
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    text = text.simplified();
    constexpr int kMaxUiPreview = 120;
    if (text.size() > kMaxUiPreview) {
        text = text.left(kMaxUiPreview - 3) + QStringLiteral("...");
    }
    return text;
}

QVariantMap sanitizeHistoryRow(const QVariantMap &row)
{
    QVariantMap out = row;
    const QString content = row.value(QStringLiteral("content")).toString();
    const QString preview = row.value(QStringLiteral("preview")).toString();
    const bool sensitive = ContentClassifier::isSensitiveText(content)
                           || ContentClassifier::isSensitiveText(preview);
    if (sensitive) {
        out.insert(QStringLiteral("content"), QString());
        out.insert(QStringLiteral("preview"), QStringLiteral("Sensitive item"));
        out.insert(QStringLiteral("isSensitive"), true);
        out.insert(QStringLiteral("isRedacted"), true);
        return out;
    }
    out.insert(QStringLiteral("preview"), normalizeUiPreview(preview));
    out.insert(QStringLiteral("isSensitive"), false);
    out.insert(QStringLiteral("isRedacted"), false);
    // Do not expose raw content on list/search paths; resolution should happen per-item.
    out.insert(QStringLiteral("content"), QString());
    return out;
}

QVariantList sanitizeHistoryRows(const QVariantList &rows)
{
    QVariantList out;
    out.reserve(rows.size());
    for (const QVariant &v : rows) {
        const QVariantMap row = v.toMap();
        if (!row.isEmpty()) {
            out.push_back(sanitizeHistoryRow(row));
        }
    }
    return out;
}

} // namespace

ClipboardService::ClipboardService(QObject *parent)
    : QObject(parent)
    , m_storage(new StorageManager)
    , m_history(new ClipboardHistory(m_storage, this))
    , m_watcher(new ClipboardWatcher(this))
{
    m_ready = m_storage->open();

    connect(m_watcher, &ClipboardWatcher::itemCaptured,
            this, &ClipboardService::onCaptured);
    connect(m_history, &ClipboardHistory::historyUpdated,
            this, &ClipboardService::HistoryUpdated);
}

ClipboardService::~ClipboardService()
{
    delete m_storage;
}

bool ClipboardService::isReady() const
{
    return m_ready;
}

QVariantList ClipboardService::getHistory(int limit) const
{
    return m_history ? sanitizeHistoryRows(m_history->list(limit)) : QVariantList{};
}

QVariantList ClipboardService::search(const QString &query, int limit) const
{
    return m_history ? sanitizeHistoryRows(m_history->search(query, limit)) : QVariantList{};
}

QString ClipboardService::backendMode() const
{
    return m_watcher ? m_watcher->backendModeString() : QStringLiteral("unknown");
}

bool ClipboardService::pasteItem(qint64 id)
{
    if (!m_history || id <= 0) {
        return false;
    }
    const QVariantMap row = m_history->itemById(id);
    if (row.isEmpty()) {
        return false;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }

    const QString type = row.value(QStringLiteral("type")).toString();
    const QString content = row.value(QStringLiteral("content")).toString();

    m_watcher->setSuppressed(true);

    bool ok = true;
    if (type == QString::fromLatin1(kTypeImage)) {
        const QImage image(content);
        if (image.isNull()) {
            ok = false;
        } else {
            clipboard->setImage(image, QClipboard::Clipboard);
        }
    } else if (type == QString::fromLatin1(kTypeFile)) {
        auto *mime = new QMimeData();
        QList<QUrl> urls;
        const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QUrl u(line.trimmed());
            if (u.isValid()) {
                urls.push_back(u);
            }
        }
        if (urls.isEmpty()) {
            delete mime;
            ok = false;
        } else {
            mime->setUrls(urls);
            clipboard->setMimeData(mime, QClipboard::Clipboard);
        }
    } else {
        clipboard->setText(content, QClipboard::Clipboard);
    }

    QTimer::singleShot(120, this, [this]() {
        if (m_watcher) {
            m_watcher->setSuppressed(false);
        }
    });

    return ok;
}

bool ClipboardService::deleteItem(qint64 id)
{
    return m_history && m_history->deleteItem(id);
}

bool ClipboardService::pinItem(qint64 id, bool pinned)
{
    return m_history && m_history->setPinned(id, pinned);
}

bool ClipboardService::clearHistory()
{
    return m_history && m_history->clearHistory();
}

void ClipboardService::onCaptured(const QVariantMap &item)
{
    if (!m_history) {
        return;
    }
    const qint64 id = m_history->add(item);
    if (id <= 0) {
        return;
    }
    QVariantMap emitted = item;
    emitted.insert(QStringLiteral("id"), id);
    emit ClipboardChanged(emitted);
}

} // namespace Slm::Clipboard
