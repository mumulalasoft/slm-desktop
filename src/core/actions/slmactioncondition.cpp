#include "slmactioncondition.h"

#include <QRegularExpression>
#include <QStringList>

namespace Slm::Actions {
namespace {

enum class TokKind {
    End,
    Identifier,
    String,
    Number,
    TrueLit,
    FalseLit,
    LParen,
    RParen,
    Comma,
    And,
    Or,
    Not,
    Eq,
    Ne,
    Gt,
    Lt,
    Ge,
    Le,
    Like,
    Match,
    In,
};

struct Tok {
    TokKind kind = TokKind::End;
    QString text;
    int pos = -1;
};

class Lexer
{
public:
    explicit Lexer(QStringView src)
        : m_src(src)
    {
    }

    Tok next()
    {
        skipWs();
        if (m_i >= m_src.size()) {
            return {TokKind::End, QString(), m_i};
        }
        const QChar c = m_src.at(m_i);
        const int pos = m_i;
        if (c == QLatin1Char('(')) {
            ++m_i;
            return {TokKind::LParen, QStringLiteral("("), pos};
        }
        if (c == QLatin1Char(')')) {
            ++m_i;
            return {TokKind::RParen, QStringLiteral(")"), pos};
        }
        if (c == QLatin1Char(',')) {
            ++m_i;
            return {TokKind::Comma, QStringLiteral(","), pos};
        }
        if (c == QLatin1Char('=') && peek(1) == QLatin1Char('=')) {
            m_i += 2;
            return {TokKind::Eq, QStringLiteral("=="), pos};
        }
        if (c == QLatin1Char('!') && peek(1) == QLatin1Char('=')) {
            m_i += 2;
            return {TokKind::Ne, QStringLiteral("!="), pos};
        }
        if (c == QLatin1Char('>') && peek(1) == QLatin1Char('=')) {
            m_i += 2;
            return {TokKind::Ge, QStringLiteral(">="), pos};
        }
        if (c == QLatin1Char('<') && peek(1) == QLatin1Char('=')) {
            m_i += 2;
            return {TokKind::Le, QStringLiteral("<="), pos};
        }
        if (c == QLatin1Char('>')) {
            ++m_i;
            return {TokKind::Gt, QStringLiteral(">"), pos};
        }
        if (c == QLatin1Char('<')) {
            ++m_i;
            return {TokKind::Lt, QStringLiteral("<"), pos};
        }
        if (c == QLatin1Char('"')) {
            ++m_i;
            QString out;
            while (m_i < m_src.size()) {
                const QChar ch = m_src.at(m_i++);
                if (ch == QLatin1Char('"')) {
                    return {TokKind::String, out, pos};
                }
                if (ch == QLatin1Char('\\') && m_i < m_src.size()) {
                    out.push_back(m_src.at(m_i++));
                } else {
                    out.push_back(ch);
                }
            }
            return {TokKind::String, out, pos};
        }
        if (c.isDigit()) {
            QString num;
            while (m_i < m_src.size() && (m_src.at(m_i).isDigit() || m_src.at(m_i) == QLatin1Char('.'))) {
                num.push_back(m_src.at(m_i++));
            }
            return {TokKind::Number, num, pos};
        }
        if (c.isLetter() || c == QLatin1Char('_')) {
            QString ident;
            while (m_i < m_src.size()) {
                const QChar ch = m_src.at(m_i);
                if (!ch.isLetterOrNumber() && ch != QLatin1Char('_')) {
                    break;
                }
                ident.push_back(ch);
                ++m_i;
            }
            const QString u = ident.toUpper();
            if (u == QStringLiteral("AND")) return {TokKind::And, ident, pos};
            if (u == QStringLiteral("OR")) return {TokKind::Or, ident, pos};
            if (u == QStringLiteral("NOT")) return {TokKind::Not, ident, pos};
            if (u == QStringLiteral("LIKE")) return {TokKind::Like, ident, pos};
            if (u == QStringLiteral("MATCH")) return {TokKind::Match, ident, pos};
            if (u == QStringLiteral("IN")) return {TokKind::In, ident, pos};
            if (u == QStringLiteral("TRUE")) return {TokKind::TrueLit, ident, pos};
            if (u == QStringLiteral("FALSE")) return {TokKind::FalseLit, ident, pos};
            return {TokKind::Identifier, ident, pos};
        }
        ++m_i;
        return {TokKind::End, QString(), pos};
    }

private:
    QChar peek(int offset) const
    {
        const int idx = m_i + offset;
        if (idx < 0 || idx >= m_src.size()) {
            return QChar();
        }
        return m_src.at(idx);
    }

