#pragma once

#include "slmacllexer.h"
#include "../slmframeworktypes.h"

#include <memory>
#include <variant>

namespace Slm::Actions::Framework::Acl {

struct AstNode;
using AstNodePtr = std::shared_ptr<AstNode>;

struct AstLiteral {
    std::variant<bool, double, QString> value;
};

struct AstIdentifier {
    QString name;
};

struct AstUnary {
    TokenType op = TokenType::Invalid;
    AstNodePtr expr;
};

struct AstBinary {
    TokenType op = TokenType::Invalid;
    AstNodePtr left;
    AstNodePtr right;
};

struct AstInList {
    AstNodePtr left;
    QVector<AstNodePtr> values;
};

struct AstFunction {
    TokenType fn = TokenType::Invalid; // Any | All
    AstNodePtr expr;
};

struct AstNode {
    using Value = std::variant<AstLiteral, AstIdentifier, AstUnary, AstBinary, AstInList, AstFunction>;
    Value value;
};

struct ParseResult {
    bool ok = false;
    QString error;
    AstNodePtr root;
};

class Parser
{
public:
    ParseResult parse(const QVector<Token> &tokens) const;
    ParseResult parse(const QString &expression) const;
};

class Evaluator
{
public:
    bool evaluate(const AstNodePtr &root, const ActionContext &context) const;
};

} // namespace Slm::Actions::Framework::Acl

