#include "ruleengine.h"

namespace Slm::Context {

QVariantMap RuleEngine::evaluate(const QVariantMap &context, const QVariantMap &settingsSnapshot) const
{
    QVariantMap out;
    QVariantMap actions;
    QVariantMap guard;

    const QVariantMap appearance = settingsSnapshot.value(QStringLiteral("globalAppearance")).toMap();
    const QVariantMap contextAutomation = settingsSnapshot.value(QStringLiteral("contextAutomation")).toMap();
    const bool autoReduceAnimation = contextAutomation.value(QStringLiteral("autoReduceAnimation"), true).toBool();
    const bool autoDisableBlur = contextAutomation.value(QStringLiteral("autoDisableBlur"), true).toBool();
    const bool autoDisableHeavyEffects = contextAutomation.value(QStringLiteral("autoDisableHeavyEffects"), true).toBool();
    const QString userMode = appearance.value(QStringLiteral("colorMode")).toString().trimmed().toLower();
    const bool autoThemeEnabled = (userMode == QLatin1String("auto"));

    guard.insert(QStringLiteral("userMode"), userMode);
    guard.insert(QStringLiteral("autoThemeEnabled"), autoThemeEnabled);
    guard.insert(QStringLiteral("context.autoReduceAnimation"), autoReduceAnimation);
    guard.insert(QStringLiteral("context.autoDisableBlur"), autoDisableBlur);
    guard.insert(QStringLiteral("context.autoDisableHeavyEffects"), autoDisableHeavyEffects);

    const QVariantMap time = context.value(QStringLiteral("time")).toMap();
    const QString period = time.value(QStringLiteral("period")).toString().trimmed().toLower();
    if (autoThemeEnabled) {
        const QString suggestedMode = (period == QLatin1String("night"))
            ? QStringLiteral("dark")
            : QStringLiteral("light");
        actions.insert(QStringLiteral("appearance.modeSuggestion"), suggestedMode);
        actions.insert(QStringLiteral("appearance.modeReason"), QStringLiteral("time-period-auto"));
    } else {
        actions.insert(QStringLiteral("appearance.modeSuggestion"), userMode);
        actions.insert(QStringLiteral("appearance.modeReason"), QStringLiteral("manual-user-setting"));
    }

    const QVariantMap power = context.value(QStringLiteral("power")).toMap();
    const bool lowPower = power.value(QStringLiteral("lowPower")).toBool();
    if (lowPower) {
        actions.insert(QStringLiteral("ui.disableBlur"), autoDisableBlur);
        actions.insert(QStringLiteral("ui.reduceAnimation"), autoReduceAnimation);
        actions.insert(QStringLiteral("background.limitRefresh"), true);
    } else {
        actions.insert(QStringLiteral("ui.disableBlur"), false);
        actions.insert(QStringLiteral("ui.reduceAnimation"), false);
        actions.insert(QStringLiteral("background.limitRefresh"), false);
    }

    const QVariantMap display = context.value(QStringLiteral("display")).toMap();
    const bool fullscreen = display.value(QStringLiteral("fullscreen")).toBool();
    actions.insert(QStringLiteral("notifications.suppress"), fullscreen);
    actions.insert(QStringLiteral("notifications.reason"),
                   fullscreen ? QStringLiteral("fullscreen") : QStringLiteral("normal"));

    const QVariantMap system = context.value(QStringLiteral("system")).toMap();
    const QString perf = system.value(QStringLiteral("performance")).toString().trimmed().toLower();
    const bool lowPerformance = (perf == QLatin1String("low"));
    actions.insert(QStringLiteral("ui.disableHeavyEffects"),
                   autoDisableHeavyEffects && (lowPower || lowPerformance));

    out.insert(QStringLiteral("guard"), guard);
    out.insert(QStringLiteral("actions"), actions);
    return out;
}

} // namespace Slm::Context
