#include "pathmanagercontroller.h"

#include "envserviceclient.h"

#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QClipboard>

namespace {
static const QRegularExpression kPathSplitRe(QStringLiteral(":+"));
static constexpr auto kPathKey = "PATH";
static constexpr auto kDisabledPathKey = "SLM_PATH_DISABLED";
}

PathManagerController::PathManagerController(EnvServiceClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    if (m_client) {
        connect(m_client, &EnvServiceClient::serviceAvailableChanged, this, &PathManagerController::refresh);
        connect(m_client, &EnvServiceClient::userVarsChanged, this, &PathManagerController::refresh);
        connect(m_client, &EnvServiceClient::sessionVarsChanged, this, &PathManagerController::refresh);
        connect(m_client, &EnvServiceClient::appVarsChanged, this, [this](const QString &changedAppId) {
            if (m_scope == QLatin1String("app") && changedAppId == m_appId) {
                refresh();
            } else {
                reloadAppIds();
            }
        });
    }
    refresh();
}

QString PathManagerController::scope() const { return m_scope; }
QString PathManagerController::appId() const { return m_appId; }
QString PathManagerController::filterText() const { return m_filterText; }
QVariantList PathManagerController::entries() const { return m_entriesVariant; }
QVariantList PathManagerController::filteredEntries() const { return m_filteredEntries; }
QStringList PathManagerController::finalPath() const { return m_finalPath; }
bool PathManagerController::loading() const { return m_loading; }
QString PathManagerController::statusText() const { return m_statusText; }
QString PathManagerController::lastError() const { return m_lastError; }
int PathManagerController::selectedIndex() const { return m_selectedIndex; }
QStringList PathManagerController::appIds() const { return m_appIds; }

QVariantMap PathManagerController::selectedEntry() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_entriesVariant.size()) {
        return {};
    }
    return m_entriesVariant[m_selectedIndex].toMap();
}

void PathManagerController::setScope(const QString &scopeValue)
{
    const QString normalized = scopeValue.trimmed().toLower();
    if (normalized.isEmpty() || m_scope == normalized) {
        return;
    }
    m_scope = normalized;
    emit scopeChanged();
    refresh();
}

void PathManagerController::setAppId(const QString &appIdValue)
{
    const QString trimmed = appIdValue.trimmed();
    if (m_appId == trimmed) {
        return;
    }
    m_appId = trimmed;
    emit appIdChanged();
    if (m_scope == QLatin1String("app")) {
        refresh();
    }
}

void PathManagerController::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    m_filterText = text;
    emit filterTextChanged();
    rebuildFilteredEntries();
}

void PathManagerController::setSelectedIndex(int index)
{
    const int bounded = (index >= 0 && index < m_entriesVariant.size()) ? index : -1;
    if (m_selectedIndex == bounded) {
        return;
    }
    m_selectedIndex = bounded;
    emit selectedIndexChanged();
    emit selectedEntryChanged();
}

void PathManagerController::refresh()
{
    m_loading = true;
    emit loadingChanged();
    setLastError(QString());
    reloadAppIds();

    QVector<Entry> loaded;
    const QString pathValue = readScopeVar(QLatin1String(kPathKey));
    const QString disabledValue = readScopeVar(QLatin1String(kDisabledPathKey));
    const QSet<QString> disabledSet = QSet<QString>(
        splitPath(disabledValue).cbegin(), splitPath(disabledValue).cend());

    const QStringList paths = splitPath(pathValue);
    for (const QString &rawPath : paths) {
        Entry e;
        e.path = rawPath;
        e.canonicalPath = canonicalizePath(rawPath);
        e.source = (m_scope == QLatin1String("global"))
                   ? QStringLiteral("User")
                   : (m_scope == QLatin1String("session")
                      ? QStringLiteral("Session")
                      : QStringLiteral("App Override"));
        e.editable = true;
        e.enabled = !disabledSet.contains(e.canonicalPath);
        loaded.append(e);
    }

    annotateEntries(loaded);
    m_entries = loaded;
    m_entriesVariant = toVariantList(m_entries);
    emit entriesChanged();
    rebuildFilteredEntries();

    m_finalPath = currentPathForResolution();
    emit finalPathChanged();

    m_statusText = tr("%1 entries").arg(m_entries.size());
    emit statusTextChanged();

    if (m_selectedIndex >= m_entriesVariant.size()) {
        setSelectedIndex(m_entriesVariant.isEmpty() ? -1 : 0);
    } else {
        emit selectedEntryChanged();
    }

    m_loading = false;
    emit loadingChanged();
}

