import QtQuick
import QtQuick.Controls
import Slm_Desktop

// NotificationContextMenu — right-click menu for notification bubbles/cards.
//
// Embed inside NotificationBubble or NotificationCard and call
// popupForNotification() on right-click.

Item {
    id: root

    signal reply(string notificationId)
    signal muteApp(string appId)
    signal turnOffNotifications(string appId)
    signal openApp(string appId)

    // Call from the notification component's right-click handler.
    function popupForNotification(notificationId, appId, canReply, x, y) {
        notifMenu.surfaceContext = {
            "type": "notification",
            "notificationId": notificationId,
            "appId": appId,
            "canReply": canReply === true
        }
        notifMenu.popupAt(x, y)
    }

    ContextMenu {
        id: notifMenu

        onAction: function(id, ctx) {
            var nid = ctx.notificationId || ""
            var aid = ctx.appId || ""
            switch (id) {
            case "reply":                   root.reply(nid);                   break
            case "mute_app":                root.muteApp(aid);                 break
            case "turn_off_notifications":  root.turnOffNotifications(aid);    break
            case "open_app":                root.openApp(aid);                 break
            default: break
            }
        }
    }
}
