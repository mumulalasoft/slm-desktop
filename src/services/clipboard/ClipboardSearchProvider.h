#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class QDBusInterface;

namespace Slm::Clipboard {

class ClipboardSearchProvider : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardSearchProvider(QObject *parent = nullptr);
    ~ClipboardSearchProvider() override;

    bool available() const;
    QVariantList query(const QString &text, int limit = 24) const;
    QVariantList query(const QString &text, int limit, const QVariantMap &options) const;

private:
    QDBusInterface *m_iface = nullptr;
};

} // namespace Slm::Clipboard
