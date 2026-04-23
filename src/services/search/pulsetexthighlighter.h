#pragma once

#include <QObject>
#include <QString>

class PulseTextHighlighter : public QObject
{
    Q_OBJECT

public:
    explicit PulseTextHighlighter(QObject *parent = nullptr);

    Q_INVOKABLE QString highlightStyledText(const QString &title,
                                            const QString &query,
                                            const QString &accentColor) const;

    static QString escapeHtml(const QString &value);
};

