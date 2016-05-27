#pragma once

#include "GEK\Utility\String.h"
#include "GEK\Context\Context.h"
#include "GEK\Context\Observable.h"
#include "GEK\Engine\Processor.h"
#include <atlbase.h>
#include <atlstr.h>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Gek
{
    GEK_PREDECLARE(Entity);
    GEK_PREDECLARE(PopulationObserver);

    GEK_INTERFACE(Population)
        : public Observable
    {
        struct ComponentDefinition
            : public std::unordered_map<wstring, wstring>
            , public wstring
        {
        };

        struct EntityDefinition
            : public std::unordered_map<wstring, ComponentDefinition>
        {
        };

        virtual float getWorldTime(void) const = 0;
        virtual float getFrameTime(void) const = 0;

        virtual void update(bool isIdle, float frameTime = 0.0f) = 0;

        virtual void load(const wchar_t *fileName) = 0;
        virtual void save(const wchar_t *fileName) = 0;
        virtual void free(void) = 0;

        virtual Entity *createEntity(const EntityDefinition &entityParameterList, const wchar_t *name = nullptr) = 0;
        virtual void killEntity(Entity *entity) = 0;
        virtual Entity *getNamedEntity(const wchar_t *name) const = 0;

        virtual void listEntities(std::function<void(Entity *)> onEntity) const = 0;

        template<typename... ARGUMENTS>
        void listEntities(std::function<void(Entity *)> onEntity) const
        {
            listEntities([onEntity](Entity *entity) -> void
            {
                if (entity->hasComponents<ARGUMENTS...>())
                {
                    onEntity(entity);
                }
            });
        }

        virtual void listProcessors(std::function<void(Processor *)> onProcessor) const = 0;

        virtual UINT32 setUpdatePriority(PopulationObserver *observer, UINT32 priority) = 0;
        virtual void removeUpdatePriority(UINT32 updateHandle) = 0;
    };

    GEK_INTERFACE(PopulationObserver)
        : public Observer
    {
        virtual void onLoadBegin(void) { };
        virtual void onLoadSucceeded(void) { };
        virtual void onLoadFailed(void) { };
        virtual void onFree(void) { };

        virtual void onEntityCreated(Entity *entity) { };
        virtual void onEntityDestroyed(Entity *entity) { };

        virtual void onUpdate(UINT32 handle, bool isIdle) { };
    };
}; // namespace Gek