QVariantMap PathManagerController::validatePath(const QString &path) const
{
    QVariantMap out;
    const QString normalized = normalizePathCandidate(path);
    const QFileInfo info(normalized);

    out[QStringLiteral("inputPath")] = path;
    out[QStringLiteral("normalizedPath")] = normalized;
    out[QStringLiteral("isAbsolute")] = QDir::isAbsolutePath(normalized);
    out[QStringLiteral("exists")] = info.exists();
    out[QStringLiteral("isDir")] = info.isDir();
    out[QStringLiteral("readable")] = info.isReadable();

    if (!QDir::isAbsolutePath(normalized)) {
        out[QStringLiteral("valid")] = false;
        out[QStringLiteral("severity")] = QStringLiteral("error");
        out[QStringLiteral("message")] = tr("Path must be absolute.");
        return out;
    }
    if (!info.exists() || !info.isDir()) {
        out[QStringLiteral("valid")] = false;
        out[QStringLiteral("severity")] = QStringLiteral("error");
        out[QStringLiteral("message")] = tr("Folder not found.");
        return out;
    }
    if (!info.isReadable()) {
        out[QStringLiteral("valid")] = false;
        out[QStringLiteral("severity")] = QStringLiteral("error");
        out[QStringLiteral("message")] = tr("Folder is not readable.");
        return out;
    }

    const QStringList executables = scanExecutablesPreview(normalized, 64);
    out[QStringLiteral("executables")] = executables;
    out[QStringLiteral("executableCount")] = executables.size();

    if (executables.isEmpty()) {
        out[QStringLiteral("valid")] = true;
        out[QStringLiteral("severity")] = QStringLiteral("warning");
        out[QStringLiteral("message")] = tr("Folder does not contain executables.");
    } else {
        out[QStringLiteral("valid")] = true;
        out[QStringLiteral("severity")] = QStringLiteral("ok");
        out[QStringLiteral("message")] = tr("Ready to add.");
    }
    return out;
}

QVariantMap PathManagerController::previewAddFolder(const QString &path) const
{
    QVariantMap validation = validatePath(path);
    const QString normalized = validation.value(QStringLiteral("normalizedPath")).toString();
    const bool flutterDetected = QFileInfo::exists(normalized + QStringLiteral("/flutter"))
                                 || QFileInfo::exists(normalized + QStringLiteral("/dart"));
    const bool hasFlutterRoot = QFileInfo::exists(normalized + QStringLiteral("/bin/flutter"));
    QString recommended = normalized;
    if (hasFlutterRoot) {
        recommended = QDir(normalized).filePath(QStringLiteral("bin"));
    }

    validation[QStringLiteral("flutterDetected")] = flutterDetected || hasFlutterRoot;
    validation[QStringLiteral("recommendedPath")] = recommended;
    validation[QStringLiteral("recommendedPosition")] = QStringLiteral("top");
    return validation;
}

bool PathManagerController::addFolder(const QString &path, const QString &position)
{
    const QVariantMap preview = previewAddFolder(path);
    if (!preview.value(QStringLiteral("valid")).toBool()) {
        setLastError(preview.value(QStringLiteral("message")).toString());
        return false;
    }
    Entry e;
    e.path = preview.value(QStringLiteral("recommendedPath")).toString();
    e.canonicalPath = canonicalizePath(e.path);
    e.source = (m_scope == QLatin1String("global"))
               ? QStringLiteral("User")
               : (m_scope == QLatin1String("session")
                  ? QStringLiteral("Session")
                  : QStringLiteral("App Override"));
    e.enabled = true;
    e.editable = true;

    if (position == QLatin1String("top")) {
        m_entries.prepend(e);
    } else {
        m_entries.append(e);
    }
    annotateEntries(m_entries);
    if (!writeScopeVar(QLatin1String(kPathKey), joinPath(m_entries, true))) {
        return false;
    }
    const QString disabled = joinPath(m_entries, false);
    if (!writeScopeVar(QLatin1String(kDisabledPathKey), disabled)) {
        return false;
    }
    refresh();
    return true;
}

