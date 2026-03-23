#include "PortalAppContextResolver.h"

namespace Slm::PortalAdapter {
namespace {

QString normalizeDesktopAppId(QString appId)
{
    appId = appId.trimmed();
    if (appId.isEmpty()) {
        return appId;
    }
    if (!appId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        appId += QStringLiteral(".desktop");
    }
    return appId;
}

void parseParentWindow(const QString &raw,
                       QString *typeOut,
                       QString *idOut,
                       bool *validOut)
{
    QString type = QStringLiteral("none");
    QString id;
    bool valid = true;

    const QString text = raw.trimmed();
    if (!text.isEmpty()) {
        if (text.startsWith(QStringLiteral("wayland:"), Qt::CaseInsensitive)) {
            type = QStringLiteral("wayland");
            id = text.mid(8).trimmed();
            valid = !id.isEmpty();
        } else if (text.startsWith(QStringLiteral("x11:"), Qt::CaseInsensitive)) {
            type = QStringLiteral("x11");
            id = text.mid(4).trimmed();
            valid = !id.isEmpty();
        } else {
            type = QStringLiteral("invalid");
            id = text;
            valid = false;
        }
    }

    if (typeOut) {
        *typeOut = type;
    }
    if (idOut) {
        *idOut = id;
    }
    if (validOut) {
        *validOut = valid;
    }
}

} // namespace

ResolvedPortalAppContext PortalAppContextResolver::resolve(const QDBusMessage &message,
                                                           const QString &requestedAppId,
                                                           const QString &parentWindow)
{
    const Slm::Permissions::CallerIdentity caller =
        Slm::Permissions::resolveCallerIdentityFromDbus(message);
    return resolve(caller, requestedAppId, parentWindow);
}

ResolvedPortalAppContext PortalAppContextResolver::resolve(
    const Slm::Permissions::CallerIdentity &caller,
    const QString &requestedAppId,
    const QString &parentWindow)
{
    ResolvedPortalAppContext out;
    out.parentWindowRaw = parentWindow.trimmed();
    out.busName = caller.busName;
    out.desktopFileId = caller.desktopFileId.trimmed();
    out.executablePath = caller.executablePath.trimmed();

    QString appId = normalizeDesktopAppId(requestedAppId);
    if (appId.isEmpty()) {
        appId = normalizeDesktopAppId(caller.desktopFileId);
        if (appId.isEmpty()) {
            appId = normalizeDesktopAppId(caller.appId);
        }
        out.appIdFromCaller = !appId.isEmpty();
    }
    if (appId.isEmpty()) {
        appId = QStringLiteral("unknown.desktop");
    }
    out.appId = appId;

    parseParentWindow(out.parentWindowRaw,
                      &out.parentWindowType,
                      &out.parentWindowId,
                      &out.parentWindowValid);
    return out;
}

} // namespace Slm::PortalAdapter

