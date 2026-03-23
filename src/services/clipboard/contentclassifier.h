#pragma once

#include <QString>
#include <QStringList>

namespace Slm::Clipboard {

class ContentClassifier
{
public:
    static QString classify(const QString &mimeType, const QString &text, const QStringList &mimeTypes);
    static QString generatePreview(const QString &type,
                                   const QString &text,
                                   const QString &mimeType,
                                   const QString &contentPathOrUri);
    static bool isSensitiveText(const QString &text);
};

} // namespace Slm::Clipboard
