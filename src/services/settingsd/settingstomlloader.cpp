#include "settingstomlloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QRegularExpression>
#include <QTextStream>

namespace Slm {

// ── Parse ─────────────────────────────────────────────────────────────────────

// Returns a flat dotted-key map from a TOML text.
// Supports:
//   [section]
//   [section.subsection]
//   key = "string"
//   key = 42
//   key = 3.14
//   key = true / false
//   # comment lines
QVariantMap SettingsTomlLoader::parseToml(const QString &text, const QString &sourcePath)
{
    QVariantMap result;
    QString section;

    // Matches: key = value (with optional leading/trailing whitespace on both sides)
    // Quoted-string: key = "..."
    // Unquoted: key = true | false | 42 | 3.14
    static const QRegularExpression reSectionHeader(
        QStringLiteral(R"(^\s*\[([A-Za-z0-9_\.\-]+)\]\s*(?:#.*)?$)"));
    static const QRegularExpression reKeyValue(
        QStringLiteral(R"(^\s*([A-Za-z0-9_\-]+)\s*=\s*(.*?)\s*(?:#.*)?$)"));

    int lineNo = 0;
    for (const QStringView line : QStringView(text).split(u'\n')) {
        ++lineNo;
        const QString trimmed = line.trimmed().toString();

        if (trimmed.isEmpty() || trimmed.startsWith(u'#'))
            continue;

        QRegularExpressionMatch m = reSectionHeader.match(trimmed);
        if (m.hasMatch()) {
            section = m.captured(1);
            continue;
        }

        m = reKeyValue.match(trimmed);
        if (!m.hasMatch()) {
            qWarning("settingsd TOML [%s:%d]: unrecognised line, skipping",
                     qPrintable(sourcePath), lineNo);
            continue;
        }

        const QString rawKey   = m.captured(1);
        const QString rawValue = m.captured(2);
        const QString fullKey  = section.isEmpty() ? rawKey
                                                   : section + u'.' + rawKey;

        // Parse value
        QVariant v;

        if (rawValue.startsWith(u'"') && rawValue.endsWith(u'"') && rawValue.size() >= 2) {
            // Quoted string — strip delimiters, handle basic escapes
            QString s = rawValue.mid(1, rawValue.size() - 2);
            s.replace(QStringLiteral("\\n"),  QStringLiteral("\n"));
            s.replace(QStringLiteral("\\t"),  QStringLiteral("\t"));
            s.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
            s.replace(QStringLiteral("\\\""), QStringLiteral("\""));
            v = s;
        } else if (rawValue == QStringLiteral("true")) {
            v = true;
        } else if (rawValue == QStringLiteral("false")) {
            v = false;
        } else {
            // Try integer, then float
            bool ok = false;
            const int iv = rawValue.toInt(&ok);
            if (ok) {
                v = iv;
            } else {
                const double dv = rawValue.toDouble(&ok);
                if (ok) {
                    v = dv;
                } else {
                    qWarning("settingsd TOML [%s:%d]: cannot parse value for '%s', skipping",
                             qPrintable(sourcePath), lineNo, qPrintable(fullKey));
                    continue;
                }
            }
        }

        result.insert(fullKey, v);
    }

    return result;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool SettingsTomlLoader::loadFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.exists())
        return true; // Missing system-defaults file is normal — not an error.
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("cannot open TOML file: %1 — %2")
                         .arg(path, f.errorString());
        return false;
    }
    QTextStream in(&f);
    const QString text = in.readAll();
    const QVariantMap parsed = parseToml(text, path);
    for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it)
        m_values.insert(it.key(), it.value());
    return true;
}

void SettingsTomlLoader::loadDirectory(const QString &dirPath)
{
    const QDir dir(dirPath);
    if (!dir.exists())
        return;
    const QFileInfoList files = dir.entryInfoList(
        QStringList{QStringLiteral("*.toml")}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files)
        loadFile(fi.absoluteFilePath());
}

bool SettingsTomlLoader::has(const QString &key) const
{
    return m_values.contains(key);
}

QVariant SettingsTomlLoader::get(const QString &key) const
{
    return m_values.value(key);
}

const QVariantMap &SettingsTomlLoader::all() const
{
    return m_values;
}

} // namespace Slm
