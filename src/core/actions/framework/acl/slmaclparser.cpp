#include "slmaclparser.h"

#include <QRegularExpression>

namespace Slm::Actions::Framework::Acl {
namespace {

class RDParser
{
public:
    explicit RDParser(const QVector<Token> &tokens)
        : m_tokens(tokens) {}

    ParseResult run()
    {
        ParseResult out;
        m_root = parseOr();
        if (!m_root) {
            out.error = m_error.isEmpty() ? QStringLiteral("Parse error") : m_error;
            return out;
        }
        if (peek().type != TokenType::End) {
            out.error = QStringLiteral("Unexpected token '%1'").arg(peek().lexeme);
            return out;
        }
        out.ok = true;
        out.root = m_root;
        return out;
    }

private:
    AstNodePtr node(AstNode::Value value) { return std::make_shared<AstNode>(AstNode{std::move(value)}); }

    const Token &peek(int offset = 0) const
    {
        const int idx = qBound(0, m_pos + offset, m_tokens.size() - 1);
        return m_tokens.at(idx);
    }

    bool match(TokenType type)
    {
        if (peek().type == type) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool expect(TokenType type, const QString &message)
    {
        if (match(type)) {
            return true;
        }
        m_error = message;
        return false;
    }

    AstNodePtr parseOr()
    {
        AstNodePtr left = parseAnd();
        while (left && match(TokenType::Or)) {
            AstNodePtr right = parseAnd();
            if (!right) return nullptr;
            left = node(AstBinary{TokenType::Or, left, right});
        }
        return left;
    }

    AstNodePtr parseAnd()
    {
        AstNodePtr left = parseUnary();
        while (left && match(TokenType::And)) {
            AstNodePtr right = parseUnary();
            if (!right) return nullptr;
            left = node(AstBinary{TokenType::And, left, right});
        }
        return left;
    }

    AstNodePtr parseUnary()
    {
        if (match(TokenType::Not)) {
            AstNodePtr expr = parseUnary();
            if (!expr) return nullptr;
            return node(AstUnary{TokenType::Not, expr});
        }
        return parseCompare();
    }

    AstNodePtr parseCompare()
    {
        AstNodePtr left = parsePrimary();
        if (!left) return nullptr;

        const TokenType op = peek().type;
        switch (op) {
        case TokenType::Eq:
        case TokenType::Ne:
        case TokenType::Gt:
        case TokenType::Lt:
        case TokenType::Ge:
        case TokenType::Le:
        case TokenType::Like:
        case TokenType::Match:
            ++m_pos;
            break;
        case TokenType::In:
            ++m_pos;
            if (!expect(TokenType::LParen, QStringLiteral("Expected '(' after IN"))) return nullptr;
            {
                QVector<AstNodePtr> values;
                if (peek().type != TokenType::RParen) {
                    while (true) {
                        AstNodePtr value = parsePrimary();
                        if (!value) return nullptr;
                        values.push_back(value);
                        if (!match(TokenType::Comma)) break;
                    }
                }
                if (!expect(TokenType::RParen, QStringLiteral("Expected ')' after IN list"))) return nullptr;
                return node(AstInList{left, values});
            }
        default:
            return left;
        }

        AstNodePtr right = parsePrimary();
        if (!right) return nullptr;
        return node(AstBinary{op, left, right});
    }

    AstNodePtr parsePrimary()
    {
        if (match(TokenType::LParen)) {
            AstNodePtr expr = parseOr();
            if (!expr) return nullptr;
            if (!expect(TokenType::RParen, QStringLiteral("Expected ')'"))) return nullptr;
            return expr;
        }
        if (match(TokenType::True)) {
            return node(AstLiteral{true});
        }
        if (match(TokenType::False)) {
            return node(AstLiteral{false});
        }
        if (peek().type == TokenType::Number) {
            const QString raw = peek().lexeme;
            ++m_pos;
            return node(AstLiteral{raw.toDouble()});
        }
        if (peek().type == TokenType::String) {
            const QString raw = peek().lexeme;
            ++m_pos;
            return node(AstLiteral{raw});
        }
        if (peek().type == TokenType::Any || peek().type == TokenType::All) {
            const TokenType fn = peek().type;
            ++m_pos;
            if (!expect(TokenType::LParen, QStringLiteral("Expected '(' after function"))) return nullptr;
            AstNodePtr expr = parseOr();
            if (!expr) return nullptr;
            if (!expect(TokenType::RParen, QStringLiteral("Expected ')' after function argument"))) return nullptr;
            return node(AstFunction{fn, expr});
        }
        if (peek().type == TokenType::Identifier) {
            const QString name = peek().lexeme;
            ++m_pos;
            return node(AstIdentifier{name});
        }
        m_error = QStringLiteral("Unexpected token '%1'").arg(peek().lexeme);
        return nullptr;
    }

    QVector<Token> m_tokens;
    int m_pos = 0;
    QString m_error;
    AstNodePtr m_root;
};

bool asBool(const std::variant<bool, double, QString> &v)
{
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
    if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0.0;
    return !std::get<QString>(v).trimmed().isEmpty();
}

double asNumber(const std::variant<bool, double, QString> &v)
{
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0 : 0.0;
    bool ok = false;
    const double d = std::get<QString>(v).toDouble(&ok);
    return ok ? d : 0.0;
}

QString asString(const std::variant<bool, double, QString> &v)
{
    if (std::holds_alternative<QString>(v)) return std::get<QString>(v);
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? QStringLiteral("true") : QStringLiteral("false");
    return QString::number(std::get<double>(v));
}

std::variant<bool, double, QString> resolveIdentifier(const QString &id, const ActionContext &ctx)
{
    const QString key = id.trimmed().toLower();
    if (key == QStringLiteral("count")) return double(ctx.selectionCount);
    if (key == QStringLiteral("target")) return ctx.target;
    if (key == QStringLiteral("writable")) return ctx.writable;
    if (key == QStringLiteral("executable")) return ctx.executable;
    if (key == QStringLiteral("mime")) {
        return ctx.mimeTypes.isEmpty() ? QString() : ctx.mimeTypes.first();
    }
    if (key == QStringLiteral("scheme")) {
        if (ctx.uris.isEmpty()) return QString();
        const QString uri = ctx.uris.first();
        const int idx = uri.indexOf(QLatin1Char(':'));
        return idx > 0 ? uri.left(idx).toLower() : QStringLiteral("file");
    }
    if (key == QStringLiteral("ext")) {
        if (ctx.uris.isEmpty()) return QString();
        const QString uri = ctx.uris.first();
        const int slash = uri.lastIndexOf(QLatin1Char('/'));
        const QString base = slash >= 0 ? uri.mid(slash + 1) : uri;
        const int dot = base.lastIndexOf(QLatin1Char('.'));
        return (dot > 0) ? base.mid(dot + 1).toLower() : QString();
    }
    if (key == QStringLiteral("size")) return double(0.0);
    return QString();
}

std::variant<bool, double, QString> evalNode(const AstNodePtr &node, const ActionContext &ctx);

bool evalPredicatePerItem(const AstNodePtr &expr, const ActionContext &ctx, bool requireAll)
{
    if (ctx.uris.isEmpty()) {
        return false;
    }
    bool any = false;
    for (const QString &uri : ctx.uris) {
        ActionContext single = ctx;
        single.uris = {uri};
        single.selectionCount = 1;
        const bool result = asBool(evalNode(expr, single));
        if (requireAll && !result) return false;
        if (!requireAll && result) return true;
        any = true;
    }
    return requireAll ? any : false;
}

std::variant<bool, double, QString> evalNode(const AstNodePtr &node, const ActionContext &ctx)
{
    if (!node) return false;
    return std::visit([&](const auto &value) -> std::variant<bool, double, QString> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AstLiteral>) {
            return value.value;
        } else if constexpr (std::is_same_v<T, AstIdentifier>) {
            return resolveIdentifier(value.name, ctx);
        } else if constexpr (std::is_same_v<T, AstUnary>) {
            if (value.op == TokenType::Not) {
                return !asBool(evalNode(value.expr, ctx));
            }
            return false;
        } else if constexpr (std::is_same_v<T, AstBinary>) {
            const auto l = evalNode(value.left, ctx);
            const auto r = evalNode(value.right, ctx);
            switch (value.op) {
            case TokenType::And: return asBool(l) && asBool(r);
            case TokenType::Or: return asBool(l) || asBool(r);
            case TokenType::Eq: return asString(l) == asString(r);
            case TokenType::Ne: return asString(l) != asString(r);
            case TokenType::Gt: return asNumber(l) > asNumber(r);
            case TokenType::Lt: return asNumber(l) < asNumber(r);
            case TokenType::Ge: return asNumber(l) >= asNumber(r);
            case TokenType::Le: return asNumber(l) <= asNumber(r);
            case TokenType::Like: {
                QString pattern = QRegularExpression::escape(asString(r));
                pattern.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
                const QRegularExpression re(QStringLiteral("^%1$").arg(pattern), QRegularExpression::CaseInsensitiveOption);
                return re.match(asString(l)).hasMatch();
            }
            case TokenType::Match: {
                const QRegularExpression re(asString(r), QRegularExpression::CaseInsensitiveOption);
                return re.match(asString(l)).hasMatch();
            }
            default: return false;
            }
        } else if constexpr (std::is_same_v<T, AstInList>) {
            const QString left = asString(evalNode(value.left, ctx));
            for (const AstNodePtr &entry : value.values) {
                if (left == asString(evalNode(entry, ctx))) {
                    return true;
                }
            }
            return false;
        } else if constexpr (std::is_same_v<T, AstFunction>) {
            if (value.fn == TokenType::Any) {
                return evalPredicatePerItem(value.expr, ctx, false);
            }
            if (value.fn == TokenType::All) {
                return evalPredicatePerItem(value.expr, ctx, true);
            }
            return false;
        } else {
            return false;
        }
    }, node->value);
}

} // namespace

ParseResult Parser::parse(const QVector<Token> &tokens) const
{
    RDParser parser(tokens);
    return parser.run();
}

ParseResult Parser::parse(const QString &expression) const
{
    Lexer lexer;
    QString error;
    const QVector<Token> tokens = lexer.lex(expression, &error);
    if (tokens.isEmpty()) {
        return {false, error.isEmpty() ? QStringLiteral("Failed to tokenize expression") : error, nullptr};
    }
    return parse(tokens);
}

bool Evaluator::evaluate(const AstNodePtr &root, const ActionContext &context) const
{
    return asBool(evalNode(root, context));
}

} // namespace Slm::Actions::Framework::Acl

