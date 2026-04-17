#include "effectiveenvpreviewcontroller.h"
#include "envserviceclient.h"

#include <algorithm>

EffectiveEnvPreviewController::EffectiveEnvPreviewController(EnvServiceClient *client,
                                                               QObject          *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &EnvServiceClient::serviceAvailableChanged,
            this, &EffectiveEnvPreviewController::refresh);
    connect(m_client, &EnvServiceClient::userVarsChanged,
            this, &EffectiveEnvPreviewController::refresh);
    connect(m_client, &EnvServiceClient::sessionVarsChanged,
            this, &EffectiveEnvPreviewController::refresh);
}

QVariantList EffectiveEnvPreviewController::entries() const
{
    return m_entries;
}

QString EffectiveEnvPreviewController::filterText() const
{
    return m_filterText;
}

bool EffectiveEnvPreviewController::loading() const
{
    return m_loading;
}

QString EffectiveEnvPreviewController::statusText() const
{
    return m_statusText;
}

void EffectiveEnvPreviewController::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    emit filterTextChanged();
    rebuild();
}

void EffectiveEnvPreviewController::refresh()
{
    if (!m_client->serviceAvailable()) {
        m_allEntries.clear();
        m_statusText = tr("Environment service is not running.");
        emit statusTextChanged();
        rebuild();
        return;
    }

    m_loading = true;
    emit loadingChanged();

    const QVariantMap resolved = m_client->resolveEnv();
    m_allEntries.clear();
    m_allEntries.reserve(resolved.size());

    // Convert QVariantMap { KEY -> { value, layer } } to a flat list.
    QStringList keys = resolved.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString &key : keys) {
        const QVariantMap entry = resolved.value(key).toMap();
        QVariantMap row;
        row[QStringLiteral("key")]   = key;
        row[QStringLiteral("value")] = entry.value(QStringLiteral("value"));
        row[QStringLiteral("layer")] = entry.value(QStringLiteral("layer"));
        m_allEntries.append(row);
    }

    m_statusText = tr("%1 variables").arg(m_allEntries.size());
    m_loading    = false;
    emit statusTextChanged();
    emit loadingChanged();
    rebuild();
}

void EffectiveEnvPreviewController::rebuild()
{
    if (m_filterText.isEmpty()) {
        m_entries = m_allEntries;
    } else {
        const QString lc = m_filterText.toLower();
        m_entries.clear();
        for (const QVariant &v : std::as_const(m_allEntries)) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("key")).toString().toLower().contains(lc)
                || row.value(QStringLiteral("value")).toString().toLower().contains(lc)) {
                m_entries.append(row);
            }
        }
    }
    emit entriesChanged();
}
