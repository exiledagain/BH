#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

enum class FormulaStatus
{
    OK = 0,
    LEXICAL_ERROR,
    SYNTAX_ERROR,
    MATH_ERROR,
    ARG_COUNT_ERROR
};

enum class FormulaTokenType
{
    NUMBER,
    VARIABLE,
    OP,
    OPEN_P,
    CLOSE_P,
    COMMA,
    T_EOF
};

enum class FormulaOpCode
{
    NONE,
    LITERAL,
    RESOLVER,
    ADD,
    SUB,
    MUL,
    DIV,
    POW,
    EQ,
    NE,
    GT,
    LT,
    GE,
    LE,
    AND,
    OR,
    IF,
    NEGATE,
    NOT,
    LN,
    EXP,
    FLOOR,
    CEIL,
    ROUND,
    MIN,
    MAX
};

template<typename T>
struct FormulaVarDefinition
{
    std::string prefix;
    int paramCount;
    std::function<float(T, const std::vector<int>&)> resolver;

    FormulaVarDefinition(std::string pre, int params, std::function<float(T, const std::vector<int>&)> res)
        : prefix(pre),
        paramCount(params),
        resolver(res)
    {
    }
};

template<typename T>
struct FormulaNode
{
    FormulaOpCode op = FormulaOpCode::NONE;
    float literalValue = 0;
    std::vector<std::unique_ptr<FormulaNode<T>>> children;
    std::function<float(T)> resolver = nullptr;

    FormulaNode(FormulaOpCode o) : op(o)
    {
    }
    FormulaNode(float val) : op(FormulaOpCode::LITERAL), literalValue(val)
    {
    }
};

template<typename T>
class Formula
{
    static float eval(FormulaNode<T>* n, T ctx, FormulaStatus& e);
    static void optimize(std::unique_ptr<FormulaNode<T>>& n);

    std::unique_ptr<FormulaNode<T>> root;
public:
	static FormulaStatus Compile(const std::string& raw, std::unique_ptr<Formula>& out, const std::vector<FormulaVarDefinition<T>>& reg);

    Formula(std::unique_ptr<FormulaNode<T>> r) : root(std::move(r))
    {
    }

    FormulaStatus execute(T ctx, float& ret) const;
};

struct FormulaToken
{
    FormulaTokenType type;
    std::string value;
};

class FormulaTokenStream
{
    std::vector<FormulaToken> tokens;
    size_t pos = 0;

public:
    void add(FormulaToken t)
    {
        tokens.push_back(t);
    }
    const FormulaToken& peek() const
    {
        static FormulaToken eof_token = { FormulaTokenType::T_EOF, "" };
        return (pos >= tokens.size()) ? eof_token : tokens[pos];
    }
    void advance()
    {
        if (pos < tokens.size())
        {
            pos++;
        }
    }
    bool is_at_end() const
    {
        return pos >= tokens.size() || peek().type == FormulaTokenType::T_EOF;
    }
};

class FormulaScanner
{
public:
    static FormulaStatus tokenize(const std::string& input, FormulaTokenStream& outStream)
    {
        size_t i = 0;
        while (i < input.length())
        {
            if (std::isspace(input[i]))
            {
                i++;
                continue;
            }

            if (i + 1 < input.length())
            {
                std::string op2 = input.substr(i, 2);
                if (op2 == "==" || op2 == "!=" || op2 == ">=" || op2 == "<=")
                {
                    outStream.add({ FormulaTokenType::OP, op2 });
                    i += 2;
                    continue;
                }
            }
            if (std::strchr("+-*/^<>!", input[i]))
            {
                outStream.add({ FormulaTokenType::OP, std::string(1, input[i]) });
                i++;
                continue;
            }
            if (input[i] == '(')
            {
                outStream.add({ FormulaTokenType::OPEN_P, "(" });
                i++;
                continue;
            }
            if (input[i] == ')')
            {
                outStream.add({ FormulaTokenType::CLOSE_P, ")" });
                i++;
                continue;
            }
            if (input[i] == ',')
            {
                outStream.add({ FormulaTokenType::COMMA, "," });
                i++;
                continue;
            }

            if (std::isdigit(input[i]) || input[i] == '.')
            {
                size_t start = i++;
                while (i < input.length() && (std::isdigit(input[i]) || input[i] == '.'))
                {
                    i++;
                }
                outStream.add({ FormulaTokenType::NUMBER, input.substr(start, i - start) });
                continue;
            }

            if (std::isalpha(input[i]) || input[i] == '_')
            {
                size_t start = i++;
                while (i < input.length() && (std::isalpha(input[i]) || input[i] == '_'))
                {
                    i++;
                }
                outStream.add({ FormulaTokenType::VARIABLE, input.substr(start, i - start) });
                continue;
            }
            return FormulaStatus::LEXICAL_ERROR;
        }
        return FormulaStatus::OK;
    }
};

