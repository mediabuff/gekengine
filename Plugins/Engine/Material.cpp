﻿#include "GEK/Utility/String.hpp"
#include "GEK/Utility/FileSystem.hpp"
#include "GEK/Utility/JSON.hpp"
#include "GEK/Utility/ContextUser.hpp"
#include "GEK/System/VideoDevice.hpp"
#include "GEK/Engine/Shader.hpp"
#include "GEK/Engine/Resources.hpp"
#include "GEK/Engine/Renderer.hpp"
#include "GEK/Engine/Material.hpp"
#include "Passes.hpp"
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        GEK_CONTEXT_USER(Material, Engine::Resources *, std::string, MaterialHandle)
            , public Engine::Material
        {
        private:
            Engine::Resources *resources = nullptr;
            std::unordered_map<size_t, Data> dataMap;
            RenderStateHandle renderState;


        public:
            Material(Context *context, Engine::Resources *resources, std::string materialName, MaterialHandle materialHandle)
                : ContextRegistration(context)
                , resources(resources)
            {
                assert(resources);

                JSON::Instance materialNode = JSON::Load(getContext()->getRootFileName("data", "materials", materialName).withExtension(".json"));
                auto &shaderNode = materialNode.get("shader");
                auto shaderName = shaderNode.get("default").convert(String::Empty);
                ShaderHandle shaderHandle = resources->getShader(shaderName, materialHandle);
                Engine::Shader *shader = resources->getShader(shaderHandle);
                if (shader)
                {
                    Video::RenderState::Description renderStateInformation;
                    renderStateInformation.load(shaderNode.get("renderState"));
                    renderState = resources->createRenderState(renderStateInformation);

                    auto &dataNode = shaderNode.get("data");
                    for (auto material = shader->begin(); material; material = material->next())
                    {
                        auto materialName = material->getName();
                        auto &data = dataMap[GetHash(materialName)];
                        for (auto &initializer : material->getInitializerList())
                        {
                            ResourceHandle resourceHandle;
                            auto &resourceNode = dataNode.get(initializer.name);
                            if (resourceNode.has("file"))
                            {
                                auto fileName = resourceNode.get("file").convert(String::Empty);
                                uint32_t flags = getTextureLoadFlags(resourceNode.get("flags").convert(String::Empty));
                                resourceHandle = resources->loadTexture(fileName, flags);
                            }
                            else if (resourceNode.has("source"))
                            {
                                resourceHandle = resources->getResourceHandle(resourceNode.get("source").convert(String::Empty));
                            }

                            if (!resourceHandle)
                            {
                                resourceHandle = initializer.fallback;
                            }

                            data.resourceList.push_back(resourceHandle);
                        }
                    }
                }
                else
                {
                    LockedWrite{ std::cerr } << String::Format("Shader %v missing for material %v", shaderName, materialName);
                }
            }

            // Material
            Data const *getData(size_t dataHash)
            {
                auto dataSearch = dataMap.find(dataHash);
                if (dataSearch != std::end(dataMap))
                {
                    return &dataSearch->second;
                }

                return nullptr;
            }

            RenderStateHandle getRenderState(void)
            {
                return renderState;
            }
        };

        GEK_REGISTER_CONTEXT_USER(Material);
    }; // namespace Implementation
}; // namespace Gek