bool PathManagerController::removeEntry(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    m_entries.removeAt(index);
    annotateEntries(m_entries);
    if (!writeScopeVar(QLatin1String(kPathKey), joinPath(m_entries, true))) {
        return false;
    }
    if (!writeScopeVar(QLatin1String(kDisabledPathKey), joinPath(m_entries, false))) {
        return false;
    }
    refresh();
    return true;
}

bool PathManagerController::moveEntry(int from, int to)
{
    if (from < 0 || from >= m_entries.size() || to < 0 || to >= m_entries.size() || from == to) {
        return false;
    }
    m_entries.move(from, to);
    annotateEntries(m_entries);
    if (!writeScopeVar(QLatin1String(kPathKey), joinPath(m_entries, true))) {
        return false;
    }
    if (!writeScopeVar(QLatin1String(kDisabledPathKey), joinPath(m_entries, false))) {
        return false;
    }
    refresh();
    return true;
}

bool PathManagerController::setEntryEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    m_entries[index].enabled = enabled;
    annotateEntries(m_entries);
    if (!writeScopeVar(QLatin1String(kPathKey), joinPath(m_entries, true))) {
        return false;
    }
    if (!writeScopeVar(QLatin1String(kDisabledPathKey), joinPath(m_entries, false))) {
        return false;
    }
    refresh();
    return true;
}

bool PathManagerController::resetScopePath()
{
    if (!writeScopeVar(QLatin1String(kPathKey), QString())) {
        return false;
    }
    if (!writeScopeVar(QLatin1String(kDisabledPathKey), QString())) {
        return false;
    }
    refresh();
    return true;
}

QVariantMap PathManagerController::resolveCommand(const QString &command) const
{
    QVariantMap out;
    const QString cmd = command.trimmed();
    if (cmd.isEmpty()) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("error")] = tr("Command is empty.");
        return out;
    }

    QStringList candidates;
    const QStringList paths = currentPathForResolution();
    for (const QString &p : paths) {
        const QFileInfo info(QDir(p).filePath(cmd));
        if (info.exists() && info.isFile() && info.isExecutable()) {
            candidates.append(info.absoluteFilePath());
        }
    }

    out[QStringLiteral("ok")] = !candidates.isEmpty();
    out[QStringLiteral("resolved")] = candidates.isEmpty() ? QString() : candidates.first();
    out[QStringLiteral("candidates")] = candidates;
    out[QStringLiteral("message")] = candidates.isEmpty()
                                     ? tr("Command not found in current PATH.")
                                     : tr("Resolved command path.");
    return out;
}

QStringList PathManagerController::detectMissingRecommendations() const
{
    QStringList recommendations;
    const QString home = QDir::homePath();
    const QString flutterBin = QDir(home).filePath(QStringLiteral("flutter/bin"));
    if (QFileInfo::exists(QDir(home).filePath(QStringLiteral("flutter/bin/flutter")))) {
        if (!m_finalPath.contains(flutterBin)) {
            recommendations.append(flutterBin);
        }
    }
    return recommendations;
}

QVariantMap PathManagerController::applyRecommendation(const QString &path)
{
    const bool ok = addFolder(path, QStringLiteral("top"));
    return {
        { QStringLiteral("ok"), ok },
        { QStringLiteral("error"), ok ? QString() : m_lastError }
    };
}

bool PathManagerController::openPathInFileManager(const QString &path) const
{
    const QString normalized = normalizePathCandidate(path);
    if (normalized.isEmpty()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(normalized));
}

bool PathManagerController::copyPathToClipboard(const QString &path) const
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    const QString normalized = normalizePathCandidate(path);
    clipboard->setText(normalized);
    return true;
}