template<typename T>
class FormulaParser
{
    FormulaTokenStream& stream;
    FormulaStatus& err;
    const std::vector<FormulaVarDefinition<T>>& registry;

    // lower precedence ops are evaluated after higher ones
    const std::map<std::string, std::pair<FormulaOpCode, int>> opTable = {
        {"==", {FormulaOpCode::EQ, 1}},
        {">", {FormulaOpCode::GT, 1}},
        {"<", {FormulaOpCode::LT, 1}},
        {"!=", {FormulaOpCode::NE, 1}},
        {">=", {FormulaOpCode::GE, 1}},
        {"<=", {FormulaOpCode::LE, 1}},
        {"+", {FormulaOpCode::ADD, 2}},
        {"-", {FormulaOpCode::SUB, 2}},
        {"*", {FormulaOpCode::MUL, 3}},
        {"/", {FormulaOpCode::DIV, 3}},
        {"^", {FormulaOpCode::POW, 4}}
    };

    const std::map<std::string, FormulaOpCode> fnTable = {
        {"if", FormulaOpCode::IF},
        {"and", FormulaOpCode::AND},
        {"or", FormulaOpCode::OR},
        {"ln", FormulaOpCode::LN},
        {"exp", FormulaOpCode::EXP},
        {"floor", FormulaOpCode::FLOOR},
        {"ceil", FormulaOpCode::CEIL},
        {"round", FormulaOpCode::ROUND},
        {"min", FormulaOpCode::MIN},
        {"max", FormulaOpCode::MAX}
    };

public:
    FormulaParser(FormulaTokenStream& ts, FormulaStatus& e, const std::vector<FormulaVarDefinition<T>>& reg)
        : stream(ts),
        err(e),
        registry(reg)
    {
    }

