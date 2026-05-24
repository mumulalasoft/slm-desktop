#pragma once

#include <QFileInfo>
#include <QString>
#include <QUrl>

namespace UrlUtils {
inline QString fileSchemePrefix()
{
    return QStringLiteral("file://");
}

inline bool isFileUrl(const QString &value)
{
    return value.trimmed().startsWith(fileSchemePrefix(), Qt::CaseInsensitive);
}

inline QString toFileUrl(const QString &localPath)
{
    return fileSchemePrefix() + localPath;
}

inline QString localPathFromFileUrl(const QString &value)
{
    if (!isFileUrl(value)) {
        return QString();
    }
    const QUrl url(value.trimmed());
    if (!url.isValid() || !url.isLocalFile()) {
        return QString();
    }
    return url.toLocalFile();
}

inline QString localPathFromUriOrPath(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QString fromUrl = localPathFromFileUrl(trimmed);
    return fromUrl.isEmpty() ? trimmed : fromUrl;
}

inline QString absoluteLocalPathFromUriOrText(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const int uriPos = trimmed.indexOf(fileSchemePrefix());
    const QString token = uriPos >= 0 ? trimmed.mid(uriPos) : trimmed;
    const QUrl url = QUrl::fromUserInput(token);
    if (url.isValid() && url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).absoluteFilePath();
    }
    if (token.startsWith(QLatin1Char('/'))) {
        return QFileInfo(token).absoluteFilePath();
    }
    return QString();
}
} // namespace UrlUtils

