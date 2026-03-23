#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QVariantMap>
#include <QStringList>
#include <QVector>

struct ShortcutEntry {
    QString name;
    QString iconSource;
    QString iconName;
    QString type; // "desktop" | "file" | "web"
    QString target;
    QString desktopFile;
    QString executable;
    QString sourcePath;
    bool removable = false;
};

class ShortcutModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconSourceRole,
        IconNameRole,
        TypeRole,
        TargetRole,
        DesktopFileRole,
        ExecutableRole,
        SourcePathRole
    };

    explicit ShortcutModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int shortcutCount() const;
    Q_INVOKABLE QVariantMap loadSlotMap() const;
    Q_INVOKABLE bool saveSlotMap(const QVariantMap &slotMap) const;
    Q_INVOKABLE bool addDesktopShortcut(const QString &desktopFilePath);
    Q_INVOKABLE bool addDesktopShortcutById(const QString &desktopId);
    Q_INVOKABLE bool addAppShortcut(const QString &name, const QString &executable,
                                    const QString &iconName = QString());
    Q_INVOKABLE bool addFileShortcut(const QString &filePathOrUrl);
    Q_INVOKABLE bool addWebShortcut(const QString &webUrl, const QString &title = QString());
    Q_INVOKABLE bool removeShortcut(int index);
    Q_INVOKABLE bool moveShortcut(int fromIndex, int toIndex);
    Q_INVOKABLE bool arrangeShortcuts();
    Q_INVOKABLE bool launchShortcut(int index);
    Q_INVOKABLE QVariantMap get(int index) const;

private:
    QString desktopDirectoryPath() const;
    QString orderFilePath() const;
    QString slotMapFilePath() const;
    bool ensureDesktopDir() const;
    void load();
    QStringList loadOrder() const;
    void saveOrder() const;
    void setupDesktopWatcher();
    void scheduleRefreshFromWatcher();
    bool launchDesktop(const ShortcutEntry &entry) const;
    bool containsDesktopFile(const QString &desktopFilePath) const;
    bool containsTarget(const QString &type, const QString &target) const;

    QVector<ShortcutEntry> m_entries;
    QFileSystemWatcher m_desktopWatcher;
    QTimer m_desktopWatchDebounce;
};
