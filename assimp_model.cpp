/*
Author: Pulkit Goel
Class: ECE6122 (section)
Last Date Modified: 10/26/2024
Description:
Help with loading object and texture
*/


#include "common_header.h"
#include "assimp_model.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <string.h>

CVertexBufferObject CAssimpModel::vboModelData;
UINT CAssimpModel::uiVAO;
vector<CTexture> CAssimpModel::tTextures;


//Get dircetory name
std::string pathDirectory(const std::string& sFilePath) 
{
    auto pos = sFilePath.find_last_of("\\/");
    return (pos != std::string::npos) ? sFilePath.substr(0, pos + 1) : "";
}

CAssimpModel::CAssimpModel()
{
    bLoaded = false;
}

//Load assimpmodel
bool CAssimpModel::LoadModelFromFile(char* sFilePath)
{
    
    if (vboModelData.GetBufferID() == 0)
    {
        vboModelData.CreateVBO();
        tTextures.reserve(50);
    }
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(sFilePath,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    if (!scene)
    {
        return false;
    }

    const int iVertexTotalSize = sizeof(aiVector3D) * 2 + sizeof(aiVector2D);
    int iTotalVertices = 0;
    
    for (size_t i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        if (!mesh->HasTextureCoords(0)) {
            continue;
        }

        int iMeshFaces = mesh->mNumFaces;
        iMaterialIndices.push_back(mesh->mMaterialIndex);
        int iSizeBefore = vboModelData.GetCurrentSize();
        iMeshStartIndices.push_back(iSizeBefore / iVertexTotalSize);

        for (size_t j = 0; j < iMeshFaces; ++j) {
            const aiFace& face = mesh->mFaces[j];
            for (size_t k = 0; k < 3; ++k) {
                aiVector3D pos = mesh->mVertices[face.mIndices[k]];
                aiVector3D uv = mesh->mTextureCoords[0][face.mIndices[k]];
                aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[face.mIndices[k]] : aiVector3D(1.0f, 1.0f, 1.0f);

                vboModelData.AddData(&pos, sizeof(aiVector3D));
                vboModelData.AddData(&uv, sizeof(aiVector2D));  
                vboModelData.AddData(&normal, sizeof(aiVector3D));
            }
        }

        int iMeshVertices = mesh->mNumVertices;
        iTotalVertices += iMeshVertices;
        iMeshSizes.push_back((vboModelData.GetCurrentSize() - iSizeBefore) / iVertexTotalSize);
    }

    iNumMaterials = scene->mNumMaterials;
    vector<int> materialRemap(iNumMaterials);
    
    for (size_t i = 0; i < iNumMaterials; ++i) 
    {
        const aiMaterial* material = scene->mMaterials[i];
        int texIndex = 0;
        aiString path;  // filename

        if (material->GetTexture(aiTextureType_DIFFUSE, texIndex, &path) == AI_SUCCESS) 
        {
            std::string sDir = pathDirectory(sFilePath);
            std::string sTextureName = path.data;
            std::string sFullPath = sDir + sTextureName;
            int iTexFound = -1;

            for (size_t j = 0; j < tTextures.size(); ++j) 
            {
                if (sFullPath == tTextures[j].GetPath()) 
                {
                    iTexFound = j;
                    break;
                }
            }

            if (iTexFound != -1) 
            {
                materialRemap[i] = iTexFound;
            }
            else 
            {
                CTexture tNew;
                tNew.LoadTexture2D(sFullPath, true);
                materialRemap[i] = tTextures.size();
                tTextures.push_back(tNew);
            }
        }
    }

    for (size_t i = 0; i < iMeshSizes.size(); ++i) 
    {
        iMaterialIndices[i] = materialRemap[iMaterialIndices[i]];
    }

    return bLoaded = true;
}

//Upload model into VBO
void CAssimpModel::FinalizeVBO()
{
    glGenVertexArrays(1, &uiVAO);
    glBindVertexArray(uiVAO);
    vboModelData.BindVBO();
    vboModelData.UploadDataToGPU(GL_STATIC_DRAW);
    
    // Position for vertex
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(aiVector3D) + sizeof(aiVector2D), 0);
    // coordinates of textures
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(aiVector3D) + sizeof(aiVector2D), (void*)sizeof(aiVector3D));
    // Normal vectors
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(aiVector3D) + sizeof(aiVector2D), (void*)(sizeof(aiVector3D) + sizeof(aiVector2D)));
}

//Bind VAO to VBO
void CAssimpModel::BindModelsVAO()
{
    glBindVertexArray(uiVAO);
}


//Render Here
void CAssimpModel::RenderModel() {
    if (!bLoaded) return;

    //iterate it
    for (size_t i = 0; i < iMeshSizes.size(); ++i) 
    {
        int iMatIndex = iMaterialIndices[i];
        if (iMatIndex < tTextures.size()) {
            tTextures[iMatIndex].BindTexture();
        }

        //Draw the array
        glDrawArrays(GL_TRIANGLES, iMeshStartIndices[i], iMeshSizes[i]);
    }
}