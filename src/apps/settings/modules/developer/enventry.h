#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

// EnvEntry — a single environment variable record.
// Scope is stored as a string tag; the store that owns an entry is authoritative
// for which scope it belongs to (user, session, system, per-app).
struct EnvEntry {
    QString   key;
    QString   value;
    bool      enabled   = true;
    QString   scope     = QStringLiteral("user");
    QString   comment;
    // merge_mode controls how this entry combines with lower-priority layers:
    //   "replace"  — this value replaces whatever lower layers produced
    //   "prepend"  — prepended to lower-layer value with ':' separator
    //   "append"   — appended to lower-layer value with ':' separator
    QString   mergeMode = QStringLiteral("replace");
    QDateTime createdAt;
    QDateTime modifiedAt;

    bool isValid() const { return !key.trimmed().isEmpty(); }

    QJsonObject toJson() const;
    static EnvEntry fromJson(const QJsonObject &obj);
};
