#include "slmdesktopentryscanner.h"

#include <QDir>
#include <QFileInfo>

#ifdef signals
#undef signals
#endif
#include <gio/gio.h>

namespace Slm::Actions::Framework {
namespace {

QString filePathFromGFile(GFile *file)
{
    if (!file) {
        return QString();
    }
    gchar *path = g_file_get_path(file);
    const QString out = path ? QString::fromUtf8(path) : QString();
    if (path) {
        g_free(path);
    }
    return out;
}

} // namespace

DesktopEntryScannerGio::DesktopEntryScannerGio(QObject *parent)
    : QObject(parent)
{
    const QString home = qEnvironmentVariable("HOME");
    m_roots = {
        QStringLiteral("/usr/share/applications"),
        home + QStringLiteral("/.local/share/applications"),
    };
}

DesktopEntryScannerGio::~DesktopEntryScannerGio()
{
    clearMonitors();
}

void DesktopEntryScannerGio::setRoots(const QStringList &roots)
{
    m_roots.clear();
    m_roots.reserve(roots.size());
    for (const QString &root : roots) {
        const QString cleaned = QDir::cleanPath(root.trimmed());
        if (!cleaned.isEmpty()) {
            m_roots.push_back(cleaned);
        }
    }
}

QStringList DesktopEntryScannerGio::roots() const
{
    return m_roots;
}

void DesktopEntryScannerGio::start()
{
    rebuildMonitors();
    rescanNow();
}

void DesktopEntryScannerGio::rescanNow()
{
    const QStringList entries = enumerateDesktopFiles();
    QSet<QString> fresh = QSet<QString>(entries.begin(), entries.end());

    for (const QString &path : fresh) {
        if (!m_knownEntries.contains(path)) {
            emit entryAdded(path);
        }
    }
    for (const QString &path : m_knownEntries) {
        if (!fresh.contains(path)) {
            emit entryRemoved(path);
        }
    }

    m_knownEntries = fresh;
    emit scanCompleted(entries);
}

void DesktopEntryScannerGio::rebuildMonitors()
{
    clearMonitors();
    for (const QString &root : m_roots) {
        if (!QFileInfo(root).isDir()) {
            continue;
        }
        GFile *file = g_file_new_for_path(root.toUtf8().constData());
        if (!file) {
            continue;
        }
        GError *error = nullptr;
        GFileMonitor *monitor = g_file_monitor_directory(file, G_FILE_MONITOR_NONE, nullptr, &error);
        g_object_unref(file);
        if (error) {
            g_error_free(error);
            continue;
        }
        if (!monitor) {
            continue;
        }
        g_signal_connect(monitor, "changed", G_CALLBACK(&DesktopEntryScannerGio::onMonitorChanged), this);
        m_monitors.push_back(monitor);
    }
}

void DesktopEntryScannerGio::clearMonitors()
{
    for (GFileMonitor *monitor : m_monitors) {
        if (!monitor) {
            continue;
        }
        g_signal_handlers_disconnect_by_data(monitor, this);
        g_file_monitor_cancel(monitor);
        g_object_unref(monitor);
    }
    m_monitors.clear();
}

QStringList DesktopEntryScannerGio::enumerateDesktopFiles() const
{
    QSet<QString> out;
    for (const QString &root : m_roots) {
        const QStringList files = enumerateDesktopFilesInRoot(root);
        for (const QString &path : files) {
            out.insert(path);
        }
    }
    QStringList sorted = out.values();
    sorted.sort();
    return sorted;
}

QStringList DesktopEntryScannerGio::enumerateDesktopFilesInRoot(const QString &rootPath)
{
    QStringList out;
    if (!QFileInfo(rootPath).isDir()) {
        return out;
    }

    QStringList queue;
    queue.push_back(rootPath);
    while (!queue.isEmpty()) {
        const QString current = queue.takeLast();
        GFile *dir = g_file_new_for_path(current.toUtf8().constData());
        if (!dir) {
            continue;
        }
        GError *error = nullptr;
        GFileEnumerator *enumerator = g_file_enumerate_children(
            dir,
            "standard::name,standard::type",
            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
            nullptr,
            &error);
        g_object_unref(dir);
        if (error) {
            g_error_free(error);
            continue;
        }
        if (!enumerator) {
            continue;
        }

        while (true) {
            GFileInfo *info = g_file_enumerator_next_file(enumerator, nullptr, &error);
            if (error) {
                g_error_free(error);
                error = nullptr;
                break;
            }
            if (!info) {
                break;
            }

            const gchar *name = g_file_info_get_name(info);
            const GFileType type = g_file_info_get_file_type(info);
            if (name && *name) {
                const QString childPath = QDir(current).filePath(QString::fromUtf8(name));
                if (type == G_FILE_TYPE_DIRECTORY) {
                    queue.push_back(childPath);
                } else if (type == G_FILE_TYPE_REGULAR && childPath.endsWith(QStringLiteral(".desktop"))) {
                    out.push_back(QDir::cleanPath(childPath));
                }
            }
            g_object_unref(info);
        }
        g_object_unref(enumerator);
    }
    return out;
}

void DesktopEntryScannerGio::handleMonitorEvent(const QString &path, int eventType)
{
    const QString cleaned = QDir::cleanPath(path);
    if (!cleaned.endsWith(QStringLiteral(".desktop"))) {
        // Directory or unrelated file change; refresh once.
        rescanNow();
        return;
    }

    switch (eventType) {
    case G_FILE_MONITOR_EVENT_CREATED:
        emit entryAdded(cleaned);
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        emit entryRemoved(cleaned);
        break;
    default:
        emit entryChanged(cleaned);
        break;
    }
    rescanNow();
}

void DesktopEntryScannerGio::onMonitorChanged(GFileMonitor * /*monitor*/,
                                              void *file,
                                              void * /*otherFile*/,
                                              int eventType,
                                              void *userData)
{
    if (!userData) {
        return;
    }
    auto *self = static_cast<DesktopEntryScannerGio *>(userData);
    const QString path = filePathFromGFile(static_cast<GFile *>(file));
    if (!path.isEmpty()) {
        self->handleMonitorEvent(path, eventType);
    } else {
        self->rescanNow();
    }
}

} // namespace Slm::Actions::Framework