    std::unique_ptr<FormulaNode<T>> parse()
    {
        auto res = parseExpression(0);
        if (!stream.is_at_end())
        {
            err = FormulaStatus::SYNTAX_ERROR;
        }
        return res;
    }

private:
    std::unique_ptr<FormulaNode<T>> parseExpression(int minPrec)
    {
        auto left = parseUnary();
        if (!left)
        {
            return nullptr;
        }

        while (true)
        {
            const FormulaToken& t = stream.peek();
            if (t.type != FormulaTokenType::OP || opTable.count(t.value) == 0)
            {
                break;
            }

            auto attr = opTable.at(t.value);
            if (attr.second < minPrec)
            {
                break;
            }

            stream.advance();
            auto n = std::make_unique<FormulaNode<T>>(attr.first);
            n->children.push_back(std::move(left));
            auto right = parseExpression(attr.second + 1);
            if (!right)
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }
            n->children.push_back(std::move(right));
            left = std::move(n);
        }
        return left;
    }

    std::unique_ptr<FormulaNode<T>> parseUnary()
    {
        if (stream.peek().value == "-")
        {
            stream.advance();
            auto n = std::make_unique<FormulaNode<T>>(FormulaOpCode::NEGATE);
            auto child = parseUnary();
            if (!child)
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }
            n->children.push_back(std::move(child));
            return n;
        }
        if (stream.peek().value == "+")
        {
            stream.advance();
            return parseUnary();
        }
        if (stream.peek().value == "!")
        {
            stream.advance();
            auto n = std::make_unique<FormulaNode<T>>(FormulaOpCode::NOT);
            auto child = parseUnary();
            if (!child)
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }
            n->children.push_back(std::move(child));
            return n;
        }
        return parsePrimary();
    }

    std::unique_ptr<FormulaNode<T>> parsePrimary()
    {
        const FormulaToken& t = stream.peek();

        if (t.type == FormulaTokenType::VARIABLE)
        {
            std::string name = t.value;

            if (fnTable.count(name))
            {
                return parseFunction(name);
            }

            const FormulaVarDefinition<T>* def = nullptr;
            for (auto& d : registry)
            {
                if (d.prefix == name)
                {
                    def = &d;
                }
            }

            if (!def)
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }

            stream.advance();

            std::vector<int> ids;

            for (int i = 0; i < def->paramCount; ++i)
            {
                if (i > 0)
                {
                    if (stream.peek().type == FormulaTokenType::COMMA)
                    {
                        stream.advance();
                    }
                    else
                    {
                        err = FormulaStatus::SYNTAX_ERROR;
                        return nullptr;
                    }
                }

                if (stream.peek().type == FormulaTokenType::NUMBER)
                {
                    ids.push_back(std::stoi(stream.peek().value));
                    stream.advance();
                }
                else
                {
                    err = FormulaStatus::SYNTAX_ERROR;
                    return nullptr;
                }
            }

            auto n = std::make_unique<FormulaNode<T>>(FormulaOpCode::RESOLVER);
            auto res = def->resolver;
            n->resolver = [res, ids](T ctx)
            {
                return res(ctx, ids);
            };
            return n;
        }

        if (t.type == FormulaTokenType::NUMBER)
        {
            float val = std::stod(t.value);
            stream.advance();
            return std::make_unique<FormulaNode<T>>(val);
        }

        if (t.type == FormulaTokenType::OPEN_P)
        {
            stream.advance();
            auto n = parseExpression(0);
            if (stream.peek().type == FormulaTokenType::CLOSE_P)
            {
                stream.advance();
            }
            else
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }
            return n;
        }

        err = FormulaStatus::SYNTAX_ERROR;
        return nullptr;
    }

    std::unique_ptr<FormulaNode<T>> parseFunction(std::string name)
    {
        FormulaOpCode code = fnTable.at(name);
        stream.advance();

        auto n = std::make_unique<FormulaNode<T>>(code);

        if (stream.peek().type == FormulaTokenType::OPEN_P)
        {
            stream.advance();
            if (stream.peek().type != FormulaTokenType::CLOSE_P)
            {
                while (true)
                {
                    auto arg = parseExpression(0);
                    if (!arg)
                    {
                        err = FormulaStatus::SYNTAX_ERROR;
                        return nullptr;
                    }
                    n->children.push_back(std::move(arg));

                    if (stream.peek().type == FormulaTokenType::COMMA)
                    {
                        stream.advance();
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if (stream.peek().type == FormulaTokenType::CLOSE_P)
            {
                stream.advance();
            }
            else
            {
                err = FormulaStatus::SYNTAX_ERROR;
                return nullptr;
            }
        }
        else
        {
            err = FormulaStatus::SYNTAX_ERROR;
            return nullptr;
        }

        size_t count = n->children.size();
        bool ok = false;
        switch (code)
        {
        case FormulaOpCode::LN:
        case FormulaOpCode::EXP:
        case FormulaOpCode::FLOOR:
        case FormulaOpCode::CEIL:
        case FormulaOpCode::ROUND:
        {
            ok = count == 1;
            break;
        }
        case FormulaOpCode::IF:
        {
            ok = count == 3;
            break;
        }
        case FormulaOpCode::AND:
        case FormulaOpCode::OR:
        case FormulaOpCode::MIN:
        case FormulaOpCode::MAX:
        {
            ok = count > 0;
            break;
        }
        }
        if (!ok)
        {
            err = FormulaStatus::ARG_COUNT_ERROR;
            return nullptr;
        }

        return n;
    }
};

template<typename T>
float Formula<T>::eval(FormulaNode<T>* n, T ctx, FormulaStatus& e)
{
    if (e != FormulaStatus::OK || !n)
    {
        return 0;
    }

    auto check = [&](size_t req)
    {
        if (n->children.size() < req)
        {
            e = FormulaStatus::ARG_COUNT_ERROR;
            return false;
        }
        return true;
    };

    switch (n->op)
    {
        case FormulaOpCode::LITERAL:
        {
            return n->literalValue;
        }
        case FormulaOpCode::RESOLVER:
        {
            return n->resolver(ctx);
        }
        case FormulaOpCode::NEGATE:
        {
            if (!check(1))
            {
                return 0;
            }
            return -eval(n->children[0].get(), ctx, e);
        }
        case FormulaOpCode::NOT:
        {
            if (!check(1))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) == 0;
        }
        case FormulaOpCode::ADD:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) + eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::SUB:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) - eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::MUL:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) * eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::DIV:
        {
            if (!check(2))
            {
                return 0;
            }
            float d = eval(n->children[1].get(), ctx, e);
            if (d == 0.0f)
            {
                e = FormulaStatus::MATH_ERROR;
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) / d;
        }
        case FormulaOpCode::POW:
        {
            if (!check(2))
            {
                return 0;
            }
            return std::pow(eval(n->children[0].get(), ctx, e), eval(n->children[1].get(), ctx, e));
        }
        case FormulaOpCode::EQ:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) == eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::NE:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) != eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::GT:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) > eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::LT:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) < eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::GE:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) >= eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::LE:
        {
            if (!check(2))
            {
                return 0;
            }
            return eval(n->children[0].get(), ctx, e) <= eval(n->children[1].get(), ctx, e);
        }
        case FormulaOpCode::IF:
        {
            if (!check(3))
            {
                return 0;
            }
            return (eval(n->children[0].get(), ctx, e) != 0) ? eval(n->children[1].get(), ctx, e) : eval(n->children[2].get(), ctx, e);
        }
        case FormulaOpCode::AND:
        {
            for (auto& c : n->children)
            {
                if (eval(c.get(), ctx, e) == 0)
                {
                    return 0;
                }
            }
            return 1;
        }
        case FormulaOpCode::OR:
        {
            for (auto& c : n->children)
            {
                if (eval(c.get(), ctx, e) != 0)
                {
                    return 1;
                }
            }
            return 0;
        }
        case FormulaOpCode::LN:
        {
            if (!check(1))
            {
                return 0;
            }
            return std::log(eval(n->children[0].get(), ctx, e));
        }
        case FormulaOpCode::EXP:
        {
            if (!check(1))
            {
                return 0;
            }
            return std::exp(eval(n->children[0].get(), ctx, e));
        }
        case FormulaOpCode::FLOOR:
        {
            if (!check(1))
            {
                return 0;
            }
            return std::floor(eval(n->children[0].get(), ctx, e));
        }
        case FormulaOpCode::CEIL:
        {
            if (!check(1))
            {
                return 0;
            }
            return std::ceil(eval(n->children[0].get(), ctx, e));
        }
        case FormulaOpCode::ROUND:
        {
            if (!check(1))
            {
                return 0;
            }
            return std::round(eval(n->children[0].get(), ctx, e));
        }
        case FormulaOpCode::MIN:
        {
            float res = FLT_MAX;
            for (auto& c : n->children)
            {
                float v = eval(c.get(), ctx, e);
                res = min(res, v);
            }
            return res;
        }
        case FormulaOpCode::MAX:
        {
            float res = FLT_MIN;
            for (auto& c : n->children)
            {
                float v = eval(c.get(), ctx, e);
                res = max(res, v);
            }
            return res;
        }
        default:
        {
            return 0;
        }
    }
}

