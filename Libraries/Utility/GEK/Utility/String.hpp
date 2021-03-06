/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision: 31a07b88fab4425367fa0aa67fe970fbff7dc9dc $
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date:   Thu Oct 27 08:51:53 2016 -0700 $
#pragma once

#include <Windows.h>
#include <string_view>
#include <functional>
#include <algorithm>
#include <sstream>
#include <codecvt>
#include <string>
#include <vector>
#include <ppl.h>

using namespace std::string_literals; // enables s-suffix for std::string literals  

namespace std
{
    inline std::string const &to_string(std::string const &string)
    {
        return string;
    }

    inline std::string to_string(void const *pointer)
    {
        return std::to_string(reinterpret_cast<size_t>(pointer));
    }

    inline std::string to_string(char const *data)
    {
        return std::string(data);
    }
};

namespace Gek
{
    class LockedWrite
        : public std::ostringstream
    {
    private:
        static std::mutex mutex;
        std::ostream &stream;

    public:
        LockedWrite(std::ostream &stream)
            : stream(stream)
        {
        }

        ~LockedWrite()
        {
            (*this) << std::endl;
            if (IsDebuggerPresent())
            {
                OutputDebugStringA(str().c_str());
            }
            else
            {
                stream << str();
            }
        }
    };

    namespace String
    {
        extern const std::string Empty;
        extern const std::locale Locale;

        template <typename CONTAINER>
        void TrimLeft(CONTAINER &string, std::function<bool(typename CONTAINER::value_type)> checkElement = [](typename CONTAINER::value_type ch) { return !std::isspace(ch, Locale); })
        {
            string.erase(std::begin(string), std::find_if(std::begin(string), std::end(string), checkElement));
        }

        template <typename CONTAINER>
        void TrimRight(CONTAINER &string, std::function<bool(typename CONTAINER::value_type)> checkElement = [](typename CONTAINER::value_type ch) { return !std::isspace(ch, Locale); })
        {
            string.erase(std::find_if(std::rbegin(string), std::rend(string), checkElement).base(), std::end(string));
        }

        template <typename CONTAINER>
        void Trim(CONTAINER &string, std::function<bool(typename CONTAINER::value_type)> checkElement = [](typename CONTAINER::value_type ch) { return !std::isspace(ch, Locale); })
        {
            TrimLeft(string, checkElement);
            TrimRight(string, checkElement);
        }

        template <typename CONTAINER>
        CONTAINER GetLower(CONTAINER const &string)
        {
            CONTAINER transformed;
            std::transform(std::begin(string), std::end(string), std::back_inserter(transformed), ::tolower);
            return transformed;
        }

        template <typename CONTAINER>
        CONTAINER GetUpper(CONTAINER const &string)
        {
            CONTAINER transformed;
            std::transform(std::begin(string), std::end(string), std::back_inserter(transformed), ::toupper);
            return transformed;
        }

		inline bool EndsWith(std::string_view const & value, std::string_view const & ending)
		{
			return ((ending.size() > value.size()) ? false : std::equal(std::rbegin(ending), std::rend(ending), std::rbegin(value)));
		}

		std::string Join(std::vector<std::string> const &list, char delimiter, bool initialDelimiter = false);

        std::vector<std::string> Split(const std::string &string, char delimiter, bool clearSpaces = true);

		inline char const *Format(char const *string)
        {
            return string;
        }

        template<typename TYPE, typename... PARAMETERS>
        std::string Format(char const *formatting, TYPE const &value, PARAMETERS... arguments)
        {
            std::string result;
            while (formatting && *formatting)
            {
                char currentCharacter = *formatting++;
                if (currentCharacter == '%' && formatting && *formatting)
                {
                    char nextCharacter = *formatting++;
                    if (nextCharacter == '%')
                    {
                        // %%
                        result.append(1U, nextCharacter);
                    }
                    else if (nextCharacter == 'v')
                    {
                        // %v
                        return (result + std::to_string(value) + Format(formatting, arguments...));
                    }
                    else
                    {
                        // %(other)
						result.append(1U, currentCharacter);
						result.append(1U, nextCharacter);
                    }
                }
                else
                {
                    result.append(1U, currentCharacter);
                }
            };

            return result;
        }

        bool Replace(std::string &string, std::string_view const &searchFor, std::string_view const &replaceWith);

        std::wstring Widen(std::string_view const &string);
        std::string Narrow(std::wstring_view const &string);

        bool Convert(std::string const &string, bool defaultValue);
        float Convert(std::string const &string, float defaultValue);

        template <typename TYPE>
        TYPE Convert(std::string const &string, TYPE const &defaultValue)
        {
            TYPE value;
            std::stringstream stream(string);
            stream >> value;
            return (stream.fail() ? defaultValue : value);
        }
    }; // namespace String
}; // namespace Gek