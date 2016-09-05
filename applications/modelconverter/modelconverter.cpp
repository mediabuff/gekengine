#include "GEK\Math\Common.h"
#include "GEK\Math\Float3.h"
#include "GEK\Math\Float4x4.h"
#include "GEK\Shapes\AlignedBox.h"
#include "GEK\Utility\Exceptions.h"
#include "GEK\Utility\String.h"
#include "GEK\Utility\FileSystem.h"
#include "GEK\Utility\XML.h"
#include <Windows.h>
#include <experimental\filesystem>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <map>

#include <Newton.h>

#include <assimp/config.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#pragma comment(lib, "assimp.lib")

using namespace Gek;

struct Header
{
    uint32_t identifier;
    uint16_t type;
    uint16_t version;
};

struct TreeHeader : public Header
{
    struct Material
    {
        wchar_t name[64];
    };

    uint32_t materialCount;
};

struct ModelHeader : public Header
{
    struct Material
    {
        wchar_t name[64];
        uint32_t vertexCount;
        uint32_t indexCount;
    };

    Shapes::AlignedBox boundingBox;

    uint32_t materialCount;
};

struct Vertex
{
    Math::Float3 position;
    Math::Float2 texCoord;
    Math::Float3 normal;
};

struct Model
{
    std::vector<uint16_t> indexList;
    std::vector<Vertex> vertexList;
};

void getMeshes(const aiScene *scene, const aiNode *node, bool fixMaxCoords, std::unordered_map<StringUTF8, std::list<Model>> &albedoMap, Shapes::AlignedBox &boundingBox)
{
    if (node == nullptr)
    {
        throw std::exception("Invalid model node");
    }

    if (node->mNumMeshes > 0)
    {
        if (node->mMeshes == nullptr)
        {
            throw std::exception("Invalid mesh list");
        }

        for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
        {
            uint32_t nodeMeshIndex = node->mMeshes[meshIndex];
            if (nodeMeshIndex >= scene->mNumMeshes)
            {
                throw std::exception("Invalid mesh index");
            }

            const aiMesh *mesh = scene->mMeshes[nodeMeshIndex];
            if (mesh->mNumFaces > 0)
            {
                if (mesh->mFaces == nullptr)
                {
                    throw std::exception("Invalid mesh face list");
                }

                if (mesh->mVertices == nullptr)
                {
                    throw std::exception("Invalid mesh vertex list");
                }

                StringUTF8 material;
                if (scene->mMaterials != nullptr)
                {
                    const aiMaterial *sceneMaterial = scene->mMaterials[mesh->mMaterialIndex];
                    if (sceneMaterial != nullptr)
                    {
                        aiString sceneDiffuseMaterial;
                        sceneMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &sceneDiffuseMaterial);
                        if (sceneDiffuseMaterial.length > 0)
                        {
                            material = sceneDiffuseMaterial.C_Str();;
                        }
                    }
                }

                Model model;
                for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
                {
                    const aiFace &face = mesh->mFaces[faceIndex];
                    if (face.mNumIndices == 3)
                    {
                        model.indexList.push_back(face.mIndices[fixMaxCoords ? 2 : 0]);
                        model.indexList.push_back(face.mIndices[1]);
                        model.indexList.push_back(face.mIndices[fixMaxCoords ? 0 : 2]);
                    }
                    else
                    {
                        printf("(Mesh %d) Invalid Face Found: %d (%d vertices)\r\n", meshIndex, faceIndex, face.mNumIndices);
                    }
                }

                for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    Vertex vertex;
                    vertex.position.set(
                        mesh->mVertices[vertexIndex].x * (fixMaxCoords ? -1.0f : 1.0f),
                        mesh->mVertices[vertexIndex].y,
                        mesh->mVertices[vertexIndex].z);
                    boundingBox.extend(vertex.position);

                    if (mesh->mTextureCoords[0])
                    {
                        vertex.texCoord.set(
                            mesh->mTextureCoords[0][vertexIndex].x,
                            mesh->mTextureCoords[0][vertexIndex].y);
                    }

                    if (mesh->mNormals)
                    {
                        vertex.normal.set(
                            mesh->mNormals[vertexIndex].x * (fixMaxCoords ? -1.0f : 1.0f),
                            mesh->mNormals[vertexIndex].y,
                            mesh->mNormals[vertexIndex].z);
                    }

                    model.vertexList.push_back(vertex);
                }

                if (!material.empty())
                {
                    albedoMap[material].push_back(model);
                }
            }
        }
    }

    if (node->mNumChildren > 0)
    {
        if (node->mChildren == nullptr)
        {
            throw std::exception("Invalid child list");
        }

        for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
        {
            getMeshes(scene, node->mChildren[childIndex], fixMaxCoords, albedoMap, boundingBox);
        }
    }
}

