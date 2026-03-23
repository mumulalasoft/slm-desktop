#pragma once

#include <QObject>
#include <QVariantMap>

class PortalManager : public QObject
{
    Q_OBJECT

public:
    explicit PortalManager(QObject *parent = nullptr);

    QVariantMap Screenshot(const QVariantMap &options) const;
    QVariantMap ScreenCast(const QVariantMap &options) const;
    QVariantMap FileChooser(const QVariantMap &options) const;
    QVariantMap OpenURI(const QString &uri, const QVariantMap &options) const;
    QVariantMap PickColor(const QVariantMap &options) const;
    QVariantMap PickFolder(const QVariantMap &options) const;
    QVariantMap Wallpaper(const QVariantMap &options) const;

private:
    static QVariantMap makeNotImplemented(const QString &method,
                                          const QVariantMap &extra = QVariantMap());
};

