#include "GEK\Utility\Evaluator.hpp"
#include "GEK\Utility\ShuntingYard.hpp"
#include "GEK\Utility\String.hpp"
#include "GEK\Math\Common.hpp"

namespace Gek
{
    namespace Evaluator
    {
        static ShuntingYard shuntingYard;
        void SetRandomSeed(uint32_t seed)
        {
            shuntingYard.setRandomSeed(seed);
        }

        template <typename TYPE>
        void CastResult(const wchar_t *expression, TYPE &result, TYPE defaultValue)
        {
            try
            {
                float value;
                shuntingYard.evaluate(expression, value);
                result = TYPE(value);
            }
            catch (const ShuntingYard::Exception &)
            {
                result = defaultValue;
            };
        }

        template <typename TYPE>
        void GetResult(const wchar_t *expression, TYPE &result, const TYPE &defaultValue)
        {
            try
            {
                ShuntingYard::TokenList rpnTokenList(shuntingYard.getTokenList(expression));
                switch (shuntingYard.getReturnSize(rpnTokenList))
                {
                case 1:
                    if(true)
                    {
                        float value;
                        shuntingYard.evaluate(rpnTokenList, value);
                        result.set(value);
                    }

                    break;

                default:
                    shuntingYard.evaluate(rpnTokenList, result);
                    break;
                };

            }
            catch (const ShuntingYard::Exception &)
            {
                result = defaultValue;
            };
        }

        void Get(const wchar_t *expression, int32_t &result, int32_t defaultValue)
        {
            CastResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, uint32_t &result, uint32_t defaultValue)
        {
            CastResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, float &result, float defaultValue)
        {
            CastResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, Math::Float2 &result, const Math::Float2 &defaultValue)
        {
            GetResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, Math::Float3 &result, const Math::Float3 &defaultValue)
        {
            GetResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, Math::SIMD::Float4 &result, const Math::SIMD::Float4 &defaultValue)
        {
            GetResult(expression, result, defaultValue);
        }

        void Get(const wchar_t *expression, Math::Float4 &result, const Math::Float4 &defaultValue)
        {
            try
            {
                ShuntingYard::TokenList rpnTokenList(shuntingYard.getTokenList(expression));
                switch (shuntingYard.getReturnSize(rpnTokenList))
                {
                case 1:
                    if (true)
                    {
                        float value;
                        shuntingYard.evaluate(expression, value);
                        result.set(value);
                    }

                    break;

                case 3:
                    if (true)
                    {
                        Math::Float3 rgb;
                        shuntingYard.evaluate(expression, rgb);
                        result.set(rgb.x, rgb.y, rgb.z, 1.0f);
                    }

                    break;

                default:
                    shuntingYard.evaluate(expression, result);
                    break;
                };
            }
            catch (const ShuntingYard::Exception &)
            {
                result = defaultValue;
            };
        }

        void Get(const wchar_t *expression, Math::SIMD::Quaternion &result, const Math::SIMD::Quaternion &defaultValue)
        {
            try
            {
                ShuntingYard::TokenList rpnTokenList(shuntingYard.getTokenList(expression));
                switch (shuntingYard.getReturnSize(rpnTokenList))
                {
                case 3:
                    if (true)
                    {
                        Math::Float3 euler;
                        shuntingYard.evaluate(expression, euler);
                        result = Math::SIMD::Quaternion::FromEuler(euler.x, euler.y, euler.z);
                    }

                    break;

                default:
                    shuntingYard.evaluate(expression, result);
                    break;
                };
            }
            catch (const ShuntingYard::Exception &)
            {
                result = defaultValue;
            };
        }

        void Get(const wchar_t *expression, String &result)
        {
            result = expression;
        }
    }; // namespace Evaluator
}; // namespace Gek
