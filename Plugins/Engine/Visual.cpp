#include "GEK\Utility\String.hpp"
#include "GEK\Utility\JSON.hpp"
#include "GEK\Utility\FileSystem.hpp"
#include "GEK\Utility\ContextUser.hpp"
#include "GEK\System\VideoDevice.hpp"
#include "GEK\Engine\Renderer.hpp"
#include "GEK\Engine\Visual.hpp"
#include "Passes.hpp"
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        GEK_CONTEXT_USER(Visual, Video::Device *, Engine::Resources *, String)
            , public Plugin::Visual
        {
        private:
            Video::Device *videoDevice;
			Video::ObjectPtr inputLayout;
			Video::ObjectPtr vertexProgram;
			Video::ObjectPtr geometryProgram;

        public:
            Visual(Context *context, Video::Device *videoDevice, Engine::Resources *resources, String visualName)
                : ContextRegistration(context)
                , videoDevice(videoDevice)
            {
                GEK_REQUIRE(videoDevice);
                GEK_REQUIRE(resources);

                const JSON::Object visualNode = JSON::Load(getContext()->getFileName(L"data\\visuals", visualName).append(L".json"));

				String inputVertexData;
				std::vector<Video::InputElement> elementList;
                auto inputNode = visualNode[L"input"];
                if (inputNode.is_array())
				{
					uint32_t semanticIndexList[static_cast<uint8_t>(Video::InputElement::Semantic::Count)] = { 0 };
					for (auto &elementNode : inputNode.elements())
					{
                        String elementName(elementNode[L"name"].as_string());
						if (elementNode.has_member(L"system"))
						{
                            String system(elementNode[L"system"].as_string());
							if (system.compareNoCase(L"InstanceID") == 0)
							{
								inputVertexData.format(L"    int %v : SV_InstanceId;\r\n", elementName);
							}
							else if (system.compareNoCase(L"VertexID") == 0)
							{
								inputVertexData.format(L"    int %v : SV_VertexId;\r\n", elementName);
							}
							else if (system.compareNoCase(L"isFrontFacing") == 0)
							{
								String format(elementNode.get(L"format", L"bool").as_string());
								if (format.compareNoCase(L"int") == 0)
								{
									inputVertexData.format(L"    int %v : SV_IsFrontFace;\r\n", elementName);
								}
                                else if (format.compareNoCase(L"uint") == 0)
                                {
                                    inputVertexData.format(L"    uint %v : SV_IsFrontFace;\r\n", elementName);
                                }
                                else if (format.compareNoCase(L"bool") == 0)
                                {
                                    inputVertexData.format(L"    bool %v : SV_IsFrontFace;\r\n", elementName);
                                }
                                else
                                {
                                    throw InvalidElementType("Invalid system format encountered");
                                }
                            }
						}
						else
						{
							Video::InputElement element;
                            String bindType(elementNode[L"bind"].as_string());
							element.format = getBindFormat(getBindType(bindType));
							if (element.format == Video::Format::Unknown)
							{
								throw InvalidElementType("Invalid vertex data format encountered");
							}

							element.semantic = getElementSemantic(elementNode[L"semantic"].as_string());
							element.source = getElementSource(elementNode[L"source"].as_string());
                            element.sourceIndex = elementNode.get(L"sourceIndex", 0).as_uint();

							auto semanticIndex = semanticIndexList[static_cast<uint8_t>(element.semantic)]++;
							inputVertexData.format(L"    %v %v : %v%v;\r\n", bindType, elementName, videoDevice->getSemanticMoniker(element.semantic), semanticIndex);
							elementList.push_back(element);
						}
					}
				}

				String outputVertexData;
                auto outputNode = visualNode[L"output"];
                if (outputNode.is_array())
				{
					uint32_t semanticIndexList[static_cast<uint8_t>(Video::InputElement::Semantic::Count)] = { 0 };
					for (auto &elementNode : outputNode.elements())
					{
                        String elementName(elementNode[L"name"].as_string());
                        String bindType(elementNode[L"bind"].as_string());
						auto bindFormat = getBindFormat(getBindType(bindType));
						if (bindFormat == Video::Format::Unknown)
						{
                            throw InvalidElementType("Invalid vertex data format encountered");
                        }

						auto semantic = getElementSemantic(elementNode[L"semantic"].as_string());
						auto semanticIndex = semanticIndexList[static_cast<uint8_t>(semantic)]++;
						outputVertexData.format(L"    %v %v : %v%v;\r\n", bindType, elementName, videoDevice->getSemanticMoniker(semantic), semanticIndex);
					}
				}

                auto vertexNode = visualNode[L"vertex"];
                if (vertexNode.is_object())
                {
					String engineData;
					engineData.format(
						L"struct InputVertex\r\n" \
						L"{\r\n" \
						L"%v" \
						L"};\r\n" \
						L"\r\n" \
						L"struct OutputVertex\r\n" \
						L"{\r\n" \
						L"    float4 projected : SV_POSITION;\r\n" \
						L"%v" \
						L"};\r\n" \
						L"\r\n" \
						L"OutputVertex getProjection(OutputVertex outputVertex)\r\n" \
						L"{\r\n" \
						L"    outputVertex.projected = mul(Camera::projectionMatrix, float4(outputVertex.position, 1.0));\r\n" \
						L"    return outputVertex;\r\n" \
						L"}\r\n", inputVertexData, outputVertexData);

                    String entryFunction(vertexNode[L"entry"].as_string());
                    String name(FileSystem::GetFileName(visualName, vertexNode[L"program"].as_string()).append(L".hlsl"));
                    auto compiledProgram = resources->compileProgram(Video::PipelineType::Vertex, name, entryFunction, engineData);
					vertexProgram = videoDevice->createProgram(Video::PipelineType::Vertex, compiledProgram.data(), compiledProgram.size());
                    vertexProgram->setName(String::Format(L"%v:%v", name, entryFunction));
                    if (!elementList.empty())
					{
						inputLayout = videoDevice->createInputLayout(elementList, compiledProgram.data(), compiledProgram.size());
					}
				}
                else
				{
					throw MissingParameter("Visual vertex data must be an object");
				}

                auto geometryNode = visualNode.get(L"geometry");
                if (geometryNode.is_object())
				{
                    String entryFunction(geometryNode[L"entry"].as_string());
                    String name(FileSystem::GetFileName(visualName, geometryNode[L"program"].as_string()).append(L".hlsl"));
                    auto compiledProgram = resources->compileProgram(Video::PipelineType::Geometry, name, entryFunction);
                    geometryProgram = videoDevice->createProgram(Video::PipelineType::Geometry, compiledProgram.data(), compiledProgram.size());
                    geometryProgram->setName(String::Format(L"%v:%v", name, entryFunction));
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