    void skipWs()
    {
        while (m_i < m_src.size() && m_src.at(m_i).isSpace()) {
            ++m_i;
        }
    }

    QStringView m_src;
    int m_i = 0;
};

class Parser
{
public:
    explicit Parser(const QString &expr)
        : m_lexer(QStringView(expr))
    {
        m_current = m_lexer.next();
    }

    ConditionParseResult run()
    {
        ConditionParseResult out;
        out.root = parseExpr();
        if (!out.root) {
            out.error = m_error;
            out.errorPos = m_errorPos;
            return out;
        }
        if (m_current.kind != TokKind::End) {
            out.error = QStringLiteral("Unexpected token near '%1'").arg(m_current.text);
            out.errorPos = m_current.pos;
            return out;
        }
        out.ok = true;
        return out;
    }

private:
    static QSharedPointer<ConditionAstNode> makeNode(ConditionAstNode::Type t)
    {
        auto n = QSharedPointer<ConditionAstNode>::create();
        n->type = t;
        return n;
    }

    void advance()
    {
        m_current = m_lexer.next();
    }

    bool expect(TokKind kind, const QString &message)
    {
        if (m_current.kind == kind) {
            advance();
            return true;
        }
        if (m_error.isEmpty()) {
            m_error = message;
            m_errorPos = m_current.pos;
        }
        return false;
    }

    QSharedPointer<ConditionAstNode> parseExpr()
    {
        return parseOr();
    }

    QSharedPointer<ConditionAstNode> parseOr()
    {
        auto left = parseAnd();
        while (left && m_current.kind == TokKind::Or) {
            advance();
            auto right = parseAnd();
            if (!right) return {};
            auto node = makeNode(ConditionAstNode::Type::BinaryOr);
            node->left = left;
            node->right = right;
            left = node;
        }
        return left;
    }

    QSharedPointer<ConditionAstNode> parseAnd()
    {
        auto left = parseUnary();
        while (left && m_current.kind == TokKind::And) {
            advance();
            auto right = parseUnary();
            if (!right) return {};
            auto node = makeNode(ConditionAstNode::Type::BinaryAnd);
            node->left = left;
            node->right = right;
            left = node;
        }
        return left;
    }

    QSharedPointer<ConditionAstNode> parseUnary()
    {
        if (m_current.kind == TokKind::Not) {
            advance();
            auto node = makeNode(ConditionAstNode::Type::UnaryNot);
            node->left = parseUnary();
            if (!node->left) {
                return {};
            }
            return node;
        }
        return parsePrimary();
    }

