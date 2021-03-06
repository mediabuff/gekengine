#include "GEK/Utility/String.hpp"
#include "GEK/Utility/JSON.hpp"
#include "GEK/Utility/FileSystem.hpp"
#include "GEK/Utility/ContextUser.hpp"
#include "GEK/System/VideoDevice.hpp"
#include "GEK/Engine/Renderer.hpp"
#include "GEK/Engine/Visual.hpp"
#include "Passes.hpp"
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        GEK_CONTEXT_USER(Visual, Video::Device *, Engine::Resources *, std::string)
            , public Plugin::Visual
        {
        private:
            Video::Device *videoDevice;
			Video::ObjectPtr inputLayout;
			Video::ObjectPtr vertexProgram;
			Video::ObjectPtr geometryProgram;

        public:
            Visual(Context *context, Video::Device *videoDevice, Engine::Resources *resources, std::string visualName)
                : ContextRegistration(context)
                , videoDevice(videoDevice)
            {
                assert(videoDevice);
                assert(resources);

                const JSON::Instance visualNode = JSON::Load(getContext()->getRootFileName("data", "visuals", visualName).withExtension(".json"));

				std::string inputVertexData;
				std::vector<Video::InputElement> elementList;

                uint32_t inputIndexList[static_cast<uint8_t>(Video::InputElement::Semantic::Count)] = { 0 };
                for (auto &baseElementNode : visualNode.get("input").getArray())
                {
                    JSON::Reference elementNode(baseElementNode);
                    std::string elementName(elementNode.get("name").convert(String::Empty));
                    std::string systemType(String::GetLower(elementNode.get("system").convert(String::Empty)));
                    if (systemType == "instanceindex")
                    {
                        inputVertexData += String::Format("    uint %v : SV_InstanceId;\r\n", elementName);
                    }
                    else if (systemType == "vertexindex")
                    {
                        inputVertexData += String::Format("    uint %v : SV_VertexId;\r\n", elementName);
                    }
                    else if (systemType == "isfrontfacing")
                    {
                        inputVertexData += String::Format("    uint %v : SV_IsFrontFace;\r\n", elementName);
                    }
                    else
                    {
                        Video::InputElement element;
                        element.format = Video::GetFormat(elementNode.get("format").convert(String::Empty));
                        element.semantic = Video::InputElement::GetSemantic(elementNode.get("semantic").convert(String::Empty));
                        element.source = Video::InputElement::GetSource(elementNode.get("source").convert(String::Empty));
                        element.sourceIndex = elementNode.get("sourceIndex").convert(0);

                        uint32_t count = elementNode.get("count").convert(1);
                        auto semanticIndex = inputIndexList[static_cast<uint8_t>(element.semantic)];
                        inputIndexList[static_cast<uint8_t>(element.semantic)] += count;

                        inputVertexData += String::Format("    %v %v : %v%v;\r\n", getFormatSemantic(element.format, count), elementName, videoDevice->getSemanticMoniker(element.semantic), semanticIndex);
                        while (count-- > 0)
                        {
                            elementList.push_back(element);
                        };
                    }
                }

				std::string outputVertexData;
				uint32_t outputIndexList[static_cast<uint8_t>(Video::InputElement::Semantic::Count)] = { 0 };
				for (auto &baseElementNode : visualNode.get("output").getArray())
				{
                    JSON::Reference elementNode(baseElementNode);
                    std::string elementName(elementNode.get("name").convert(String::Empty));
                    Video::Format format = Video::GetFormat(elementNode.get("format").convert(String::Empty));
					auto semantic = Video::InputElement::GetSemantic(elementNode.get("semantic").convert(String::Empty));
                    uint32_t count = elementNode.get("count").convert(1);
                    auto semanticIndex = outputIndexList[static_cast<uint8_t>(semantic)];
                    outputIndexList[static_cast<uint8_t>(semantic)] += count;
                    outputVertexData += String::Format("    %v %v : %v%v;\r\n", getFormatSemantic(format, count), elementName, videoDevice->getSemanticMoniker(semantic), semanticIndex);
				}

				std::string engineData;
				engineData += String::Format(
					"struct InputVertex\r\n" \
					"{\r\n" \
					"%v" \
					"};\r\n" \
					"\r\n" \
					"struct OutputVertex\r\n" \
					"{\r\n" \
					"    float4 projected : SV_POSITION;\r\n" \
					"%v" \
					"};\r\n" \
					"\r\n" \
					"OutputVertex getProjection(OutputVertex outputVertex)\r\n" \
					"{\r\n" \
					"    outputVertex.projected = mul(Camera::ProjectionMatrix, float4(outputVertex.position, 1.0));\r\n" \
					"    return outputVertex;\r\n" \
					"}\r\n", inputVertexData, outputVertexData);

                auto vertexNode = visualNode.get("vertex");
                std::string vertexEntry(vertexNode.get("entry").convert(String::Empty));
                std::string vertexProgram(vertexNode.get("program").convert(String::Empty));
                std::string vertexFileName(FileSystem::GetFileName(visualName, vertexProgram).withExtension(".hlsl").u8string());
                auto compiledVertexProgram = resources->compileProgram(Video::PipelineType::Vertex, vertexFileName, vertexEntry, engineData);
				this->vertexProgram = videoDevice->createProgram(Video::PipelineType::Vertex, compiledVertexProgram.data(), compiledVertexProgram.size());
                this->vertexProgram->setName(String::Format("%v:%v", vertexProgram, vertexEntry));
                if (!elementList.empty())
				{
					inputLayout = videoDevice->createInputLayout(elementList, compiledVertexProgram.data(), compiledVertexProgram.size());
				}

                auto geometryNode = visualNode.get("geometry");
                std::string geometryEntry(geometryNode.get("entry").convert(String::Empty));
                std::string geometryProgram(geometryNode.get("program").convert(String::Empty));
                if (!geometryEntry.empty() && !geometryProgram.empty())
                {
                    std::string geometryFileName(FileSystem::GetFileName(visualName, geometryProgram).withExtension(".hlsl").u8string());
                    auto compiledGeometryProgram = resources->compileProgram(Video::PipelineType::Geometry, geometryFileName, geometryEntry);
                    this->geometryProgram = videoDevice->createProgram(Video::PipelineType::Geometry, compiledGeometryProgram.data(), compiledGeometryProgram.size());
                    this->geometryProgram->setName(String::Format("%v:%v", geometryProgram, geometryEntry));
                }
			}

            // Plugin
            void enable(Video::Device::Context *videoContext)
            {
				videoContext->setInputLayout(inputLayout.get());
				videoContext->vertexPipeline()->setProgram(vertexProgram.get());
				videoContext->geometryPipeline()->setProgram(geometryProgram.get());
			}
        };

        GEK_REGISTER_CONTEXT_USER(Visual);
    }; // namespace Implementation
}; // namespace Gek
