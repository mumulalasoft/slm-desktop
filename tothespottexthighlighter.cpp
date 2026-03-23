#include "tothespottexthighlighter.h"

#include <QRegularExpression>

TothespotTextHighlighter::TothespotTextHighlighter(QObject *parent)
    : QObject(parent)
{
}

QString TothespotTextHighlighter::highlightStyledText(const QString &title,
                                                      const QString &query,
                                                      const QString &accentColor) const
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return escapeHtml(title);
    }

    const QRegularExpression re(QRegularExpression::escape(trimmed),
                                QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(title);
    if (!it.hasNext()) {
        return escapeHtml(title);
    }

    const QString color = accentColor.trimmed().isEmpty()
                              ? QStringLiteral("#1d6fd8")
                              : accentColor.trimmed();
    QString out;
    int cursor = 0;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const int start = m.capturedStart();
        const int end = m.capturedEnd();
        if (start < cursor || start < 0 || end < start) {
            continue;
        }
        out += escapeHtml(title.mid(cursor, start - cursor));
        out += QStringLiteral("<span style=\"font-weight:700;color:%1;\">%2</span>")
                   .arg(escapeHtml(color),
                        escapeHtml(title.mid(start, end - start)));
        cursor = end;
    }
    out += escapeHtml(title.mid(cursor));
    return out;
}

QString TothespotTextHighlighter::escapeHtml(const QString &value)
{
    QString out = value;
    out.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    out.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    out.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    out.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
    out.replace(QStringLiteral("'"), QStringLiteral("&#39;"));
    return out;
}

