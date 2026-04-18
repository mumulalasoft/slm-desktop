#pragma once

#include "cleaneranalyzer.h"
#include "cleanerengine.h"
#include "cleanerpolicy.h"
#include "cleanerscanner.h"

#include <QObject>
#include <QVariantMap>

namespace Slm::Cleaner {

class CleanerService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Cleaner")
public:
    explicit CleanerService(QObject *parent = nullptr);

public slots:
    QVariantMap Ping() const;
    QVariantMap Scan(const QVariantMap &options = QVariantMap{});
    QVariantMap PreviewClean(const QVariantMap &options);
    QVariantMap Clean(const QVariantMap &options);
    QVariantMap GetPolicy() const;
    QVariantMap SetPolicy(const QVariantMap &policyPatch);

signals:
    void ProgressChanged(int percent, const QString &message);

private:
    static CleanRequest requestFromOptions(const QVariantMap &options,
                                           const CleanerPolicy &policyDefaults);
    static QVariantMap errorMap(const QString &code, const QString &message);
    void appendLog(const QString &line) const;

    CleanerScanner m_scanner;
    CleanerAnalyzer m_analyzer;
    CleanerEngine m_engine;
    CleanerPolicyStore m_policyStore;
    CleanerPolicy m_policy;
};

} // namespace Slm::Cleaner
