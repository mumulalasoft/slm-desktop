#include "FsdPathPolicy.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringView>
#include <QTextStream>

namespace {

// Built-in protected prefixes with associated scope names.
// Order matters: more specific prefixes should come first.
const struct { const char *prefix; const char *scope; } kBuiltinPrefixes[] = {
    { "/boot",              "boot"     },
    { "/efi",               "boot"     },
    { "/etc",               "etc"      },
    { "/usr",               "system"   },
    { "/bin",               "system"   },
    { "/sbin",              "system"   },
    { "/lib",               "system"   },
    { "/lib64",             "system"   },
    { "/opt",               "opt"      },
    { "/var/lib/slm",       "slm-data" },
    { "/var/cache/slm",     "slm-data" },
    { "/proc",              "pseudo"   },
    { "/sys",               "pseudo"   },
    { "/dev",               "pseudo"   },
    { "/run/systemd",       "pseudo"   },
};

constexpr qsizetype kMaxPathBytes = 4096; // PATH_MAX

} // namespace

FsdPathPolicy::FsdPathPolicy(QObject *parent)
    : QObject(parent)
{
    loadBuiltinDefaults();
}

void FsdPathPolicy::loadConfig()
{
    const QString confDir = QStringLiteral("/etc/slm/fsd/protected-paths.d");
    if (QDir(confDir).exists()) {
        loadFromDir(confDir);
        qInfo().noquote() << "[slm-fsd] loaded path policy from" << confDir;
    } else {
        qInfo().noquote() << "[slm-fsd] no config dir" << confDir << "— using built-in defaults";
    }
}

bool FsdPathPolicy::isProtected(const QString &path, QString *scope) const
{
    // Normalize: remove trailing slashes for comparison
    const QString norm = path.endsWith(QLatin1Char('/')) && path.size() > 1
                             ? path.chopped(1)
                             : path;

    for (const auto &entry : m_entries) {
        if (norm == entry.prefix || norm.startsWith(entry.prefix + QLatin1Char('/'))) {
            if (scope)
                *scope = entry.scope;
            return true;
        }
    }
    return false;
}

QString FsdPathPolicy::validateAndCanonicalize(const QString &rawPath,
                                                QString *errorCode) const
{
    auto setError = [&](const char *code) -> QString {
        if (errorCode)
            *errorCode = QString::fromLatin1(code);
        return {};
    };

    // 1. Size check (before UTF-8 decode to avoid large allocation)
    if (rawPath.size() > kMaxPathBytes)
        return setError("PathTooLong");

    // 2. Must be non-empty and absolute
    if (rawPath.isEmpty() || !rawPath.startsWith(QLatin1Char('/')))
        return setError("PathNotAbsolute");

    // 3. UTF-8 validity (Qt internally stores as UTF-16; check round-trip)
    const QByteArray utf8 = rawPath.toUtf8();
    if (QString::fromUtf8(utf8) != rawPath)
        return setError("PathInvalidEncoding");

    // 4. Reject raw ".." components before we even try to canonicalize.
    //    This stops obvious traversal attempts early.
    const QStringList parts = rawPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".."))
            return setError("PathTraversal");
    }

    // 5. Canonicalize via QFileInfo (resolves symlinks in parent components).
    //    Note: the final component may not exist yet (e.g. RequestCreate), so
    //    we use QDir::cleanPath for syntactic normalization first.
    const QString cleaned = QDir::cleanPath(rawPath);
    QFileInfo fi(cleaned);

    // For existing paths, resolve to canonical (follows symlinks).
    // For non-existing paths, cleaned is the best we can do safely.
    const QString canonical = fi.exists() ? fi.canonicalFilePath() : cleaned;

    if (canonical.isEmpty())
        return setError("PathResolutionFailed");

    // 6. After canonicalization, ensure there are still no ".." (shouldn't happen
    //    after cleanPath, but be defensive).
    if (canonical.contains(QLatin1String("/../")) ||
        canonical.endsWith(QLatin1String("/.."))) {
        return setError("PathTraversal");
    }

    return canonical;
}

QStringList FsdPathPolicy::protectedPrefixes() const
{
    QStringList result;
    result.reserve(m_entries.size());
    for (const auto &e : m_entries)
        result << e.prefix;
    return result;
}

// ── Private ──────────────────────────────────────────────────────────────────

void FsdPathPolicy::loadBuiltinDefaults()
{
    for (const auto &kv : kBuiltinPrefixes) {
        ProtectedEntry entry;
        entry.prefix = QString::fromLatin1(kv.prefix);
        entry.scope  = QString::fromLatin1(kv.scope);
        m_entries << entry;
    }
}

void FsdPathPolicy::loadFromDir(const QString &dirPath)
{
    const QDir dir(dirPath);
    const QStringList files = dir.entryList({QStringLiteral("*.conf")},
                                            QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning().noquote() << "[slm-fsd] cannot read" << f.fileName();
            continue;
        }
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            if (!line.startsWith(QLatin1Char('/'))) {
                qWarning().noquote() << "[slm-fsd] ignoring non-absolute prefix in"
                                     << fileName << ":" << line;
                continue;
            }
            ProtectedEntry entry;
            entry.prefix = line;
            entry.scope  = scopeForPrefix(line);
            // Avoid duplicates
            bool dup = false;
            for (const auto &existing : std::as_const(m_entries)) {
                if (existing.prefix == entry.prefix) { dup = true; break; }
            }
            if (!dup)
                m_entries << entry;
        }
    }
}

QString FsdPathPolicy::scopeForPrefix(const QString &prefix) const
{
    // Check against built-in to reuse the scope label if it matches
    for (const auto &kv : kBuiltinPrefixes) {
        if (prefix == QString::fromLatin1(kv.prefix))
            return QString::fromLatin1(kv.scope);
    }
    return QStringLiteral("custom");
}
