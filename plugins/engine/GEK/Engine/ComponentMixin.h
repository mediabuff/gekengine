#pragma once

#include "GEK\Utility\Evaluator.h"
#include "GEK\Engine\Population.h"
#include "GEK\Engine\Component.h"

namespace Gek
{
    template <typename TYPE>
    void saveParameter(Population::ComponentDefinition &componentData, const wchar_t *name, const TYPE &value)
    {
        if (name)
        {
            componentData[name] = String::from<wchar_t>(value);
        }
        else
        {
            componentData.SetString(String::from<wchar_t>(value));
        }
    }

    template <typename TYPE>
    bool loadParameter(const Population::ComponentDefinition &componentData, const wchar_t *name, TYPE &value)
    {
        if (name)
        {
            auto iterator = componentData.find(name);
            if (iterator != componentData.end())
            {
                value = Evaluator::get<TYPE>((*iterator).second);
                //value = String::to<TYPE>((*iterator).second);
                return true;
            }
        }
        else if(!componentData.IsEmpty())
        {
            value = Evaluator::get<TYPE>(componentData.GetString());
        }

        return false;
    }

    template <class DATA>
    class ComponentMixin
        : public Component
    {
    public:
        ComponentMixin(void)
        {
        }

        virtual ~ComponentMixin(void)
        {
        }

        // Component
        std::type_index getIdentifier(void) const
        {
            return typeid(DATA);
        }

        void *create(const Population::ComponentDefinition &componentData)
        {
            DATA *data = new DATA();
            if (data)
            {
                data->load(componentData);
            }

            return data;
        }

        void destroy(void *data)
        {
            delete static_cast<DATA *>(data);
        }
    };
}; // namespace Gek