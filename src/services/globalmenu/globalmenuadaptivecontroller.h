#pragma once

#include <QObject>
#include <QString>

// GlobalMenuAdaptiveController — decides which display mode the Global Menu bar
// should use based on available width, window state, and user preference.
//
// Three modes:
//   Full    — all menu categories visible
//   Compact — only key menus (App | File | Edit | View | More)
//   Focus   — menu hidden; revealed on mouse-hover at top edge or shortcut
//
// Mode switching is automatic unless the user explicitly overrides it.

class GlobalMenuAdaptiveController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool isFullMode    READ isFullMode    NOTIFY modeChanged)
    Q_PROPERTY(bool isCompactMode READ isCompactMode NOTIFY modeChanged)
    Q_PROPERTY(bool isFocusMode   READ isFocusMode   NOTIFY modeChanged)
    Q_PROPERTY(QString userOverride READ userOverride WRITE setUserOverride NOTIFY modeChanged)

public:
    explicit GlobalMenuAdaptiveController(QObject *parent = nullptr);

    QString mode() const;
    bool isFullMode() const    { return mode() == QLatin1String("full"); }
    bool isCompactMode() const { return mode() == QLatin1String("compact"); }
    bool isFocusMode() const   { return mode() == QLatin1String("focus"); }

    QString userOverride() const;
    void setUserOverride(const QString &override);

    // Called from QML when layout width changes.
    Q_INVOKABLE void setAvailableWidth(int width);

    // Called from QML when the active window enters/leaves fullscreen.
    Q_INVOKABLE void setWindowFullscreen(bool fullscreen);

signals:
    void modeChanged();

private:
    void recompute();

    int m_availableWidth = 1920;
    bool m_windowFullscreen = false;
    QString m_userOverride;  // "" = auto, "full" | "compact" | "focus"
    QString m_computedMode = QStringLiteral("full");

    // Width thresholds for auto switching.
    static constexpr int kCompactThreshold = 900;
};