template<typename T>
void Formula<T>::optimize(std::unique_ptr<FormulaNode<T>>& n)
{
    if (!n)
    {
        return;
    }
    for (auto& child : n->children)
    {
        optimize(child);
    }

    bool allLiterals = !n->children.empty();
    for (const auto& c : n->children)
    {
        if (c->op != FormulaOpCode::LITERAL)
        {
            allLiterals = false;
        }
    }

    if (allLiterals && n->op != FormulaOpCode::RESOLVER && n->op != FormulaOpCode::LITERAL)
    {
        FormulaStatus err = FormulaStatus::OK;
        float val = eval(n.get(), nullptr, err);
        if (err == FormulaStatus::OK)
        {
            n->children.clear();
            n->op = FormulaOpCode::LITERAL;
            n->literalValue = val;
        }
    }
}

template<typename T>
FormulaStatus Formula<T>::Compile(const std::string& raw, std::unique_ptr<Formula<T>>& out, const std::vector<FormulaVarDefinition<T>>& reg)
{
    FormulaTokenStream ts;
    std::string input = raw;
    std::transform(input.begin(), input.end(), input.begin(), tolower);

    FormulaStatus err = FormulaScanner::tokenize(input, ts);
    if (err != FormulaStatus::OK)
    {
        return err;
    }

    FormulaParser<T> p(ts, err, reg);
    auto rootNode = p.parse();

    if (err != FormulaStatus::OK || !rootNode)
    {
        return (err == FormulaStatus::OK) ? FormulaStatus::SYNTAX_ERROR : err;
    }

    optimize(rootNode);
    out = std::make_unique<Formula>(std::move(rootNode));
    return FormulaStatus::OK;
}

template<typename T>
FormulaStatus Formula<T>::execute(T ctx, float& ret) const
{
    FormulaStatus e = FormulaStatus::OK;
    ret = eval(root.get(), ctx, e);
    return e;
}
