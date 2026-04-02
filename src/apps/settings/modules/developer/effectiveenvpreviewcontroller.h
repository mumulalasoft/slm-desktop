#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class EnvServiceClient;

// EffectiveEnvPreviewController — drives the "Effective Environment" preview
// panel in the Settings UI.
//
// Calls ResolveEnv on the service and exposes the result as a flat sorted list
// for display in a QML ListView.  Falls back to an empty list and sets an
// error message when the service is unavailable.
//
class EffectiveEnvPreviewController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList entries    READ entries    NOTIFY entriesChanged)
    Q_PROPERTY(QString      filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool         loading    READ loading    NOTIFY loadingChanged)
    Q_PROPERTY(QString      statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit EffectiveEnvPreviewController(EnvServiceClient *client,
                                           QObject          *parent = nullptr);

    QVariantList entries()    const;
    QString      filterText() const;
    bool         loading()    const;
    QString      statusText() const;

    Q_INVOKABLE void refresh();
    void setFilterText(const QString &text);

signals:
    void entriesChanged();
    void filterTextChanged();
    void loadingChanged();
    void statusTextChanged();

private:
    void rebuild();

    EnvServiceClient *m_client;
    QVariantList      m_allEntries;   // unfiltered cache
    QVariantList      m_entries;      // after filter
    QString           m_filterText;
    bool              m_loading    = false;
    QString           m_statusText;
};
