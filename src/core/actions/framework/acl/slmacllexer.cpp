#include "slmacllexer.h"

namespace Slm::Actions::Framework::Acl {
namespace {

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_');
}

bool isIdentPart(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-');
}

TokenType keywordType(const QString &word)
{
    const QString w = word.toLower();
    if (w == QStringLiteral("true")) return TokenType::True;
    if (w == QStringLiteral("false")) return TokenType::False;
    if (w == QStringLiteral("and")) return TokenType::And;
    if (w == QStringLiteral("or")) return TokenType::Or;
    if (w == QStringLiteral("not")) return TokenType::Not;
    if (w == QStringLiteral("like")) return TokenType::Like;
    if (w == QStringLiteral("match")) return TokenType::Match;
    if (w == QStringLiteral("in")) return TokenType::In;
    if (w == QStringLiteral("any")) return TokenType::Any;
    if (w == QStringLiteral("all")) return TokenType::All;
    return TokenType::Identifier;
}

} // namespace

QVector<Token> Lexer::lex(const QString &input, QString *error) const
{
    QVector<Token> out;
    const int n = input.size();
    int i = 0;
    auto push = [&out](TokenType type, const QString &lexeme, int pos) {
        out.push_back(Token{type, lexeme, pos});
    };

    while (i < n) {
        const QChar c = input.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }

        if (c == QLatin1Char('(')) {
            push(TokenType::LParen, QStringLiteral("("), i++);
            continue;
        }
        if (c == QLatin1Char(')')) {
            push(TokenType::RParen, QStringLiteral(")"), i++);
            continue;
        }
        if (c == QLatin1Char(',')) {
            push(TokenType::Comma, QStringLiteral(","), i++);
            continue;
        }

        if (c == QLatin1Char('=') && i + 1 < n && input.at(i + 1) == QLatin1Char('=')) {
            push(TokenType::Eq, QStringLiteral("=="), i);
            i += 2;
            continue;
        }
        if (c == QLatin1Char('!') && i + 1 < n && input.at(i + 1) == QLatin1Char('=')) {
            push(TokenType::Ne, QStringLiteral("!="), i);
            i += 2;
            continue;
        }
        if (c == QLatin1Char('>')) {
            if (i + 1 < n && input.at(i + 1) == QLatin1Char('=')) {
                push(TokenType::Ge, QStringLiteral(">="), i);
                i += 2;
            } else {
                push(TokenType::Gt, QStringLiteral(">"), i++);
            }
            continue;
        }
        if (c == QLatin1Char('<')) {
            if (i + 1 < n && input.at(i + 1) == QLatin1Char('=')) {
                push(TokenType::Le, QStringLiteral("<="), i);
                i += 2;
            } else {
                push(TokenType::Lt, QStringLiteral("<"), i++);
            }
            continue;
        }

        if (c == QLatin1Char('"')) {
            const int start = i++;
            QString buffer;
            while (i < n) {
                const QChar ch = input.at(i++);
                if (ch == QLatin1Char('"')) {
                    push(TokenType::String, buffer, start);
                    break;
                }
                if (ch == QLatin1Char('\\') && i < n) {
                    buffer.push_back(input.at(i++));
                } else {
                    buffer.push_back(ch);
                }
            }
            if (out.isEmpty() || out.back().position != start) {
                if (error) {
                    *error = QStringLiteral("Unterminated string at %1").arg(start);
                }
                return {};
            }
            continue;
        }

        if (c.isDigit()) {
            const int start = i;
            while (i < n && (input.at(i).isDigit() || input.at(i) == QLatin1Char('.'))) {
                ++i;
            }
            push(TokenType::Number, input.mid(start, i - start), start);
            continue;
        }

        if (isIdentStart(c)) {
            const int start = i;
            while (i < n && isIdentPart(input.at(i))) {
                ++i;
            }
            const QString word = input.mid(start, i - start);
            push(keywordType(word), word, start);
            continue;
        }

        if (error) {
            *error = QStringLiteral("Unexpected token '%1' at %2").arg(c, QString::number(i));
        }
        return {};
    }

    out.push_back(Token{TokenType::End, QString(), n});
    return out;
}

} // namespace Slm::Actions::Framework::Acl

