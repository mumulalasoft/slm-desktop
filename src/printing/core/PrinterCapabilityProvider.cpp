#include "PrinterCapabilityProvider.h"

#include <QProcess>
#include <QRegularExpression>

namespace Slm::Print {

PrinterCapabilityProvider::PrinterCapabilityProvider(QObject *parent)
    : QObject(parent)
{
}

QVariantMap PrinterCapabilityProvider::capabilityMapForPrinter(const QString &printerId) const
{
    return capabilityToVariantMap(capabilityForPrinter(printerId));
}

PrinterCapability PrinterCapabilityProvider::capabilityForPrinter(const QString &printerId) const
{
    const QString output = runCommand(QStringLiteral("lpoptions"),
                                      { QStringLiteral("-p"), printerId, QStringLiteral("-l") });
    return parseLpoptions(printerId, output);
}

PrinterCapability PrinterCapabilityProvider::parseLpoptions(const QString &printerId, const QString &lpoptionsOutput)
{
    PrinterCapability capability;
    capability.printerId = printerId;
    capability.supportsPdfDirect = true; // optimistic default for modern IPP/CUPS stacks
    capability.supportsColor = true;
    capability.supportsDuplex = false;

    const QStringList lines = lpoptionsOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &lineRaw : lines) {
        const QString line = lineRaw.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }

        const QString left = line.left(colon);
        const QString valuesPart = line.mid(colon + 1).trimmed();
        const QString optionName = left.section(QLatin1Char('/'), 0, 0).trimmed();

        if (optionName.compare(QStringLiteral("PageSize"), Qt::CaseInsensitive) == 0
            || optionName.compare(QStringLiteral("media"), Qt::CaseInsensitive) == 0) {
            QStringList values;
            const QStringList tokens = valuesPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &token : tokens) {
                values.append(token.startsWith(QLatin1Char('*')) ? token.mid(1) : token);
            }
            values.removeDuplicates();
            capability.paperSizes = values;
            continue;
        }

        if (optionName.compare(QStringLiteral("ColorModel"), Qt::CaseInsensitive) == 0
            || optionName.compare(QStringLiteral("print-color-mode"), Qt::CaseInsensitive) == 0) {
            bool hasColorChoice = false;
            const QStringList tokens = valuesPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (QString token : tokens) {
                token = token.startsWith(QLatin1Char('*')) ? token.mid(1) : token;
                const QString lower = token.toLower();
                const bool monochromeOnly = lower == QStringLiteral("gray") || lower == QStringLiteral("grey")
                                            || lower == QStringLiteral("grayscale") || lower == QStringLiteral("mono")
                                            || lower == QStringLiteral("monochrome") || lower == QStringLiteral("black")
                                            || lower == QStringLiteral("none");
                if (!monochromeOnly) {
                    hasColorChoice = true;
                    break;
                }
            }
            capability.supportsColor = hasColorChoice;
            continue;
        }

        if (optionName.compare(QStringLiteral("Duplex"), Qt::CaseInsensitive) == 0
            || optionName.compare(QStringLiteral("sides"), Qt::CaseInsensitive) == 0) {
            const QString lowered = valuesPart.toLower();
            capability.supportsDuplex = lowered.contains(QStringLiteral("two-sided"))
                                        || lowered.contains(QStringLiteral("duplex"))
                                        || lowered.contains(QStringLiteral("tumble"));
            continue;
        }

        if (optionName.compare(QStringLiteral("print-quality"), Qt::CaseInsensitive) == 0
            || optionName.compare(QStringLiteral("Quality"), Qt::CaseInsensitive) == 0) {
            QStringList values;
            const QStringList tokens = valuesPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &token : tokens) {
                values.append(token.startsWith(QLatin1Char('*')) ? token.mid(1) : token);
            }
            values.removeDuplicates();
            capability.qualityModes = values;
            continue;
        }

        if (optionName.compare(QStringLiteral("Resolution"), Qt::CaseInsensitive) == 0) {
            static const QRegularExpression dpiRe(QStringLiteral("(\\d+)dpi"), QRegularExpression::CaseInsensitiveOption);
            auto matches = dpiRe.globalMatch(valuesPart);
            while (matches.hasNext()) {
                const auto m = matches.next();
                const int dpi = m.captured(1).toInt();
                if (!capability.resolutionsDpi.contains(dpi)) {
                    capability.resolutionsDpi.append(dpi);
                }
            }
            continue;
        }

        if (optionName.compare(QStringLiteral("InputSlot"), Qt::CaseInsensitive) == 0
            || optionName.compare(QStringLiteral("media-source"), Qt::CaseInsensitive) == 0) {
            const QStringList tokens = valuesPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &rawToken : tokens) {
                const QString token = rawToken.startsWith(QLatin1Char('*'))
                                      ? rawToken.mid(1) : rawToken;
                if (token.isEmpty()) continue;
                MediaSource src;
                src.id = token.toLower();
                // Build a human-readable label from the raw token.
                QString label = token;
                label.replace(QLatin1Char('-'), QLatin1Char(' '));
                // Capitalize first letter of each word.
                for (int ci = 0; ci < label.size(); ++ci) {
                    if (ci == 0 || label[ci - 1] == QLatin1Char(' ')) {
                        label[ci] = label[ci].toUpper();
                    }
                }
                src.label = label;
                capability.mediaSources.append(src);
            }
            continue;
        }

        capability.vendorExtensions.insert(optionName, valuesPart);
    }

    if (capability.paperSizes.isEmpty()) {
        capability.paperSizes = { QStringLiteral("A4"), QStringLiteral("Letter") };
    }
    if (capability.qualityModes.isEmpty()) {
        capability.qualityModes = { QStringLiteral("draft"), QStringLiteral("normal"), QStringLiteral("high") };
    }
    if (capability.resolutionsDpi.isEmpty()) {
        capability.resolutionsDpi = { 300, 600 };
    }

    return capability;
}

QVariantMap PrinterCapabilityProvider::capabilityToVariantMap(const PrinterCapability &capability)
{
    QVariantMap map;
    map.insert(QStringLiteral("printerId"), capability.printerId);
    map.insert(QStringLiteral("supportsPdfDirect"), capability.supportsPdfDirect);
    map.insert(QStringLiteral("supportsColor"), capability.supportsColor);
    map.insert(QStringLiteral("supportsDuplex"), capability.supportsDuplex);
    map.insert(QStringLiteral("paperSizes"), capability.paperSizes);
    map.insert(QStringLiteral("qualityModes"), capability.qualityModes);

    QVariantList resolutions;
    for (const int dpi : capability.resolutionsDpi) {
        resolutions.append(dpi);
    }
    map.insert(QStringLiteral("resolutionsDpi"), resolutions);

    QVariantList mediaSources;
    for (const MediaSource &src : capability.mediaSources) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"),    src.id);
        entry.insert(QStringLiteral("label"), src.label);
        mediaSources.append(entry);
    }
    map.insert(QStringLiteral("mediaSources"), mediaSources);
    map.insert(QStringLiteral("vendorExtensions"), capability.vendorExtensions);
    return map;
}

QString PrinterCapabilityProvider::runCommand(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(2000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QString();
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

} // namespace Slm::Print