QString PathManagerController::normalizePathCandidate(const QString &path) const
{
    QString p = path.trimmed();
    if (p.startsWith(QStringLiteral("file://"))) {
        p = QUrl(p).toLocalFile();
    }
    if (p.startsWith(QStringLiteral("~"))) {
        p.replace(0, 1, QDir::homePath());
    }
    return QDir::cleanPath(p);
}

QString PathManagerController::canonicalizePath(const QString &path) const
{
    const QString normalized = normalizePathCandidate(path);
    const QFileInfo info(normalized);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? normalized : canonical;
}

QStringList PathManagerController::splitPath(const QString &value) const
{
    QString cleaned = value;
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }
    const QStringList parts = cleaned.split(kPathSplitRe, Qt::SkipEmptyParts);
    QStringList out;
    out.reserve(parts.size());
    for (const QString &part : parts) {
        const QString p = normalizePathCandidate(part);
        if (!p.isEmpty()) {
            out.append(p);
        }
    }
    return out;
}

QString PathManagerController::joinPath(const QVector<Entry> &entries, bool enabledOnly) const
{
    QStringList out;
    out.reserve(entries.size());
    for (const Entry &e : entries) {
        if (enabledOnly) {
            if (e.enabled) {
                out.append(normalizePathCandidate(e.path));
            }
        } else if (!e.enabled) {
            out.append(e.canonicalPath);
        }
    }
    return out.join(QLatin1Char(':'));
}

QVariantList PathManagerController::toVariantList(const QVector<Entry> &entries) const
{
    QVariantList out;
    out.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const Entry &e = entries[i];
        QVariantMap row;
        row[QStringLiteral("index")] = i;
        row[QStringLiteral("path")] = e.path;
        row[QStringLiteral("canonicalPath")] = e.canonicalPath;
        row[QStringLiteral("source")] = e.source;
        row[QStringLiteral("enabled")] = e.enabled;
        row[QStringLiteral("editable")] = e.editable;
        row[QStringLiteral("missing")] = e.missing;
        row[QStringLiteral("duplicate")] = e.duplicate;
        row[QStringLiteral("status")] = e.status;
        row[QStringLiteral("warning")] = e.warning;
        row[QStringLiteral("executablesPreview")] = e.executables;
        out.append(row);
    }
    return out;
}

void PathManagerController::annotateEntries(QVector<Entry> &entries) const
{
    QHash<QString, int> countByCanonical;
    for (const Entry &e : entries) {
        countByCanonical[e.canonicalPath] += 1;
    }
    for (Entry &e : entries) {
        const QFileInfo info(e.path);
        e.missing = !info.exists() || !info.isDir() || !info.isReadable();
        e.duplicate = countByCanonical.value(e.canonicalPath) > 1;
        e.executables = scanExecutablesPreview(e.path, 3);
        if (!e.enabled) {
            e.status = QStringLiteral("Disabled");
            e.warning.clear();
        } else if (e.missing) {
            e.status = QStringLiteral("Missing");
            e.warning = tr("Folder not found.");
        } else if (e.duplicate) {
            e.status = QStringLiteral("Duplicate");
            e.warning = tr("Path is duplicated.");
        } else {
            e.status = QStringLiteral("Active");
            e.warning = e.executables.isEmpty() ? tr("Folder does not contain executables.") : QString();
        }
    }
}

QStringList PathManagerController::scanExecutablesPreview(const QString &path, int maxItems) const
{
    QStringList out;
    if (maxItems <= 0) {
        return out;
    }
    const QDir dir(path);
    if (!dir.exists()) {
        return out;
    }
    const QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : list) {
        if (fi.isExecutable()) {
            out.append(fi.fileName());
            if (out.size() >= maxItems) {
                break;
            }
        }
    }
    return out;
}