void serializeCollision(void* const serializeHandle, const void* const buffer, int size)
{
    FILE *file = (FILE *)serializeHandle;
    fwrite(buffer, 1, size, file);
}

int wmain(int argumentCount, const wchar_t *argumentList[], const wchar_t *environmentVariableList)
{
    try
    {
        printf("GEK Model Converter\r\n");

        String fileNameInput;
        String fileNameOutput;
        String mode(L"model");

        bool fixMaxCoords = false;
        bool flipCoords = false;
        bool flipWinding = false;
        bool generateNormals = false;
        bool smoothNormals = false;
        float smoothingAngle = 80.0f;
        for (int argumentIndex = 1; argumentIndex < argumentCount; argumentIndex++)
        {
            String argument(argumentList[argumentIndex]);
            std::vector<String> arguments(argument.split(L':'));
            if (arguments.empty())
            {
                throw std::exception("No arguments specified for command line parameter");
            }

            if (arguments[0].compareNoCase(L"-input") == 0 && ++argumentIndex < argumentCount)
            {
                fileNameInput = argumentList[argumentIndex];
            }
            else if (arguments[0].compareNoCase(L"-output") == 0 && ++argumentIndex < argumentCount)
            {
                fileNameOutput = argumentList[argumentIndex];
            }
            else if (arguments[0].compareNoCase(L"-mode") == 0)
            {
                if (arguments.size() != 2)
                {
                    throw std::exception("Missing parameters for mode");
                }

                mode = arguments[1];
            }
            else if (arguments[0].compareNoCase(L"-flipCoords") == 0)
            {
                flipCoords = true;
            }
            else if (arguments[0].compareNoCase(L"-flipWinding") == 0)
            {
                flipWinding = true;
            }
            else if (arguments[0].compareNoCase(L"-generateNormals") == 0)
            {
                generateNormals = true;
            }
            else if (arguments[0].compareNoCase(L"-smoothNormals") == 0)
            {
                if (arguments.size() != 2)
                {
                    throw std::exception("Missing parameters for smoothNormals");
                }

                smoothNormals = true;
                smoothingAngle = arguments[1];
            }
            else if (arguments[0].compareNoCase(L"-fixMaxCoords") == 0)
            {
                fixMaxCoords = true;
            }
        }

        int notRequiredComponents =
            aiComponent_TANGENTS_AND_BITANGENTS |
            aiComponent_COLORS |
            aiComponent_BONEWEIGHTS |
            aiComponent_ANIMATIONS |
            aiComponent_LIGHTS |
            aiComponent_CAMERAS |
            0;

        unsigned int importFlags =
            (flipWinding ? aiProcess_FlipWindingOrder : 0) |
            aiProcess_RemoveComponent | // remove extra data that is not required
            aiProcess_SplitLargeMeshes | // split large, unrenderable meshes into submeshes
            aiProcess_Triangulate | // triangulate polygons with more than 3 edges
            aiProcess_SortByPType | // make �clean� meshes which consist of a single typ of primitives
            aiProcess_ValidateDataStructure | // perform a full validation of the loader�s output
            aiProcess_ImproveCacheLocality | // improve the cache locality of the output vertices
            aiProcess_RemoveRedundantMaterials | // remove redundant materials
            aiProcess_FindDegenerates | // remove degenerated polygons from the import
            aiProcess_FindInstances | // search for instanced meshes and remove them by references to one master
            aiProcess_OptimizeMeshes | // join small meshes, if possible;
            0;

        unsigned int postProcessFlags =
            aiProcess_PreTransformVertices | // pretransform the vertices by the a local node matrices
            aiProcess_JoinIdenticalVertices | // join identical vertices / optimize indexing
            aiProcess_ValidateDataStructure | // perform a full validation of the loader�s output
            aiProcess_FindInvalidData | // detect invalid model data, such as invalid normal vectors
            0;

        aiPropertyStore *propertyStore = aiCreatePropertyStore();
        aiSetImportPropertyInteger(propertyStore, AI_CONFIG_GLOB_MEASURE_TIME, 1);
        aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
        if (mode.compareNoCase(L"model") == 0)
        {
            importFlags |= aiProcess_GenUVCoords; // convert spherical, cylindrical, box and planar mapping to proper UVs
            importFlags |= aiProcess_TransformUVCoords; // preprocess UV transformations (scaling, translation �)
            if (generateNormals)
            {
                notRequiredComponents |= aiComponent_NORMALS; // remove normals component since we are generating them
                if (smoothNormals)
                {
                    aiSetImportPropertyFloat(propertyStore, AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, smoothingAngle);
                    postProcessFlags |= aiProcess_GenSmoothNormals; // generate smoothed normal vectors
                }
                else
                {
                    postProcessFlags |= aiProcess_GenNormals; // generate normal vectors
                }
            }

            aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_TER_MAKE_UVS, 1);
            if (flipCoords)
            {
                importFlags |= aiProcess_FlipUVs;
            }
        }
        else
        {
            notRequiredComponents |= aiComponent_TEXCOORDS; // we don't need texture coordinates for collision information
            notRequiredComponents |= aiComponent_NORMALS; // we don't need normals for collision information
        }

        aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_RVC_FLAGS, notRequiredComponents);
        const aiScene* scene = aiImportFileExWithProperties(StringUTF8(fileNameInput), importFlags, nullptr, propertyStore);
        if (scene == nullptr)
        {
            throw std::exception("Unable to load scene with Assimp");
        }

        aiApplyPostProcessing(scene, postProcessFlags);

        Shapes::AlignedBox boundingBox;
        std::unordered_map<StringUTF8, std::list<Model>> albedoMap;
        getMeshes(scene, scene->mRootNode, fixMaxCoords, albedoMap, boundingBox);

        aiReleasePropertyStore(propertyStore);
        aiReleaseImport(scene);

		String rootPath;
		String currentModuleName((MAX_PATH + 1), L' ');
		GetModuleFileName(nullptr, &currentModuleName.at(0), MAX_PATH);
		currentModuleName.trimRight();

		String fullModuleName((MAX_PATH + 1), L' ');
		GetFullPathName(currentModuleName, MAX_PATH, &fullModuleName.at(0), nullptr);
		fullModuleName.trimRight();

		std::experimental::filesystem::path fullModulePath(fullModuleName);
		fullModulePath.remove_filename();
		fullModulePath.remove_filename();
		rootPath = fullModulePath.append(L"Data").wstring();

		String texturesPath(FileSystem::getFileName(rootPath, L"Textures").getLower());
		String materialsPath(FileSystem::getFileName(rootPath, L"Materials").getLower());

		printf("Textures: %S\r\n", texturesPath.c_str());
		printf("Materials: %S\r\n", materialsPath.c_str());

		std::map<String, String> materialAlbedoMap;

		std::function<bool(const wchar_t *)> findMaterials;
		findMaterials = [&](const wchar_t *fileName) -> bool
		{
			if (FileSystem::isDirectory(fileName))
			{
				FileSystem::find(fileName, findMaterials);
			}
			else if (FileSystem::isFile(fileName))
			{
				try
				{
					printf("Material: %S\r\n", fileName);
					Xml::Node materialNode = Xml::load(fileName, L"material");
					materialNode.findChild(L"shader", [&](auto &shaderNode) -> void
					{
						for (auto &shaderPassNode : shaderNode.children)
						{
							shaderPassNode.findChild(L"albedo", [&](auto &albedoNode) -> void
							{
								if (albedoNode.attributes.count(L"file"))
								{
									String materialName(FileSystem::replaceExtension(fileName, L"").getLower());
									materialName.replace((materialsPath + L"\\"), L"");
									materialAlbedoMap[albedoNode.attributes[L"file"]] = materialName;
									printf("Mapping: %S: %S\r\n", albedoNode.attributes[L"file"].c_str(), materialName.c_str());
								}
							});
						}
					});
				}
				catch (...)
				{
				};
			}

			return true;
		};

		FileSystem::find(materialsPath, findMaterials);
        std::unordered_map<String, std::list<Model>> materialMultiMap;
        for (auto &albedo : albedoMap)
        {
            String albedoName(albedo.first.getLower());
            albedoName.replace(L"/", L"\\");
			albedoName = FileSystem::replaceExtension(albedoName, L"");
			albedoName.replace((texturesPath + L"\\"), L"");

			printf("FileName: %s\r\n", albedo.first.c_str());
			printf("Albedo: %S\r\n", albedoName.c_str());

			auto materialAlebedoSearch = materialAlbedoMap.find(albedoName);
			if (materialAlebedoSearch != materialAlbedoMap.end())
			{
				materialMultiMap[materialAlebedoSearch->second] = albedo.second;
				printf("Remap: %S: %S\r\n", albedoName.c_str(), materialAlebedoSearch->second.c_str());
			}
        }

		std::unordered_map<String, Model> materialMap;
		for (auto &multiMaterial : materialMultiMap)
		{
			Model &material = materialMap[multiMaterial.first];
			for (auto &instance : multiMaterial.second)
			{
				for (auto &index : instance.indexList)
				{
					material.indexList.push_back(uint16_t(index + material.vertexList.size()));
				}

				material.vertexList.insert(material.vertexList.end(), instance.vertexList.begin(), instance.vertexList.end());
			}
		}

		if (materialMap.empty())
		{
			throw std::exception("No valid material models found");
		}

		printf("> Num. Models: %d\r\n", materialMap.size());
        printf("< Size: Min(%f, %f, %f)\r\n", boundingBox.minimum.x, boundingBox.minimum.y, boundingBox.minimum.z);
        printf("        Max(%f, %f, %f)\r\n", boundingBox.maximum.x, boundingBox.maximum.y, boundingBox.maximum.z);
        if (mode.compareNoCase(L"model") == 0)
        {
            FILE *file = nullptr;
            _wfopen_s(&file, fileNameOutput, L"wb");
            if (file == nullptr)
            {
                throw std::exception("Unable to create output file");
            }

            ModelHeader header;
            header.identifier = *(uint32_t *)"GEKX";
            header.type = 0;
            header.version = 4;
            header.materialCount = materialMap.size();
            fwrite(&header, sizeof(ModelHeader), 1, file);
            for (auto &material : materialMap)
            {
                printf("-  %S\r\n", material.first.c_str());
                printf("    %d vertices\r\n", material.second.vertexList.size());
                printf("    %d indices\r\n", material.second.indexList.size());

                ModelHeader::Material materialHeader;
                wcsncpy(materialHeader.name, material.first, 63);
                materialHeader.vertexCount = material.second.vertexList.size();
                materialHeader.indexCount = material.second.indexList.size();
                fwrite(&materialHeader, sizeof(ModelHeader::Material), 1, file);
            }

            for (auto &material : materialMap)
            {
                fwrite(material.second.indexList.data(), sizeof(uint16_t), material.second.indexList.size(), file);
                fwrite(material.second.vertexList.data(), sizeof(Vertex), material.second.vertexList.size(), file);
            }

            fclose(file);
        }
        else  if (mode.compareNoCase(L"hull") == 0)
        {
            std::vector<Math::Float3> pointCloudList;
            for (auto &material : materialMap)
            {
                for (auto &index : material.second.indexList)
                {
                    pointCloudList.push_back(material.second.vertexList[index].position);
                }
            }

            printf("> Num. Points: %d\r\n", pointCloudList.size());

            NewtonWorld *newtonWorld = NewtonCreate();
            NewtonCollision *newtonCollision = NewtonCreateConvexHull(newtonWorld, pointCloudList.size(), pointCloudList[0].data, sizeof(Math::Float3), 0.025f, 0, Math::Float4x4().data);
            if (newtonCollision == nullptr)
            {
                throw std::exception("Unable to create convex hull collision object");
            }

            FILE *file = nullptr;
            _wfopen_s(&file, fileNameOutput, L"wb");
            if (file == nullptr)
            {
                throw std::exception("Unable to create output file");
            }

            Header header;
            header.identifier = *(uint32_t *)"GEKX";
            header.type = 1;
            header.version = 1;
            fwrite(&header, sizeof(Header), 1, file);
            NewtonCollisionSerialize(newtonWorld, newtonCollision, serializeCollision, file);
            fclose(file);

            NewtonDestroyCollision(newtonCollision);
            NewtonDestroy(newtonWorld);
        }
        else  if (mode.compareNoCase(L"tree") == 0)
        {
            NewtonWorld *newtonWorld = NewtonCreate();
            NewtonCollision *newtonCollision = NewtonCreateTreeCollision(newtonWorld, 0);
            if (newtonCollision == nullptr)
            {
                throw std::exception("Unable to create tree collision object");
            }

            int materialIdentifier = 0;
            NewtonTreeCollisionBeginBuild(newtonCollision);
            for (auto &material : materialMap)
            {
                printf("-  %S\r\n", material.first.c_str());
                printf("    %d vertices\r\n", material.second.vertexList.size());
                printf("    %d indices\r\n", material.second.indexList.size());

                auto &indexList = material.second.indexList;
                auto &vertexList = material.second.vertexList;
                for(uint32_t index = 0; index < indexList.size(); index += 3)
                {
                    Math::Float3 faceList[3] = 
                    {
                        vertexList[indexList[index + 0]].position,
                        vertexList[indexList[index + 1]].position,
                        vertexList[indexList[index + 2]].position,
                    };

                    NewtonTreeCollisionAddFace(newtonCollision, 3, faceList[0].data, sizeof(Math::Float3), materialIdentifier);
                }

                ++materialIdentifier;
            }

            NewtonTreeCollisionEndBuild(newtonCollision, 1);

            FILE *file = nullptr;
            _wfopen_s(&file, fileNameOutput, L"wb");
            if (file == nullptr)
            {
                throw std::exception("Unable to create output file");
            }

            TreeHeader header;
            header.identifier = *(uint32_t *)"GEKX";
            header.type = 2;
            header.version = 1;
            header.materialCount = materialMap.size();
            fwrite(&header, sizeof(TreeHeader), 1, file);
            for (auto &material : materialMap)
            {
                TreeHeader::Material materialHeader;
                wcsncpy(materialHeader.name, material.first, 63);
                fwrite(&materialHeader, sizeof(TreeHeader::Material), 1, file);
            }

            NewtonCollisionSerialize(newtonWorld, newtonCollision, serializeCollision, file);
            fclose(file);

            NewtonDestroyCollision(newtonCollision);
            NewtonDestroy(newtonWorld);
        }
        else
        {
            throw std::exception("Invalid conversion mode specified");
        }
    }
    catch (const std::exception &exception)
    {
        printf("[error] Exception occurred: %s", exception.what());
    }
    catch (...)
    {
        printf("[error] Unhandled Exception Occurred!");
    }

    printf("\r\n");
    return 0;
}