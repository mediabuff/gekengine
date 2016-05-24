#pragma once

#include "GEK\Math\Float2.h"
#include "GEK\Math\Float3.h"
#include "GEK\Math\Float4.h"
#include "GEK\Math\Color.h"
#include "GEK\Math\Quaternion.h"
#include "GEK\Utility\Hash.h"
#include <Windows.h>
#include <sstream>
#include <vector>

namespace Gek
{
    template<class ELEMENT, class TRAITS = std::char_traits<ELEMENT>, class ALLOCATOR = std::allocator<ELEMENT>>
    class baseString : public std::basic_string<ELEMENT, TRAITS, ALLOCATOR>
    {
    private:
        void print(baseString<char> &target, const char *formatting, va_list variableList)
        {
            target.resize(std::vsprintf(nullptr, formatting, variableList) + 1);
            std::vsprintf(&target.front(), formatting, variableList);
        }

        void print(baseString<char> &target, const wchar_t *formatting, va_list variableList)
        {
            print(target, String::from<char>(formatting), variableList);
        }

        void print(baseString<wchar_t> &target, const char *formatting, va_list variableList)
        {
            print(target, String::from<wchar_t>(formatting), variableList);
        }

        void print(baseString<wchar_t> &target, const wchar_t *formatting, va_list variableList)
        {
            target.resize(std::vswprintf(nullptr, 0, formatting, variableList) + 1);
            std::vswprintf(&target.front(), target.size(), formatting, variableList);
        }

    public:
        using basic_string::basic_string;

        baseString(void)
        {
        }

        baseString(const baseString &string)
        {
            assign(string);
        }

        operator const ELEMENT * () const
        {
            return c_str();
        }

        operator std::basic_string<ELEMENT> & ()
        {
            return *(std::basic_string *)this;
        }

        operator const std::basic_string<ELEMENT> & () const
        {
            return *(std::basic_string *)this;
        }

        std::vector<baseString<ELEMENT>> split(ELEMENT delimiter)
        {
            baseString current;
            std::vector<baseString> tokens;
            std::basic_stringstream stream(c_str());
            while (std::getline(stream, current, delimiter))
            {
                tokens.push_back(current);
            };

            return tokens;
        }

        baseString<ELEMENT> getAppended(const ELEMENT *string)
        {
            baseString concatenated(c_str());
            concatenated.append(string);
            return concatenated;
        }

        template <typename SOURCE>
        void format(const SOURCE *formatting, va_list variableList)
        {
            print(*this, formatting, variableList);
        }

        baseString<ELEMENT> getExtension(void)
        {
            size_t position = find_last_of('.');
            if (position == std::string::npos)
            {
                return nullptr;
            }
            else
            {
                return &at(position);
            }
        }
    };

    typedef baseString<char> string;
    typedef baseString<wchar_t> wstring;

    namespace String
    {
        template <typename TYPE>
        struct string_type_of;

        template <>
        struct string_type_of<const char *>
        {
            typedef string wrap;
        };

        template <>
        struct string_type_of<const wchar_t *>
        {
            typedef wstring wrap;
        };

        template <typename TYPE>
        struct string_traits;

        template <>
        struct string_traits<string>
        {
            typedef char char_trait;
            static int byte_convert(const int codepage, const char *data, int data_length, wchar_t *buffer, int buffer_size)
            {
                return ::MultiByteToWideChar(codepage, 0, data, data_length, buffer, buffer_size);
            }
        };

        template <>
        struct string_traits<wstring>
        {
            typedef wchar_t char_trait;
            static int byte_convert(const int codepage, const wchar_t *data, int data_length, char *buffer, int buffer_size)
            {
                return ::WideCharToMultiByte(codepage, 0, data, data_length, buffer, buffer_size, nullptr, nullptr);
            }
        };

        template <typename DESTINATION, typename SOURCE>
        struct string_cast_imp
        {
            static DESTINATION cast(const SOURCE &source)
            {
                int length = string_traits<SOURCE>::byte_convert(CP_ACP, source.data(), source.length(), NULL, 0);
                if (length == 0)
                {
                    return DESTINATION();
                }

                std::vector<typename string_traits<DESTINATION>::char_trait> buffer(length);
                string_traits<SOURCE>::byte_convert(CP_ACP, source.data(), source.length(), &buffer[0], length);
                return DESTINATION(buffer.begin(), buffer.end());
            }
        };

