#include "PrinterSettingsStore.h"

#include <QSettings>

namespace Slm::Print {

static const char *kSettingsKeys[] = {
    "copies", "pageRange", "paperSize", "orientation",
    "duplex", "colorMode", "quality", "scale",
    "collate", "staple", "punch", "pluginFeatures",
    nullptr
};

static constexpr char kLastPrinterKey[] = "print/lastPrinterId";

PrinterSettingsStore::PrinterSettingsStore(QObject *parent)
    : QObject(parent)
{
}

QVariantMap PrinterSettingsStore::load(const QString &printerId) const
{
    if (printerId.trimmed().isEmpty()) {
        return {};
    }
    QSettings s;
    s.beginGroup(groupKey(printerId));

    QVariantMap out;
    for (int i = 0; kSettingsKeys[i]; ++i) {
        const QString key = QString::fromLatin1(kSettingsKeys[i]);
        if (s.contains(key)) {
            out.insert(key, s.value(key));
        }
    }
    s.endGroup();
    return out;
}

void PrinterSettingsStore::save(const QString &printerId, const QVariantMap &settings)
{
    if (printerId.trimmed().isEmpty()) {
        return;
    }
    QSettings s;
    s.beginGroup(groupKey(printerId));

    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        // Skip printerId itself — it's the key, not a setting value.
        if (it.key() == QLatin1String("printerId")) {
            continue;
        }
        s.setValue(it.key(), it.value());
    }
    s.endGroup();
    s.sync();
}

void PrinterSettingsStore::clear(const QString &printerId)
{
    if (printerId.trimmed().isEmpty()) {
        return;
    }
    QSettings s;
    s.remove(groupKey(printerId));
    s.sync();
}

bool PrinterSettingsStore::has(const QString &printerId) const
{
    if (printerId.trimmed().isEmpty()) {
        return false;
    }
    QSettings s;
    s.beginGroup(groupKey(printerId));
    const bool result = !s.childKeys().isEmpty();
    s.endGroup();
    return result;
}

QString PrinterSettingsStore::lastPrinterId() const
{
    QSettings s;
    return s.value(QLatin1String(kLastPrinterKey)).toString();
}

void PrinterSettingsStore::saveLastPrinterId(const QString &printerId)
{
    if (printerId.trimmed().isEmpty())
        return;
    QSettings s;
    s.setValue(QLatin1String(kLastPrinterKey), printerId.trimmed());
    s.sync();
}

QString PrinterSettingsStore::groupKey(const QString &printerId)
{
    // QSettings group keys cannot contain '/' — replace with '_'.
    QString sanitized = printerId.trimmed();
    sanitized.replace(QLatin1Char('/'), QLatin1Char('_'));
    return QStringLiteral("print/") + sanitized;
}

} // namespace Slm::Print
