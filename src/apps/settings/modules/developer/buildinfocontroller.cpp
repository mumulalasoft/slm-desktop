#include "buildinfocontroller.h"

#include <QLibraryInfo>
#include <QSysInfo>
#include <QVariantMap>

BuildInfoController::BuildInfoController(QObject *parent)
    : QObject(parent)
{
    build();
}

void BuildInfoController::addSection(const QString &group,
                                      const QString &key,
                                      const QString &value)
{
    QVariantMap entry;
    entry[QStringLiteral("group")] = group;
    entry[QStringLiteral("key")]   = key;
    entry[QStringLiteral("value")] = value;
    m_sections.append(entry);
}

void BuildInfoController::build()
{
    // ── Shell ──────────────────────────────────────────────────────────────
    m_shellVersion = QStringLiteral(SLM_SHELL_VERSION);
    addSection(QStringLiteral("Shell"), QStringLiteral("Version"), m_shellVersion);

    // ── Qt ─────────────────────────────────────────────────────────────────
    m_qtVersion = QString::fromLatin1(qVersion());
    addSection(QStringLiteral("Qt"), QStringLiteral("Version"), m_qtVersion);
    addSection(QStringLiteral("Qt"), QStringLiteral("Install Prefix"),
               QLibraryInfo::path(QLibraryInfo::PrefixPath));
    addSection(QStringLiteral("Qt"), QStringLiteral("QML Imports"),
               QLibraryInfo::path(QLibraryInfo::QmlImportsPath));

    // ── Build ──────────────────────────────────────────────────────────────
#ifdef QT_DEBUG
    m_buildType = QStringLiteral("debug");
#else
    m_buildType = QStringLiteral("release");
#endif
    addSection(QStringLiteral("Build"), QStringLiteral("Type"), m_buildType);

    // Compiler info via predefined macros.
#if defined(__clang__)
    m_compilerInfo = QStringLiteral("Clang %1.%2.%3")
                         .arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__);
#elif defined(__GNUC__)
    m_compilerInfo = QStringLiteral("GCC %1.%2.%3")
                         .arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#else
    m_compilerInfo = QStringLiteral("Unknown");
#endif
    addSection(QStringLiteral("Build"), QStringLiteral("Compiler"), m_compilerInfo);
    addSection(QStringLiteral("Build"), QStringLiteral("Date"),
               QStringLiteral(__DATE__ " " __TIME__));

    // ── Runtime / Session ──────────────────────────────────────────────────
    const QByteArray waylandDisplay = qgetenv("WAYLAND_DISPLAY");
    const QByteArray x11Display     = qgetenv("DISPLAY");
    if (!waylandDisplay.isEmpty()) {
        m_sessionType   = QStringLiteral("wayland");
        m_displayServer = QString::fromLocal8Bit(waylandDisplay);
    } else if (!x11Display.isEmpty()) {
        m_sessionType   = QStringLiteral("x11");
        m_displayServer = QString::fromLocal8Bit(x11Display);
    } else {
        m_sessionType   = QStringLiteral("unknown");
        m_displayServer = QStringLiteral("—");
    }
    addSection(QStringLiteral("Session"), QStringLiteral("Type"), m_sessionType);
    addSection(QStringLiteral("Session"), QStringLiteral("Display"), m_displayServer);

    const QByteArray dbusAddr = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    addSection(QStringLiteral("Session"), QStringLiteral("D-Bus Address"),
               dbusAddr.isEmpty() ? QStringLiteral("—") : QString::fromLocal8Bit(dbusAddr));

    const QByteArray xdgRuntime = qgetenv("XDG_RUNTIME_DIR");
    addSection(QStringLiteral("Session"), QStringLiteral("XDG_RUNTIME_DIR"),
               xdgRuntime.isEmpty() ? QStringLiteral("—") : QString::fromLocal8Bit(xdgRuntime));

    // ── System ─────────────────────────────────────────────────────────────
    m_kernelVersion = QSysInfo::kernelVersion();
    m_machineArch   = QSysInfo::currentCpuArchitecture();
    addSection(QStringLiteral("System"), QStringLiteral("Kernel"),
               QSysInfo::kernelType() + QStringLiteral(" ") + m_kernelVersion);
    addSection(QStringLiteral("System"), QStringLiteral("Architecture"), m_machineArch);
    addSection(QStringLiteral("System"), QStringLiteral("Product"),
               QSysInfo::prettyProductName());

    // GL renderer comes from env (set by Wayland compositor or Mesa).
    const QByteArray glRenderer = qgetenv("MESA_GL_VERSION_OVERRIDE");
    m_glRenderer = glRenderer.isEmpty()
                   ? qgetenv("__GLX_VENDOR_LIBRARY_NAME")
                   : QString::fromLocal8Bit(glRenderer);
    if (m_glRenderer.isEmpty())
        m_glRenderer = QStringLiteral("—");
    addSection(QStringLiteral("System"), QStringLiteral("GL Renderer"), m_glRenderer);
}

QString BuildInfoController::allAsText() const
{
    QString out;
    QString currentGroup;
    for (const QVariant &v : m_sections) {
        const QVariantMap m = v.toMap();
        const QString group = m[QStringLiteral("group")].toString();
        const QString key   = m[QStringLiteral("key")].toString();
        const QString value = m[QStringLiteral("value")].toString();
        if (group != currentGroup) {
            if (!out.isEmpty()) out += QLatin1Char('\n');
            out += QStringLiteral("[%1]\n").arg(group);
            currentGroup = group;
        }
        out += QStringLiteral("  %1: %2\n").arg(key, value);
    }
    return out;
}
