#pragma once

#include <QVariantMap>
#include <QStringList>

class SettingsStore
{
public:
    SettingsStore();

    bool start(QString *error = nullptr);
    QVariantMap settings() const;

    bool setSetting(const QString &path, const QVariant &value,
                    QStringList *changedPaths, QString *error);
    bool setSettingsPatch(const QVariantMap &patch,
                          QStringList *changedPaths, QString *error);

    static QVariant valueByPath(const QVariantMap &root, const QString &path, bool *ok);

private:
    static QVariantMap defaultSettings();
    static QVariantMap normalizeRoot(const QVariantMap &raw);
    static bool validateRoot(const QVariantMap &raw, QString *error);
    static bool validateValue(const QString &path, const QVariant &value, QString *error);
    static QVariantMap parseJsonObject(const QByteArray &json, bool *ok);

    static bool setValueByPath(QVariantMap &root, const QString &path, const QVariant &value, QString *error);

    bool save(QString *error);
    bool saveLastKnownGood(QString *error);
    bool loadLastKnownGood(QVariantMap *out, QString *error) const;
    QString storagePath() const;
    QString lastKnownGoodPath() const;

    QVariantMap m_root;
};
