#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

namespace {
constexpr const char *kService = "org.slm.Desktop.FileOperations";
constexpr const char *kPath = "/org/slm/Desktop/FileOperations";
constexpr const char *kIface = "org.slm.Desktop.FileOperations";

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  fileopctl --help\n";
    out << "  fileopctl copy <destination> <uri_or_path>... [--wait]\n";
    out << "  fileopctl move <destination> <uri_or_path>... [--wait]\n";
    out << "  fileopctl delete <uri_or_path>... [--wait]\n";
    out << "  fileopctl trash <uri_or_path>... [--wait]\n";
    out << "  fileopctl empty-trash [--wait]\n";
    out << "  fileopctl pause <id>\n";
    out << "  fileopctl resume <id>\n";
    out << "  fileopctl cancel <id>\n";
    out << "  fileopctl get-job <id>\n";
    out << "  fileopctl list-jobs\n";
}

bool hasArg(const QStringList &args, const QString &value)
{
    return args.contains(value);
}

bool serviceRegistered(const QString &name)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return false;
    }
    QDBusReply<bool> reply = iface->isServiceRegistered(name);
    return reply.isValid() && reply.value();
}

class JobWatcher : public QObject
{
    Q_OBJECT
public:
    explicit JobWatcher(const QString &id, QObject *parent = nullptr)
        : QObject(parent), m_id(id)
    {
    }

    bool watch(int timeoutMs)
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface), QStringLiteral("Progress"),
                         this, SLOT(onProgress(QString,int)))) {
            return false;
        }
        bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                    QString::fromLatin1(kIface), QStringLiteral("Finished"),
                    this, SLOT(onFinished(QString)));
        bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                    QString::fromLatin1(kIface), QStringLiteral("Error"),
                    this, SLOT(onError(QString)));
        bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                    QString::fromLatin1(kIface), QStringLiteral("ErrorDetail"),
                    this, SLOT(onErrorDetail(QString,QString,QString)));

        // Avoid missing terminal signals if job already finished/failed
        // before DBus signal subscriptions are active.
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        if (iface.isValid()) {
            QDBusReply<QVariantMap> getJobReply = iface.call(QStringLiteral("GetJob"), m_id);
            if (getJobReply.isValid()) {
                const QVariantMap row = getJobReply.value();
                const QString state = row.value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("finished")) {
                    m_done = true;
                    return true;
                }
                if (state == QStringLiteral("error")) {
                    m_done = true;
                    m_error = true;
                    const QString code = row.value(QStringLiteral("error_code")).toString().trimmed();
                    const QString message = row.value(QStringLiteral("error_message")).toString();
                    if (!code.isEmpty() || !message.isEmpty()) {
                        QTextStream(stderr) << "error " << m_id
                                            << " code=" << (code.isEmpty() ? QStringLiteral("unknown") : code)
                                            << " message=" << (message.isEmpty() ? QStringLiteral("-") : message)
                                            << "\n";
                    }
                    return false;
                }
            }
        }

        if (timeoutMs > 0) {
            QTimer::singleShot(timeoutMs, &m_loop, &QEventLoop::quit);
        }
        m_loop.exec();
        return m_done && !m_error;
    }

private slots:
    void onProgress(const QString &id, int percent)
    {
        if (id != m_id) {
            return;
        }
        QTextStream(stdout) << "progress " << id << " " << percent << "%\n";
    }

    void onFinished(const QString &id)
    {
        if (id != m_id) {
            return;
        }
        m_done = true;
        QTextStream(stdout) << "finished " << id << "\n";
        m_loop.quit();
    }

    void onError(const QString &id)
    {
        if (id != m_id) {
            return;
        }
        m_error = true;
        m_done = true;
        if (m_errorCode.isEmpty() && m_errorMessage.isEmpty()) {
            QTextStream(stderr) << "error " << id << "\n";
        }
        m_loop.quit();
    }

    void onErrorDetail(const QString &id, const QString &code, const QString &message)
    {
        if (id != m_id) {
            return;
        }
        m_error = true;
        m_done = true;
        m_errorCode = code;
        m_errorMessage = message;
        QTextStream(stderr) << "error " << id
                            << " code=" << (code.isEmpty() ? QStringLiteral("unknown") : code)
                            << " message=" << (message.isEmpty() ? QStringLiteral("-") : message)
                            << "\n";
        m_loop.quit();
    }

