#pragma once

#include <QSet>
#include <QString>

namespace DockIdentity {

QString normalizeToken(const QString &value);
bool isGenericWrapperToken(const QString &tokenRaw);
bool isWrapperLikeToken(const QString &tokenRaw);
bool isWeakIdentityToken(const QString &tokenRaw);
bool isStrongIdentityToken(const QString &tokenRaw);
QSet<QString> filterLaunchHintTokens(const QSet<QString> &tokens);

} // namespace DockIdentity

