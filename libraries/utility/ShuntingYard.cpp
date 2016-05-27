#include "GEK\Utility\ShuntingYard.h"
#include "GEK\Utility\String.h"
#include "GEK\Math\Common.h"
#include <unordered_map>
#include <functional>
#include <random>
#include <stack>
#include <regex>

// https://blog.kallisti.net.nz/2008/02/extension-to-the-shunting-yard-algorithm-to-allow-variable-numbers-of-arguments-to-functions/

namespace Gek
{
    ShuntingYard::Token::Token(ShuntingYard::TokenType type)
        : type(type)
        , parameterCount(0)
    {
    }

    ShuntingYard::Token::Token(ShuntingYard::TokenType type, const wstring &string, UINT32 parameterCount)
        : type(type)
        , string(string)
        , parameterCount(parameterCount)
    {
    }

    ShuntingYard::Token::Token(float value)
        : type(TokenType::Number)
        , value(value)
    {
    }
    
    ShuntingYard::ShuntingYard(void)
        : mersineTwister(std::random_device()())
    {
        variableMap[L"pi"] = 3.14159265358979323846;
        variableMap[L"e"] = 2.71828182845904523536;

        operationsMap.insert({ L"^",{ 4, Associations::Right, nullptr, [](float valueLeft, float valueRight) -> float
        {
            return std::pow(valueLeft, valueRight);
        } } });

        operationsMap.insert({ L"*",{ 3, Associations::Left, nullptr, [](float valueLeft, float valueRight) -> float
        {
            return (valueLeft * valueRight);
        } } });

        operationsMap.insert({ L"/",{ 3, Associations::Left, nullptr, [](float valueLeft, float valueRight) -> float
        {
            return (valueLeft / valueRight);
        } } });

        operationsMap.insert({ L"+",{ 2, Associations::Left, [](float value) -> float
        {
            return value;
        }, [](float valueLeft, float valueRight) -> float
        {
            return (valueLeft + valueRight);
        } } });

        operationsMap.insert({ L"-",{ 2, Associations::Left, [](float value) -> float
        {
            return -value;
        }, [](float valueLeft, float valueRight) -> float
        {
            return (valueLeft - valueRight);
        } } });

        functionsMap.insert({ L"sin",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::sin(value);
        } } });

        functionsMap.insert({ L"cos",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::cos(value);
        } } });

        functionsMap.insert({ L"tan",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::tan(value);
        } } });

        functionsMap.insert({ L"asin",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::asin(value);
        } } });

        functionsMap.insert({ L"acos",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::acos(value);
        } } });

        functionsMap.insert({ L"atan",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::atan(value);
        } } });

        functionsMap.insert({ L"min",{ 2, [](Stack<Token> &stack) -> float
        {
            float value2 = stack.popTop().value;
            float value1 = stack.popTop().value;
            return std::min(value1, value2);
        } } });

        functionsMap.insert({ L"max",{ 2, [](Stack<Token> &stack) -> float
        {
            float value2 = stack.popTop().value;
            float value1 = stack.popTop().value;
            return std::max(value1, value2);
        } } });

        functionsMap.insert({ L"abs",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::abs(value);
        } } });

        functionsMap.insert({ L"ceil",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::ceil(value);
        } } });

        functionsMap.insert({ L"floor",{ 1, [](Stack<Token> &stack) -> float
        {
            float value = stack.popTop().value;
            return std::floor(value);
        } } });

        functionsMap.insert({ L"lerp",{ 3, [](Stack<Token> &stack) -> float
        {
            float value3 = stack.popTop().value;
            float value2 = stack.popTop().value;
            float value1 = stack.popTop().value;
            return Math::lerp(value1, value2, value3);
        } } });

        functionsMap.insert({ L"random",{ 2, [&](Stack<Token> &stack) -> float
        {
            float value2 = stack.popTop().value;
            float value1 = stack.popTop().value;
            std::uniform_real_distribution<float> uniformRealDistribution(value1, value2);
            return uniformRealDistribution(mersineTwister);
        } } });
    }

    void ShuntingYard::evaluteTokenList(const wchar_t *expression, std::vector<Token> &rpnTokenList)
    {
        std::vector<Token> infixTokenList = convertExpressionToInfix(expression);
        convertInfixToReversePolishNotation(infixTokenList, rpnTokenList);
    }

    void ShuntingYard::evaluateValue(std::vector<Token> &rpnTokenList, float *value, UINT32 valueSize)
    {
        evaluateReversePolishNotation(rpnTokenList, value, valueSize);
    }

    void ShuntingYard::evaluateValue(const wchar_t *expression, float *value, UINT32 valueSize)
    {
        std::vector<Token> rpnTokenList;
        evaluteTokenList(expression, rpnTokenList);
        evaluateReversePolishNotation(rpnTokenList, value, valueSize);
    }

    bool ShuntingYard::isNumber(const wstring &token)
    {
        return std::regex_match(token.c_str(), std::wregex(L"^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?$"));
    }

    bool ShuntingYard::isOperation(const wstring &token)
    {
        return (operationsMap.count(token) > 0);
    }

    bool ShuntingYard::isFunction(const wstring &token)
    {
        return (functionsMap.count(token) > 0);
    }

    bool ShuntingYard::isLeftParenthesis(const wstring &token)
    {
        return token.front() == L'(';
    }

    bool ShuntingYard::isRightParenthesis(const wstring &token)
    {
        return token.front() == L')';
    }

    bool ShuntingYard::isParenthesis(const wstring &token)
    {
        return (isLeftParenthesis(token) || isRightParenthesis(token));
    }

    bool ShuntingYard::isSeparator(const wstring &token)
    {
        return token.front() == L',';
    }

    bool ShuntingYard::isAssociative(const wstring &token, const Associations &type)
    {
        auto &p = operationsMap.find(token)->second;
        return p.association == type;
    }

    int ShuntingYard::comparePrecedence(const wstring &token1, const wstring &token2)
    {
        auto &p1 = operationsMap.find(token1)->second;
        auto &p2 = operationsMap.find(token2)->second;
        return p1.precedence - p2.precedence;
    }

    ShuntingYard::TokenType ShuntingYard::getTokenType(const wstring &token)
    {
        if (isNumber(token))
        {
            return TokenType::Number;
        }
        else if (isOperation(token))
        {
            return TokenType::BinaryOperation;
        }
        else if (isFunction(token))
        {
            return TokenType::Function;
        }
        else if (isSeparator(token))
        {
            return TokenType::Separator;
        }
        else if (isLeftParenthesis(token))
        {
            return TokenType::LeftParenthesis;
        }
        else if (isRightParenthesis(token))
        {
            return TokenType::RightParenthesis;
        }

        return TokenType::Unknown;
    }

    void ShuntingYard::insertToken(std::vector<Token> &infixTokenList, Token &token)
    {
        if (!infixTokenList.empty())
        {
            const Token &previous = infixTokenList.back();

            // ) 2 or 2 2
            if (token.type == TokenType::Number && (previous.type == TokenType::Number || previous.type == TokenType::RightParenthesis))
            {
                infixTokenList.push_back(Token(TokenType::BinaryOperation, L"*"));
            }
            // 2 ( or ) (
            else if (token.type == TokenType::LeftParenthesis && (previous.type == TokenType::Number || previous.type == TokenType::RightParenthesis))
            {
                infixTokenList.push_back(Token(TokenType::BinaryOperation, L"*"));
            }
            // ) sin or 2 sin
            else if (token.type == TokenType::Function && (previous.type == TokenType::Number || previous.type == TokenType::RightParenthesis))
            {
                infixTokenList.push_back(Token(TokenType::BinaryOperation, L"*"));
            }
        }

        if (token.type == TokenType::BinaryOperation)
        {
            if (token.string == L"-" || token.string == L"+")
            {
                // -3 or -sin
                if (infixTokenList.empty())
                {
                    token.type = TokenType::UnaryOperation;
                }
                else
                {
                    const Token &previous = infixTokenList.back();

                    // 2+-3 or sin(1)*-1
                    if (previous.type == TokenType::BinaryOperation || previous.type == TokenType::UnaryOperation)
                    {
                        token.type = TokenType::UnaryOperation;
                    }
                    // (-3)
                    else if (previous.type == TokenType::LeftParenthesis)
                    {
                        token.type = TokenType::UnaryOperation;
                    }
                    // ,-3
                    else if (previous.type == TokenType::Separator)
                    {
                        token.type = TokenType::UnaryOperation;
                    }
                }
            }
        }

        infixTokenList.push_back(token);
    }

    bool ShuntingYard::replaceFirstVariable(std::vector<Token> &infixTokenList, wstring &token)
    {
        for (auto &variable : variableMap)
        {
            if (token.find(variable.first) == std::string::npos)
            {
                insertToken(infixTokenList, Token(variable.second));
                token = token.subString(variable.first.size());
                return true;
            }
        }

        return false;
    }

    bool ShuntingYard::replaceFirstFunction(std::vector<Token> &infixTokenList, wstring &token)
    {
        for (auto &function : functionsMap)
        {
            if (token.find(function.first) == std::string::npos)
            {
                insertToken(infixTokenList, Token(TokenType::Function, function.first));
                token = token.subString(function.first.size());
                return true;
            }
        }

        return false;
    }

    void ShuntingYard::parseSubTokens(std::vector<Token> &infixTokenList, wstring token)
    {
        while (!token.empty())
        {
            std::wcmatch matches;
            if (std::regex_search(token.c_str(), matches, std::wregex(L"^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?")))
            {
                wstring value = matches[0].str().c_str();
                insertToken(infixTokenList, Token(Gek::String::to<float>(value)));
                token = token.subString(value.size());
                continue;
            }

            if (replaceFirstVariable(infixTokenList, token))
            {
                continue;
            }

            if (replaceFirstFunction(infixTokenList, token))
            {
                GEK_THROW_ERROR(!token.empty(), MissingFunctionParenthesis, "Unbalanced scope, missing parenthesis: %v", token);
                continue;
            }

            GEK_THROW_ERROR(!token.empty(), UnknownTokenType, "Unknown tokens remaining: %v", token);
        };
    }

    std::vector<ShuntingYard::Token> ShuntingYard::convertExpressionToInfix(const wstring &expression)
    {
        wstring runningToken;
        std::vector<Token> infixTokenList;
        for (size_t index = 0; index < expression.size(); ++index)
        {
            wstring nextToken(expression.subString(index, 1));
            if (isOperation(nextToken) || isParenthesis(nextToken) || isSeparator(nextToken))
            {
                if (!runningToken.empty())
                {
                    parseSubTokens(infixTokenList, runningToken);
                    runningToken.clear();
                }

                insertToken(infixTokenList, Token(getTokenType(nextToken), nextToken));
            }
            else
            {
                if (nextToken == L" ")
                {
                    if (!runningToken.empty())
                    {
                        parseSubTokens(infixTokenList, runningToken);
                        runningToken.clear();
                    }
                }
                else
                {
                    runningToken.append(nextToken);
                }
            }
        }

        if (!runningToken.empty())
        {
            parseSubTokens(infixTokenList, runningToken);
        }

        return infixTokenList;
    }

    void ShuntingYard::convertInfixToReversePolishNotation(const std::vector<Token> &infixTokenList, std::vector<Token> &rpnTokenList)
    {
        Stack<Token> stack;
        Stack<bool> parameterExistsStack;
        Stack<UINT32> parameterCountStack;
        for (auto &token : infixTokenList)
        {
            switch (token.type)
            {
            case TokenType::Number:
                rpnTokenList.push_back(token);
                if (!parameterExistsStack.empty())
                {
                    parameterExistsStack.pop();
                    parameterExistsStack.push(true);
                }

                break;

            case TokenType::UnaryOperation:
                stack.push(token);
                break;

            case TokenType::BinaryOperation:
                while (!stack.empty() && (stack.top().type == TokenType::BinaryOperation &&
                    (isAssociative(token.string, Associations::Left) && comparePrecedence(token.string, stack.top().string) == 0) ||
                    (isAssociative(token.string, Associations::Right) && comparePrecedence(token.string, stack.top().string) < 0)))
                {
                    rpnTokenList.push_back(stack.popTop());
                };

                stack.push(token);
                break;

            case TokenType::LeftParenthesis:
                // only return vector as a final value
                if (stack.empty() || (stack.top().type == TokenType::Function))
                {
                    parameterCountStack.push(0);
                    if (!parameterExistsStack.empty())
                    {
                        parameterExistsStack.pop();
                        parameterExistsStack.push(true);
                    }

                    parameterExistsStack.push(false);
                }

                stack.push(token);
                break;

            case TokenType::RightParenthesis:
                GEK_THROW_ERROR(stack.empty(), UnbalancedParenthesis, "Unmatched ending parenthesis found");
                while (stack.top().type != TokenType::LeftParenthesis)
                {
                    rpnTokenList.push_back(stack.popTop());
                    GEK_THROW_ERROR(stack.empty(), UnbalancedParenthesis, "Unmatched ending parenthesis found while parsing block");
                };

                stack.pop();
                if (stack.empty())
                {
                    Token vector(TokenType::Vector);
                    vector.parameterCount = parameterCountStack.popTop();
                    if (parameterExistsStack.popTop())
                    {
                        vector.parameterCount++;
                    }

                    // don't push single value ()'s as vectors
                    if (vector.parameterCount > 1)
                    {
                        rpnTokenList.push_back(vector);
                    }
                }
                else if (stack.top().type == TokenType::Function)
                {
                    Token function = stack.popTop();
                    function.parameterCount = parameterCountStack.popTop();
                    if (parameterExistsStack.popTop())
                    {
                        function.parameterCount++;
                    }

                    rpnTokenList.push_back(function);
                }

                break;

            case TokenType::Separator:
                GEK_THROW_ERROR(stack.empty(), MisplacedSeparator, "Comma found without a leading value");
                GEK_THROW_ERROR(parameterExistsStack.empty(), MisplacedSeparator, "Comma found without a leading value");
                while (stack.top().type != TokenType::LeftParenthesis)
                {
                    rpnTokenList.push_back(stack.popTop());
                    GEK_THROW_ERROR(stack.empty(), MisplacedSeparator, "Comma found without a starting parenthesis while parsing block");
                };

                if (parameterExistsStack.top())
                {
                    parameterCountStack.top()++;
                }

                parameterExistsStack.pop();
                parameterExistsStack.push(false);
                break;

            case TokenType::Function:
                stack.push(token);
                break;

            default:
                GEK_THROW_EXCEPTION(UnknownTokenType, "Unknown token type: %v", token.string);
            };
        }

        while (!stack.empty())
        {
            GEK_THROW_ERROR(stack.top().type == TokenType::LeftParenthesis, UnbalancedParenthesis, "Unused left parenthesis found while unwinding the stack");
            GEK_THROW_ERROR(stack.top().type == TokenType::RightParenthesis, UnbalancedParenthesis, "Unused right parenthesis found while unwinding the stack");
            rpnTokenList.push_back(stack.popTop());
        };
    }

    void ShuntingYard::evaluateReversePolishNotation(const std::vector<Token> &rpnTokenList, float *value, UINT32 valueSize)
    {
        Stack<Token> stack;
        for (auto &token : rpnTokenList)
        {
            switch (token.type)
            {
            case TokenType::Number:
                stack.push(token);
                break;

            case TokenType::UnaryOperation:
                if (true)
                {
                    auto &operationIterater = operationsMap.find(token.string);
                    GEK_THROW_ERROR(operationIterater == operationsMap.end(), InvalidOperator, "Unable to find operation: %v", token.string);

                    auto &operation = (*operationIterater).second;
                    GEK_THROW_ERROR(!operation.unaryFunction, InvalidOperator, "No unary function found for operation: %v", token.string);

                    GEK_THROW_ERROR(stack.empty(), InvalidOperand, "No values found for unary operator: %v", token.string);
                    GEK_THROW_ERROR(stack.top().type != TokenType::Number, InvalidOperand, "Invalid token found for unary operator: %v", stack.top().string);
                    float functionValue = stack.popTop().value;

                    stack.push(Token(operation.unaryFunction(functionValue)));
                    break;
                }

            case TokenType::BinaryOperation:
                if (true)
                {
                    auto &operationIterater = operationsMap.find(token.string);
                    GEK_THROW_ERROR(operationIterater == operationsMap.end(), InvalidOperator, "Unable to find operation: %v", token.string);

                    auto &operation = (*operationIterater).second;
                    GEK_THROW_ERROR(!operation.binaryFunction, InvalidOperator, "No binary function found for operation: %v", token.string);


                    GEK_THROW_ERROR(stack.empty(), InvalidOperand, "No right value found for unary operator: %v", token.string);
                    GEK_THROW_ERROR(stack.top().type != TokenType::Number, InvalidOperand, "Invalid right token found for unary operator: %v", stack.top().string);
                    float functionValueRight = stack.popTop().value;

                    GEK_THROW_ERROR(stack.empty(), InvalidOperand, "No left value found for unary operator: %v", token.string);
                    GEK_THROW_ERROR(stack.top().type != TokenType::Number, InvalidOperand, "Invalid left token found for unary operator: %v", stack.top().string);
                    float functionValueLeft = stack.popTop().value;

                    stack.push(Token(operation.binaryFunction(functionValueLeft, functionValueRight)));
                    break;
                }

            case TokenType::Function:
                if (true)
                {
                    auto &functionIterator = functionsMap.find(token.string);
                    GEK_THROW_ERROR(functionIterator == functionsMap.end(), InvalidFunction, "Unable to find operation: %v", token.string);

                    auto &function = (*functionIterator).second;
                    GEK_THROW_ERROR(function.parameterCount != token.parameterCount, InvalidFunctionParameters, "Mismatched function parameters found");
                    GEK_THROW_ERROR(stack.size() < function.parameterCount, NotEnoughFunctionParameters, "Not enough function parameters found");

                    stack.push(Token(function.function(stack)));
                    break;
                }

            case TokenType::Vector:
                GEK_THROW_ERROR(token.parameterCount != valueSize, InvalidVector, "Mismatched vector type found");
                GEK_THROW_ERROR(stack.size() != valueSize, InvalidVector, "Not enough values for required vector found");
                for (UINT32 axis = valueSize; axis > 0; axis--)
                {
                    value[axis - 1] = stack.popTop().value;
                }

            default:
                GEK_THROW_EXCEPTION(UnknownTokenType, "Unknown token type: %v", token.string);
            };
        }

        GEK_THROW_ERROR(valueSize != 1, InvalidEquation, "Returning too many values for requested type");
        GEK_THROW_ERROR(stack.size() != valueSize, InvalidEquation, "Returning too many values for requested type");
        (*value) = stack.top().value;
    }
}; // namespace Gek