    QSharedPointer<ConditionAstNode> parsePrimary()
    {
        if (m_current.kind == TokKind::LParen) {
            advance();
            auto node = parseExpr();
            if (!node || !expect(TokKind::RParen, QStringLiteral("Missing ')'"))) {
                return {};
            }
            return node;
        }
        if (m_current.kind == TokKind::Identifier) {
            const QString ident = m_current.text;
            advance();
            if (m_current.kind == TokKind::LParen
                && (ident.compare(QStringLiteral("any"), Qt::CaseInsensitive) == 0
                    || ident.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0)) {
                advance();
                auto arg = parseExpr();
                if (!arg || !expect(TokKind::RParen, QStringLiteral("Missing ')' after function argument"))) {
                    return {};
                }
                auto fn = makeNode(ident.compare(QStringLiteral("any"), Qt::CaseInsensitive) == 0
                                       ? ConditionAstNode::Type::FuncAny
                                       : ConditionAstNode::Type::FuncAll);
                fn->left = arg;
                return fn;
            }
            auto left = makeNode(ConditionAstNode::Type::Identifier);
            left->identifier = ident;
            return parseComparisonTail(left);
        }
        if (m_current.kind == TokKind::String) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = m_current.text;
            advance();
            return parseComparisonTail(lit);
        }
        if (m_current.kind == TokKind::Number) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = m_current.text.toDouble();
            advance();
            return parseComparisonTail(lit);
        }
        if (m_current.kind == TokKind::TrueLit || m_current.kind == TokKind::FalseLit) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = (m_current.kind == TokKind::TrueLit);
            advance();
            return parseComparisonTail(lit);
        }
        if (m_error.isEmpty()) {
            m_error = QStringLiteral("Unexpected token '%1'").arg(m_current.text);
            m_errorPos = m_current.pos;
        }
        return {};
    }

    QSharedPointer<ConditionAstNode> parseListLiteral()
    {
        if (!expect(TokKind::LParen, QStringLiteral("Missing '(' for IN list"))) {
            return {};
        }
        auto list = makeNode(ConditionAstNode::Type::List);
        while (true) {
            if (m_current.kind == TokKind::String) {
                auto lit = makeNode(ConditionAstNode::Type::Literal);
                lit->value = m_current.text;
                list->listValues.push_back(lit);
                advance();
            } else if (m_current.kind == TokKind::Number) {
                auto lit = makeNode(ConditionAstNode::Type::Literal);
                lit->value = m_current.text.toDouble();
                list->listValues.push_back(lit);
                advance();
            } else if (m_current.kind == TokKind::Identifier) {
                auto id = makeNode(ConditionAstNode::Type::Identifier);
                id->identifier = m_current.text;
                list->listValues.push_back(id);
                advance();
            } else {
                if (m_error.isEmpty()) {
                    m_error = QStringLiteral("Invalid IN list value");
                    m_errorPos = m_current.pos;
                }
                return {};
            }
            if (m_current.kind == TokKind::Comma) {
                advance();
                continue;
            }
            break;
        }
        if (!expect(TokKind::RParen, QStringLiteral("Missing ')' for IN list"))) {
            return {};
        }
        return list;
    }

    QSharedPointer<ConditionAstNode> parseValue()
    {
        if (m_current.kind == TokKind::Identifier) {
            auto id = makeNode(ConditionAstNode::Type::Identifier);
            id->identifier = m_current.text;
            advance();
            return id;
        }
        if (m_current.kind == TokKind::String) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = m_current.text;
            advance();
            return lit;
        }
        if (m_current.kind == TokKind::Number) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = m_current.text.toDouble();
            advance();
            return lit;
        }
        if (m_current.kind == TokKind::TrueLit || m_current.kind == TokKind::FalseLit) {
            auto lit = makeNode(ConditionAstNode::Type::Literal);
            lit->value = (m_current.kind == TokKind::TrueLit);
            advance();
            return lit;
        }
        if (m_current.kind == TokKind::LParen) {
            return parseListLiteral();
        }
        if (m_error.isEmpty()) {
            m_error = QStringLiteral("Expected value");
            m_errorPos = m_current.pos;
        }
        return {};
    }

    QSharedPointer<ConditionAstNode> parseComparisonTail(const QSharedPointer<ConditionAstNode> &left)
    {
        ConditionAstNode::Type opType;
        bool hasOp = true;
        switch (m_current.kind) {
        case TokKind::Eq: opType = ConditionAstNode::Type::CompareEq; break;
        case TokKind::Ne: opType = ConditionAstNode::Type::CompareNe; break;
        case TokKind::Gt: opType = ConditionAstNode::Type::CompareGt; break;
        case TokKind::Lt: opType = ConditionAstNode::Type::CompareLt; break;
        case TokKind::Ge: opType = ConditionAstNode::Type::CompareGe; break;
        case TokKind::Le: opType = ConditionAstNode::Type::CompareLe; break;
        case TokKind::Like: opType = ConditionAstNode::Type::CompareLike; break;
        case TokKind::Match: opType = ConditionAstNode::Type::CompareMatch; break;
        case TokKind::In: opType = ConditionAstNode::Type::CompareIn; break;
        default:
            hasOp = false;
            break;
        }
        if (!hasOp) {
            return left;
        }
        advance();
        auto right = parseValue();
        if (!right) {
            return {};
        }
        auto node = makeNode(opType);
        node->left = left;
        node->right = right;
        return node;
    }

    Lexer m_lexer;
    Tok m_current;
    QString m_error;
    int m_errorPos = -1;
};

