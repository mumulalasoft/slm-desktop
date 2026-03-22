#pragma once

#include "slmactiontypes.h"

#include <QSharedPointer>
#include <QString>
#include <QVariant>
#include <QVector>

namespace Slm::Actions {

class ConditionAstNode
{
public:
    enum class Type {
        Literal,
        Identifier,
        UnaryNot,
        BinaryAnd,
        BinaryOr,
        CompareEq,
        CompareNe,
        CompareGt,
        CompareLt,
        CompareGe,
        CompareLe,
        CompareLike,
        CompareMatch,
        CompareIn,
        List,
        FuncAny,
        FuncAll,
    };

    Type type = Type::Literal;
    QVariant value;
    QString identifier;
    QSharedPointer<ConditionAstNode> left;
    QSharedPointer<ConditionAstNode> right;
    QVector<QSharedPointer<ConditionAstNode>> listValues;
};

struct ConditionParseResult {
    bool ok = false;
    QString error;
    int errorPos = -1;
    QSharedPointer<ConditionAstNode> root;
};

class ActionConditionParser
{
public:
    ConditionParseResult parse(const QString &expression) const;
};

class ActionConditionEvaluator
{
public:
    bool evaluate(const QSharedPointer<ConditionAstNode> &root,
                  const ActionEvalContext &ctx) const;
};

} // namespace Slm::Actions

