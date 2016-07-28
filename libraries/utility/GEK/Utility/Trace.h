#pragma once

#include "GEK\Utility\String.h"
#include <unordered_map>
#include <future>
#include <chrono>
#include <memory>
#include <array>

namespace Gek
{
    class Exception
        : public std::exception
    {
    private:
        std::array<char, 1024> function;
        uint32_t line;

    public:
        Exception(const char *function, uint32_t line, const char *message);
        virtual ~Exception(void) = default;

        const char *in(void) const;
        uint32_t at(void) const;
    };

    namespace Trace
    {
        void initialize(void);
        void shutdown(void);

        template <class TYPE>
        struct Parameter
        {
            const char *name;
            const TYPE &value;

            Parameter(const char *name, const TYPE &value)
                : name(name)
                , value(value)
            {
            }
        };

        using ParameterMap = std::unordered_map<StringUTF8, StringUTF8>;

        void inline getParameters(ParameterMap &parameters)
        {
        }

        template<typename VALUE, typename... PARAMETERS>
        void getParameters(ParameterMap &parameters, Parameter<VALUE> &pair, PARAMETERS&... arguments)
        {
            parameters[pair.name] = pair.value;
            getParameters(parameters, arguments...);
        }

        void logBase(const char *type, const char *category, uint64_t timeStamp, const char *function, const ParameterMap &parameters);

        template<typename... PARAMETERS>
        void log(const char *type, const char *category, uint64_t timeStamp, const char *function, PARAMETERS&... arguments)
        {
            ParameterMap parameters;
            getParameters(parameters, arguments...);
            logBase(type, category, timeStamp, function, parameters);
        }

        template<typename... PARAMETERS>
        void log(const char *type, const char *category, uint64_t timeStamp, const char *function, const char *message, PARAMETERS&... arguments)
        {
            ParameterMap parameters;
            getParameters(parameters, arguments...);
            if (message)
            {
                parameters["msg"] = message;
            }

            logBase(type, category, timeStamp, function, parameters);
        }

        class Scope
        {
        private:
            const char *category;
            const char *function;
            std::chrono::time_point<std::chrono::system_clock> start;
            ParameterMap parameters;

        public:
            template<typename... PARAMETERS>
            Scope(const char *category, const char *function, PARAMETERS &... arguments)
                : category(category)
                , function(function)
                , start(std::chrono::system_clock::now())
            {
                getParameters(parameters, arguments...);
            }

            ~Scope(void)
            {
                auto duration(std::chrono::system_clock::now() - start);
                logBase("X", category, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), function, parameters);
            }
        };

        class Exception
            : public Gek::Exception
        {
        public:
            Exception(const char *function, uint32_t line, const char *message);
            virtual ~Exception(void) = default;
        };
    }; // namespace Trace

    template <typename TYPE, typename... PARAMETERS>
    std::shared_ptr<TYPE> makeShared(PARAMETERS... arguments)
    {
        std::shared_ptr<TYPE> object;
        try
        {
            object = std::make_shared<TYPE>(arguments...);
        }
        catch (const std::bad_alloc &exception)
        {
            throw Gek::Exception(__FUNCTION__, __LINE__, StringUTF8("Unable to allocate new object: %v (%v)", typeid(TYPE).name(), exception.what()));
        };

        return object;
    }

    template<typename FUNCTION, typename... PARAMETERS>
    void setPromise(std::promise<void> &promise, FUNCTION function, PARAMETERS&&... arguments)
    {
        function(std::forward<PARAMETERS>(arguments)...);
    }

    template<typename RETURN, typename FUNCTION, typename... PARAMETERS>
    void setPromise(std::promise<RETURN> &promise, FUNCTION functin, PARAMETERS&&... arguments)
    {
        promise.set_value(function(std::forward<PARAMETERS>(arguments)...));
    }

    template<typename FUNCTION, typename... PARAMETERS>
    std::future<typename std::result_of<FUNCTION(PARAMETERS...)>::type> asynchronous(FUNCTION function, PARAMETERS&&... arguments)
    {
        using ReturnValue = std::result_of<FUNCTION(PARAMETERS...)>::type;

        std::promise<ReturnValue> promise;
        std::future<ReturnValue> future = promise.get_future();
        std::thread thread([](std::promise<ReturnValue>& promise, FUNCTION function, PARAMETERS&&... arguments)
        {
            try
            {
                setPromise(promise, function, std::forward<PARAMETERS>(arguments)...);
            }
            catch (const std::exception &)
            {
                promise.set_exception(std::current_exception());
            }
        }, std::move(promise), function, std::forward<PARAMETERS>(arguments)...);
        thread.detach();
        return std::move(future);
    }
}; // namespace Gek

#define _ENABLE_TRACE_

#ifdef _ENABLE_TRACE_
    #define GEK_PARAMETER(NAME)                                 Trace::Parameter<decltype(NAME)>(#NAME, (NAME))
    #define GEK_PARAMETER_TYPE(NAME, TYPE)                      Trace::Parameter<TYPE>(#NAME, static_cast<TYPE>(NAME))
    #define GEK_TRACE_SCOPE(...)                                Trace::Scope traceScope("Scope", __FUNCTION__, __VA_ARGS__)
    #define GEK_TRACE_FUNCTION(...)                             Trace::log("I", "Function", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), __FUNCTION__, __VA_ARGS__)
    #define GEK_TRACE_EVENT(MESSAGE, ...)                       Trace::log("I", "Event", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), __FUNCTION__, MESSAGE, __VA_ARGS__)
    #define GEK_TRACE_ERROR(MESSAGE, ...)                       Trace::log("I", "Error", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), __FUNCTION__, MESSAGE, __VA_ARGS__)
    #define GEK_START_EXCEPTIONS()                              class Exception : public Gek::Trace::Exception { public: using Gek::Trace::Exception::Exception; };
#else
    #define GEK_PARAMETER(NAME)
    #define GEK_TRACE_SCOPE(...)
    #define GEK_TRACE_FUNCTION(...)
    #define GEK_TRACE_EVENT(MESSAGE, ...)
    #define GEK_TRACE_ERROR(MESSAGE, ...)
    #define GEK_START_EXCEPTIONS()                              class Exception : public Gek::Exception { public: using Gek::Exception::Exception; };
#endif

#define GEK_ADD_EXCEPTION(TYPE)                                 class TYPE : public Exception { public: using Exception::Exception; };
#define GEK_REQUIRE(CHECK)                                      do { if ((CHECK) == false) { _ASSERTE(CHECK); exit(-1); } } while (false)
#define GEK_CHECK_CONDITION(CONDITION, EXCEPTION, MESSAGE, ...) if(CONDITION) throw EXCEPTION(__FUNCTION__, __LINE__, Gek::StringUTF8(MESSAGE, __VA_ARGS__));
#define GEK_THROW_EXCEPTION(EXCEPTION, MESSAGE, ...)            throw EXCEPTION(__FUNCTION__, __LINE__, Gek::StringUTF8(MESSAGE, __VA_ARGS__));
