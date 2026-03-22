#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>

typedef struct _GFileMonitor GFileMonitor;

namespace Slm::Actions::Framework {

class DesktopEntryScannerGio : public QObject
{
    Q_OBJECT

public:
    explicit DesktopEntryScannerGio(QObject *parent = nullptr);
    ~DesktopEntryScannerGio() override;

    void setRoots(const QStringList &roots);
    QStringList roots() const;

    void start();
    void rescanNow();

signals:
    void entryAdded(const QString &path);
    void entryChanged(const QString &path);
    void entryRemoved(const QString &path);
    void scanCompleted(const QStringList &allPaths);

private:
    void rebuildMonitors();
    void clearMonitors();
    QStringList enumerateDesktopFiles() const;
    static QStringList enumerateDesktopFilesInRoot(const QString &rootPath);
    void handleMonitorEvent(const QString &path, int eventType);

    static void onMonitorChanged(GFileMonitor *monitor,
                                 void *file,
                                 void *otherFile,
                                 int eventType,
                                 void *userData);

    QStringList m_roots;
    QSet<QString> m_knownEntries;
    QList<GFileMonitor *> m_monitors;
};

} // namespace Slm::Actions::Framework

