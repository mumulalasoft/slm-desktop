#pragma once

#include <QString>
#include <QVector>

namespace Slm::Actions::Framework::Acl {

enum class TokenType {
    Invalid = 0,
    End,
    Identifier,
    Number,
    String,
    True,
    False,
    And,
    Or,
    Not,
    Like,
    Match,
    In,
    Any,
    All,
    Eq,
    Ne,
    Gt,
    Lt,
    Ge,
    Le,
    LParen,
    RParen,
    Comma,
};

struct Token {
    TokenType type = TokenType::Invalid;
    QString lexeme;
    int position = -1;
};

class Lexer
{
public:
    QVector<Token> lex(const QString &input, QString *error = nullptr) const;
};

} // namespace Slm::Actions::Framework::Acl