bool PathManagerController::writeScopeVar(const QString &key, const QString &value)
{
    if (!m_client) {
        setLastError(tr("Environment service unavailable."));
        return false;
    }
    const QString trimmedKey = key.trimmed();
    const QString trimmedValue = value.trimmed();

    bool ok = false;
    if (m_scope == QLatin1String("global")) {
        QVariantList vars = m_client->getUserVars();
        bool exists = false;
        for (const QVariant &v : vars) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("key")).toString() == trimmedKey) {
                exists = true;
                break;
            }
        }
        if (trimmedValue.isEmpty()) {
            ok = !exists || m_client->deleteUserVar(trimmedKey);
        } else if (exists) {
            ok = m_client->updateUserVar(trimmedKey, trimmedValue, QString(), QStringLiteral("replace"));
        } else {
            ok = m_client->addUserVar(trimmedKey, trimmedValue, QString(), QStringLiteral("replace"));
        }
    } else if (m_scope == QLatin1String("session")) {
        if (trimmedValue.isEmpty()) {
            ok = m_client->unsetSessionVar(trimmedKey);
        } else {
            ok = m_client->setSessionVar(trimmedKey, trimmedValue, QStringLiteral("replace"));
        }
    } else {
        if (m_appId.isEmpty()) {
            setLastError(tr("Select an app first for App scope."));
            return false;
        }
        m_client->removeAppVar(m_appId, trimmedKey);
        if (trimmedValue.isEmpty()) {
            ok = true;
        } else {
            ok = m_client->addAppVar(m_appId, trimmedKey, trimmedValue, QStringLiteral("replace"));
        }
    }

    if (!ok) {
        setLastError(m_client->lastError().isEmpty() ? tr("Failed to write PATH entries.") : m_client->lastError());
    } else {
        setLastError(QString());
    }
    return ok;
}

QString PathManagerController::readScopeVar(const QString &key) const
{
    if (!m_client) {
        return {};
    }
    const QString target = key.trimmed();
    QVariantList vars;
    if (m_scope == QLatin1String("global")) {
        vars = m_client->getUserVars();
    } else if (m_scope == QLatin1String("session")) {
        vars = m_client->getSessionVars();
    } else {
        if (m_appId.isEmpty()) {
            return {};
        }
        vars = m_client->getAppVars(m_appId);
    }
    for (const QVariant &v : vars) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("key")).toString() == target) {
            return row.value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

void PathManagerController::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void PathManagerController::rebuildFilteredEntries()
{
    if (m_filterText.trimmed().isEmpty()) {
        m_filteredEntries = m_entriesVariant;
    } else {
        m_filteredEntries.clear();
        const QString needle = m_filterText.trimmed().toLower();
        for (const QVariant &v : m_entriesVariant) {
            const QVariantMap row = v.toMap();
            const QString path = row.value(QStringLiteral("path")).toString().toLower();
            const QString source = row.value(QStringLiteral("source")).toString().toLower();
            const QString status = row.value(QStringLiteral("status")).toString().toLower();
            if (path.contains(needle) || source.contains(needle) || status.contains(needle)) {
                m_filteredEntries.append(row);
            }
        }
    }
    emit filteredEntriesChanged();
}

QStringList PathManagerController::currentPathForResolution() const
{
    QStringList localEnabled;
    for (const Entry &e : m_entries) {
        if (e.enabled) {
            localEnabled.append(normalizePathCandidate(e.path));
        }
    }
    if (!m_client || !m_client->serviceAvailable()) {
        return localEnabled;
    }

    QVariantMap resolved = m_client->resolveEnv(m_scope == QLatin1String("app") ? m_appId : QString());
    const QVariantMap pathMap = resolved.value(QStringLiteral("PATH")).toMap();
    const QString resolvedPath = pathMap.value(QStringLiteral("value")).toString();
    if (resolvedPath.isEmpty()) {
        return localEnabled;
    }
    return splitPath(resolvedPath);
}

void PathManagerController::reloadAppIds()
{
    if (!m_client || !m_client->serviceAvailable()) {
        if (!m_appIds.isEmpty()) {
            m_appIds.clear();
            emit appIdsChanged();
        }
        return;
    }
    const QStringList next = m_client->listAppsWithOverrides();
    if (m_appIds == next) {
        return;
    }
    m_appIds = next;
    emit appIdsChanged();
}
