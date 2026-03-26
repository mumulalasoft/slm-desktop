#pragma once

#include <QString>

// AuditLog — append-only JSON-lines log for privileged environment operations.
//
// Written to /var/log/slm/envd-audit.log.
// Each record is one JSON object per line:
//   {"ts":"2026-03-26T10:00:00Z","op":"write","key":"FOO","uid":1000,"result":"ok"}
//
// Failures to write the audit log are silent — they must not block the operation.
//
class AuditLog
{
public:
    static void record(const QString &operation,
                       const QString &key,
                       quint32        uid,
                       bool           success,
                       const QString &detail = {});

private:
    static QString logPath();
};
