#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ThemeIconController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lightThemeName READ lightThemeName NOTIFY themeMappingChanged)
    Q_PROPERTY(QString darkThemeName READ darkThemeName NOTIFY themeMappingChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit ThemeIconController(QObject *parent = nullptr);

    QString lightThemeName() const;
    QString darkThemeName() const;
    QStringList availableThemes() const;
    int revision() const;

    Q_INVOKABLE void applyForDarkMode(bool darkMode);
    Q_INVOKABLE void setThemeMapping(const QString &lightThemeName, const QString &darkThemeName);
    Q_INVOKABLE void useAutoDetectedMapping();
    Q_INVOKABLE void refreshAvailableThemes();

signals:
    void themeMappingChanged();
    void availableThemesChanged();
    void revisionChanged();

private:
    bool themeExists(const QString &themeName) const;
    void detectDefaultThemeMapping();
    QString normalizedThemeName(QString name) const;

    QString m_lightThemeName;
    QString m_darkThemeName;
    QStringList m_availableThemes;
    int m_revision = 0;
};
