#pragma once

#include <QObject>
#include <QString>
#include "../../core/prefs/uipreferences.h"

/**
 * Manages desktop wallpaper via:
 *  - org.freedesktop.portal.FileChooser for image picking (D-Bus portal)
 *  - UIPreferences for reading/writing the wallpaper URI
 */
class WallpaperManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentWallpaperUri READ currentWallpaperUri NOTIFY wallpaperChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit WallpaperManager(UIPreferences *uiPrefs, QObject *parent = nullptr);

    QString currentWallpaperUri() const;
    bool isLoading() const { return m_loading; }

    /** Open the XDG FileChooser portal and set the chosen image as wallpaper. */
    Q_INVOKABLE void openFilePicker();

    /** Directly apply a file:// URI as wallpaper. */
    Q_INVOKABLE void setWallpaperUri(const QString &fileUri);

signals:
    void wallpaperChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

private slots:
    void onFileChooserResponse(uint response, const QVariantMap &results);

private:
    void setLoading(bool loading);

    UIPreferences *m_uiPrefs = nullptr;
    bool m_loading = false;
    QString m_activeRequestPath;
};
