#include "contentclassifier.h"

#include "clipboardtypes.h"

#include <QRegularExpression>
#include <QUrl>

namespace Slm::Clipboard {
namespace {

bool looksLikeCode(const QString &text)
{
    if (text.count('\n') < 1) {
        return false;
    }
    const bool hasIndent = text.contains(QStringLiteral("\n    ")) || text.contains(QStringLiteral("\n\t"));
    const bool hasCodeToken = text.contains(QLatin1Char('{'))
            || text.contains(QLatin1String("=>"))
            || text.contains(QLatin1String("::"))
            || text.contains(QLatin1String("function "))
            || text.contains(QLatin1String("class "))
            || text.contains(QLatin1String("#include"));
    return hasIndent || hasCodeToken;
}

bool looksLikeHexColor(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?$"));
    return re.match(text.trimmed()).hasMatch();
}

} // namespace

QString ContentClassifier::classify(const QString &mimeType,
                                    const QString &text,
                                    const QStringList &mimeTypes)
{
    const QString mime = mimeType.toLower().trimmed();
    if (mime.startsWith(QStringLiteral("image/"))
        || mimeTypes.contains(QStringLiteral("image/png"), Qt::CaseInsensitive)
        || mimeTypes.contains(QStringLiteral("image/jpeg"), Qt::CaseInsensitive)) {
        return QString::fromLatin1(kTypeImage);
    }

    if (mime == QStringLiteral("text/uri-list")) {
        const QString t = text.trimmed();
        if (t.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
            || t.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            return QString::fromLatin1(kTypeUrl);
        }
        return QString::fromLatin1(kTypeFile);
    }

    const QString t = text.trimmed();
    if (t.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || t.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return QString::fromLatin1(kTypeUrl);
    }
    if (looksLikeHexColor(t)) {
        return QString::fromLatin1(kTypeColor);
    }
    if (looksLikeCode(text)) {
        return QString::fromLatin1(kTypeCode);
    }
    return QString::fromLatin1(kTypeText);
}

QString ContentClassifier::generatePreview(const QString &type,
                                           const QString &text,
                                           const QString &mimeType,
                                           const QString &contentPathOrUri)
{
    Q_UNUSED(mimeType)
    if (type == QString::fromLatin1(kTypeUrl)) {
        const QUrl url(text.trimmed());
        if (url.isValid() && !url.host().isEmpty()) {
            return url.host();
        }
        return text.trimmed().left(120);
    }

    if (type == QString::fromLatin1(kTypeFile)) {
        const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                             Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            const QUrl u(lines.constFirst());
            const QString p = u.isLocalFile() ? u.toLocalFile() : lines.constFirst();
            const int slash = p.lastIndexOf(QLatin1Char('/'));
            return slash >= 0 ? p.mid(slash + 1) : p;
        }
        return QStringLiteral("file");
    }

    if (type == QString::fromLatin1(kTypeImage)) {
        if (!contentPathOrUri.isEmpty()) {
            const int slash = contentPathOrUri.lastIndexOf(QLatin1Char('/'));
            return slash >= 0 ? contentPathOrUri.mid(slash + 1) : contentPathOrUri;
        }
        return QStringLiteral("image");
    }

    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")));
    while (!lines.isEmpty() && lines.constLast().trimmed().isEmpty()) {
        lines.removeLast();
    }
    if (lines.size() > 2) {
        lines = lines.mid(0, 2);
    }
    QString preview = lines.join(QStringLiteral(" ")).trimmed();
    if (preview.length() > 160) {
        preview = preview.left(157) + QStringLiteral("...");
    }
    return preview;
}

bool ContentClassifier::isSensitiveText(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return false;
    }
    const QString t = text.toLower();
    return t.contains(QStringLiteral("password"))
            || t.contains(QStringLiteral("token"))
            || t.contains(QStringLiteral("otp"))
            || t.contains(QStringLiteral("secret"));
}

} // namespace Slm::Clipboard