        template <typename DESTINATION>
        struct string_cast_imp<DESTINATION, DESTINATION>
        {
            static const DESTINATION& cast(const DESTINATION &source)
            {
                return source;
            }
        };

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, double &value, double defaultValue = 0.0)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, float &value, float defaultValue = 0.0f)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, Gek::Math::Float2 &value, const Gek::Math::Float2 &defaultValue = Gek::Math::Float2(0.0f, 0.0f))
        {
            ELEMENT separator;
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> separator >> value.x >> separator >> value.y >> separator; // ( X , Y )
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, Gek::Math::Float3 &value, const Gek::Math::Float3 &defaultValue = Gek::Math::Float3(0.0f, 0.0f, 0.0f))
        {
            ELEMENT separator;
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> separator >> value.x >> separator >> value.y >> separator >> value.z >> separator; // ( x , Y , Z )
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, Gek::Math::Float4 &value, const Gek::Math::Float4 &defaultValue = Gek::Math::Float4(0.0f, 0.0f, 0.0f, 0.0f))
        {
            ELEMENT separator;
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> separator >> value.x >> separator >> value.y >> separator >> value.z >> separator >> value.w >> separator; // ( X , Y , Z , W )
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, Gek::Math::Color &value, const Gek::Math::Color &defaultValue = Gek::Math::Color(0.0f, 0.0f, 0.0f, 1.0f))
        {
            bool failed = false;
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value.r;
            if (stream.fail())
            {
                ELEMENT separator;
                stream >> separator >> value.r >> separator >> value.g >> separator >> value.b >> separator; // ( R , G , B ) or ( R , G , B ,
                if (!(failed = stream.fail()))
                {
                    switch (separator)
                    {
                    case ')':
                        value.a = 1.0f;
                        break;

                    case ',':
                        stream >> value.a >> separator;
                        failed = stream.fail();
                        break;
                    };
                }

                if (failed)
                {
                    value = defaultValue;
                }
            }
            else
            {
                value.set(value.r);
            }

            return !failed;
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, Gek::Math::Quaternion &value, const Gek::Math::Quaternion &defaultValue = Gek::Math::Quaternion::Identity)
        {
            ELEMENT separator;
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> separator >> value.x >> separator >> value.y >> separator >> value.z >> separator;
            bool failed = stream.fail();
            if (!failed)
            {
                switch (separator)
                {
                case ')':
                    value.setEulerRotation(value.x, value.y, value.z);
                    break;

                case ',':
                    stream >> value.w >> separator;
                    failed = stream.fail();
                    break;
                };
            }

            if (failed)
            {
                value = defaultValue;
            }

            return !failed;
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, INT32 &value, INT32 defaultValue = 0)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, UINT32 &value, UINT32 defaultValue = 0)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, INT64 &value, INT64 defaultValue = 0)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, UINT64 &value, UINT64 defaultValue = 0)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, bool &value, bool defaultValue = false)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream(expression);
            stream >> std::boolalpha >> value;
            if (stream.fail())
            {
                value = defaultValue;
            }

            return !stream.fail();
        }

        template <typename ELEMENT>
        bool to(const ELEMENT *expression, std::basic_string<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> &value)
        {
            value = expression;
            return true;
        }

        template <typename TYPE, typename ELEMENT>
        TYPE to(const ELEMENT *expression)
        {
            to(expression, value);
            return value;
        }

        template <typename TYPE, typename ELEMENT>
        TYPE to(const std::basic_string<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> &expression)
        {
            TYPE value;
            to(expression.data(), value);
            return value;
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(double value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << std::showpoint << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(float value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << std::showpoint << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(const Gek::Math::Float2 &value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << '(' << value.x << ',' << value.y << ')';
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(const Gek::Math::Float3 &value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << '(' << value.x << ',' << value.y << ',' << value.z << ')';
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(const Gek::Math::Float4 &value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << '(' << value.x << ',' << value.y << ',' << value.z << ',' << value.w << ')';
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(const Gek::Math::Color &value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << '(' << value.r << ',' << value.g << ',' << value.b << ',' << value.a << ')';
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(const Gek::Math::Quaternion &value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << '(' << value.x << ',' << value.y << ',' << value.z << ',' << value.w << ')';
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(INT8 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(UINT8 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(INT16 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(UINT16 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(INT32 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(UINT32 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(DWORD value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(LPCVOID value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(INT64 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(UINT64 value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT>
        baseString<ELEMENT> from(bool value)
        {
            std::basic_stringstream<ELEMENT, std::char_traits<ELEMENT>, std::allocator<ELEMENT>> stream;
            stream << std::boolalpha << value;
            return stream.str().c_str();
        }

        template <typename ELEMENT, typename SOURCE>
        baseString<ELEMENT> from(const SOURCE &source)
        {
            return string_cast_imp<baseString<ELEMENT>, SOURCE>::cast(source);
        }

        template <typename ELEMENT, typename SOURCE>
        baseString<ELEMENT> from(const SOURCE *source)
        {
            return string_cast_imp<baseString<ELEMENT>, typename string_type_of<const SOURCE *>::wrap>::cast(source);
        }

        template <typename ELEMENT>
        baseString<ELEMENT> format(const ELEMENT *formatting)
        {
            baseString<ELEMENT> result;
            while (*formatting)
            {
                if (*formatting == '%')
                {
                    ++formatting;
                    if (*formatting == '%')
                    {
                        result += *formatting++;
                    }
                    else
                    {
                        throw std::runtime_error("invalid format string: missing arguments");
                    }
                }
                else
                {
                    result += *formatting++;
                }
            };

            return result;
        }

        template<typename ELEMENT, typename TYPE, typename... ARGUMENTS>
        baseString<ELEMENT> format(const ELEMENT *formatting, TYPE value, ARGUMENTS... arguments)
        {
            baseString<ELEMENT> result;
            while (*formatting)
            {
                if (*formatting == '%')
                {
                    ++formatting;
                    if (*formatting == '%')
                    {
                        result += *formatting++;
                    }
                    else
                    {
                        ++formatting;
                        // Should verify format type here
                        result += from<ELEMENT>(value);
                        result += format(formatting, arguments...);
                        return result;
                    }
                }
                else
                {
                    result += *formatting++;
                }
            };

            throw std::logic_error("extra arguments provided to format");
        }
    }; // namespace String
}; // namespace Gek

namespace std
{
    template <>
    struct hash<Gek::string>
    {
        size_t operator()(const Gek::string &value) const
        {
            return hash<string>()(value);
        }
    };

    template <>
    struct hash<Gek::wstring>
    {
        size_t operator()(const Gek::wstring &value) const
        {
            return hash<wstring>()(value);
        }
    };
}; // namespace std