bool isTruthy(const QVariant &v)
{
    if (!v.isValid()) return false;
    if (v.metaType().id() == QMetaType::Bool) return v.toBool();
    if (v.canConvert<double>()) return v.toDouble() != 0.0;
    const QString s = v.toString().trimmed().toLower();
    return s == QStringLiteral("true") || s == QStringLiteral("yes") || s == QStringLiteral("1");
}

QVariant resolveIdentifier(const QString &id, const ActionEvalContext &ctx, const ActionItemMeta *item)
{
    const QString key = id.trimmed().toLower();
    if (key == QStringLiteral("count")) return ctx.count;
    if (!item) {
        return {};
    }
    if (key == QStringLiteral("mime")) return item->mime;
    if (key == QStringLiteral("ext")) return item->ext;
    if (key == QStringLiteral("size")) return item->size;
    if (key == QStringLiteral("scheme")) return item->scheme;
    if (key == QStringLiteral("target")) return item->target;
    if (key == QStringLiteral("writable")) return item->writable;
    if (key == QStringLiteral("executable")) return item->executable;
    return {};
}

QVariant evalValueNode(const QSharedPointer<ConditionAstNode> &node,
                       const ActionEvalContext &ctx,
                       const ActionItemMeta *item);

bool evalNode(const QSharedPointer<ConditionAstNode> &node,
              const ActionEvalContext &ctx,
              const ActionItemMeta *item)
{
    if (!node) {
        return true;
    }
    switch (node->type) {
    case ConditionAstNode::Type::Literal:
        return isTruthy(node->value);
    case ConditionAstNode::Type::Identifier:
        return isTruthy(resolveIdentifier(node->identifier, ctx, item));
    case ConditionAstNode::Type::UnaryNot:
        return !evalNode(node->left, ctx, item);
    case ConditionAstNode::Type::BinaryAnd:
        return evalNode(node->left, ctx, item) && evalNode(node->right, ctx, item);
    case ConditionAstNode::Type::BinaryOr:
        return evalNode(node->left, ctx, item) || evalNode(node->right, ctx, item);
    case ConditionAstNode::Type::FuncAny: {
        if (ctx.items.isEmpty()) {
            return evalNode(node->left, ctx, nullptr);
        }
        for (const ActionItemMeta &it : ctx.items) {
            if (evalNode(node->left, ctx, &it)) {
                return true;
            }
        }
        return false;
    }
    case ConditionAstNode::Type::FuncAll: {
        if (ctx.items.isEmpty()) {
            return evalNode(node->left, ctx, nullptr);
        }
        for (const ActionItemMeta &it : ctx.items) {
            if (!evalNode(node->left, ctx, &it)) {
                return false;
            }
        }
        return true;
    }
    case ConditionAstNode::Type::CompareEq:
    case ConditionAstNode::Type::CompareNe:
    case ConditionAstNode::Type::CompareGt:
    case ConditionAstNode::Type::CompareLt:
    case ConditionAstNode::Type::CompareGe:
    case ConditionAstNode::Type::CompareLe:
    case ConditionAstNode::Type::CompareLike:
    case ConditionAstNode::Type::CompareMatch:
    case ConditionAstNode::Type::CompareIn:
        break;
    case ConditionAstNode::Type::List:
        return !node->listValues.isEmpty();
    }

    const QVariant l = evalValueNode(node->left, ctx, item);
    const QVariant r = evalValueNode(node->right, ctx, item);
    const bool lNum = l.canConvert<double>();
    const bool rNum = r.canConvert<double>();
    if (node->type == ConditionAstNode::Type::CompareEq) {
        if (lNum && rNum) return qFuzzyCompare(l.toDouble() + 1.0, r.toDouble() + 1.0);
        return l.toString().compare(r.toString(), Qt::CaseInsensitive) == 0;
    }
    if (node->type == ConditionAstNode::Type::CompareNe) {
        if (lNum && rNum) return !qFuzzyCompare(l.toDouble() + 1.0, r.toDouble() + 1.0);
        return l.toString().compare(r.toString(), Qt::CaseInsensitive) != 0;
    }
    if (node->type == ConditionAstNode::Type::CompareGt) return l.toDouble() > r.toDouble();
    if (node->type == ConditionAstNode::Type::CompareLt) return l.toDouble() < r.toDouble();
    if (node->type == ConditionAstNode::Type::CompareGe) return l.toDouble() >= r.toDouble();
    if (node->type == ConditionAstNode::Type::CompareLe) return l.toDouble() <= r.toDouble();
    if (node->type == ConditionAstNode::Type::CompareLike) {
        const QString wildcard = QRegularExpression::wildcardToRegularExpression(r.toString());
        const QRegularExpression re(wildcard, QRegularExpression::CaseInsensitiveOption);
        return re.match(l.toString()).hasMatch();
    }
    if (node->type == ConditionAstNode::Type::CompareMatch) {
        const QRegularExpression re(r.toString(), QRegularExpression::CaseInsensitiveOption);
        return re.isValid() && re.match(l.toString()).hasMatch();
    }
    if (node->type == ConditionAstNode::Type::CompareIn) {
        if (node->right && node->right->type == ConditionAstNode::Type::List) {
            const QString lstr = l.toString().toLower();
            for (const auto &entry : node->right->listValues) {
                const QString itemVal = evalValueNode(entry, ctx, item).toString().toLower();
                if (itemVal == lstr) {
                    return true;
                }
            }
            return false;
        }
        const QStringList parts = r.toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        const QString needle = l.toString().toLower();
        for (const QString &part : parts) {
            if (part.trimmed().toLower() == needle) {
                return true;
            }
        }
        return false;
    }
    return false;
}

QVariant evalValueNode(const QSharedPointer<ConditionAstNode> &node,
                       const ActionEvalContext &ctx,
                       const ActionItemMeta *item)
{
    if (!node) {
        return {};
    }
    if (node->type == ConditionAstNode::Type::Literal) {
        return node->value;
    }
    if (node->type == ConditionAstNode::Type::Identifier) {
        return resolveIdentifier(node->identifier, ctx, item);
    }
    return evalNode(node, ctx, item);
}

} // namespace

ConditionParseResult ActionConditionParser::parse(const QString &expression) const
{
    const QString trimmed = expression.trimmed();
    if (trimmed.isEmpty()) {
        ConditionParseResult out;
        out.ok = true;
        return out;
    }
    Parser parser(trimmed);
    return parser.run();
}

bool ActionConditionEvaluator::evaluate(const QSharedPointer<ConditionAstNode> &root,
                                        const ActionEvalContext &ctx) const
{
    if (!root) {
        return true;
    }
    if (ctx.items.isEmpty()) {
        return evalNode(root, ctx, nullptr);
    }
    for (const ActionItemMeta &item : ctx.items) {
        if (!evalNode(root, ctx, &item)) {
            return false;
        }
    }
    return true;
}

} // namespace Slm::Actions