private:
    QString m_id;
    QEventLoop m_loop;
    bool m_done = false;
    bool m_error = false;
    QString m_errorCode;
    QString m_errorMessage;
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString cmd = args.at(1).trimmed().toLower();
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        printUsage();
        return 0;
    }

    if (!serviceRegistered(QString::fromLatin1(kService))) {
        err << "service unavailable: " << kService << '\n';
        return 2;
    }

    const bool wait = hasArg(args, QStringLiteral("--wait"));
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        err << "cannot open DBus interface: " << kIface << '\n';
        return 2;
    }

    QString id;
    if (cmd == "pause" || cmd == "resume" || cmd == "cancel") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        const QString method = (cmd == "pause") ? QStringLiteral("Pause")
                              : (cmd == "resume") ? QStringLiteral("Resume")
                                                  : QStringLiteral("Cancel");
        QDBusReply<bool> reply = iface.call(method, args.at(2));
        if (!reply.isValid()) {
            return 3;
        }
        out << (reply.value() ? "ok" : "failed") << '\n';
        return reply.value() ? 0 : 4;
    } else if (cmd == "get-job") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetJob"), args.at(2));
        if (!reply.isValid()) {
            return 3;
        }
        const QJsonObject obj = QJsonObject::fromVariantMap(reply.value());
        out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        return 0;
    } else if (cmd == "list-jobs") {
        QDBusReply<QVariantList> reply = iface.call(QStringLiteral("ListJobs"));
        if (!reply.isValid()) {
            return 3;
        }
        QJsonArray arr;
        const QVariantList rows = reply.value();
        for (const QVariant &row : rows) {
            arr.push_back(QJsonObject::fromVariantMap(row.toMap()));
        }
        out << QJsonDocument(arr).toJson(QJsonDocument::Compact) << '\n';
        return 0;
    } else if (cmd == "copy" || cmd == "move") {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        const QString destination = args.at(2);
        QStringList uris;
        for (int i = 3; i < args.size(); ++i) {
            if (args.at(i) == QStringLiteral("--wait")) {
                continue;
            }
            uris.push_back(args.at(i));
        }
        if (uris.isEmpty()) {
            printUsage();
            return 1;
        }
        QDBusReply<QString> reply = iface.call(cmd == "copy" ? QStringLiteral("Copy")
                                                             : QStringLiteral("Move"),
                                               uris, destination);
        if (!reply.isValid()) {
            return 3;
        }
        id = reply.value();
    } else if (cmd == "delete" || cmd == "trash") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        QStringList uris;
        for (int i = 2; i < args.size(); ++i) {
            if (args.at(i) == QStringLiteral("--wait")) {
                continue;
            }
            uris.push_back(args.at(i));
        }
        if (uris.isEmpty()) {
            printUsage();
            return 1;
        }
        QDBusReply<QString> reply = iface.call(cmd == "delete" ? QStringLiteral("Delete")
                                                               : QStringLiteral("Trash"),
                                               uris);
        if (!reply.isValid()) {
            return 3;
        }
        id = reply.value();
    } else if (cmd == "empty-trash") {
        QDBusReply<QString> reply = iface.call(QStringLiteral("EmptyTrash"));
        if (!reply.isValid()) {
            return 3;
        }
        id = reply.value();
    } else {
        err << "unknown command: " << cmd << '\n';
        printUsage();
        return 1;
    }

    out << id << '\n';
    if (!wait) {
        return 0;
    }

    JobWatcher watcher(id);
    const bool ok = watcher.watch(0);
    return ok ? 0 : 4;
}

#include "fileopctl.moc"
