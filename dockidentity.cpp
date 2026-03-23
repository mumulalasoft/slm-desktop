#include "dockidentity.h"

#include <QRegularExpression>

namespace DockIdentity {

QString normalizeToken(const QString &value)
{
    QString out = value.trimmed().toLower();
    out.remove(QRegularExpression(QStringLiteral("[^a-z0-9._-]")));
    return out;
}

bool isGenericWrapperToken(const QString &tokenRaw)
{
    const QString token = normalizeToken(tokenRaw);
    return token == QStringLiteral("flatpak") || token == QStringLiteral("snap") ||
           token == QStringLiteral("gtklaunch") || token == QStringLiteral("gio");
}

bool isWrapperLikeToken(const QString &tokenRaw)
{
    const QString token = normalizeToken(tokenRaw);
    if (token.isEmpty()) {
        return true;
    }
    if (isGenericWrapperToken(token)) {
        return true;
    }
    return token == QStringLiteral("usrbinflatpak") ||
           token == QStringLiteral("usrbinsnap") ||
           token.endsWith(QStringLiteral("flatpak")) ||
           token.endsWith(QStringLiteral("gtklaunch"));
}

bool isWeakIdentityToken(const QString &tokenRaw)
{
    const QString token = normalizeToken(tokenRaw);
    if (token.isEmpty()) {
        return true;
    }
    if (isWrapperLikeToken(token)) {
        return true;
    }
    static const QSet<QString> weak{
        QStringLiteral("app"),
        QStringLiteral("apps"),
        QStringLiteral("application"),
        QStringLiteral("desktop"),
        QStringLiteral("launcher"),
        QStringLiteral("service"),
        QStringLiteral("code"),
    };
    return weak.contains(token);
}

bool isStrongIdentityToken(const QString &tokenRaw)
{
    const QString token = normalizeToken(tokenRaw);
    if (token.size() < 5) {
        return false;
    }
    if (isWeakIdentityToken(token)) {
        return false;
    }
    return token.contains('.') || token.contains('-') || token.contains('_') || token.size() >= 8;
}

QSet<QString> filterLaunchHintTokens(const QSet<QString> &tokens)
{
    QSet<QString> out;
    for (const QString &tokenRaw : tokens) {
        const QString token = normalizeToken(tokenRaw);
        if (!isStrongIdentityToken(token)) {
            continue;
        }
        out.insert(token);
    }
    return out;
}

} // namespace DockIdentity

