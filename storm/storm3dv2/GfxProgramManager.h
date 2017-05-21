#pragma once

#include "GfxDevice.h"

bool createShader(
    IDirect3DVertexShader9** shader,
    gfx::Device& device,
    const char* source_name,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
);

bool createShader(
    IDirect3DPixelShader9** shader,
    gfx::Device& device,
    const char* source_name,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
);

namespace gfx
{
    struct ProgramManager
    {
        enum : uint16_t
        {
            SSF_DEFAULT = 0,
            SSF_2D_POS = (1 << 0),
            SSF_COLOR = (1 << 1),
            SSF_TEXTURE = (1 << 2),
            SSF_PROGRAM_COUNT = 8,
            PROCEDURAL = SSF_PROGRAM_COUNT,
            PROCEDURAL_DISTORTION,
            TERRAIN_TEXTURES_BLEND,
            TERRAIN_LIGHTING,
            TERRAIN_PROJECTION_FLAT_SHADOW,
            TERRAIN_PROJECTION_POINT_SHADOW,
            TERRAIN_PROJECTION_DIRECT_SHADOW,
            TERRAIN_PROJECTION_FLAT_NOSHADOW,
            TERRAIN_PROJECTION_POINT_NOSHADOW,
            TERRAIN_PROJECTION_DIRECT_NOSHADOW,
            DECAL_SHADOW,
            DECAL_LIGHTING,
            MESH_STATIC_PROJECTION_FLAT_SHADOW,
            MESH_STATIC_PROJECTION_POINT_SHADOW,
            MESH_STATIC_PROJECTION_DIRECT_SHADOW,
            MESH_STATIC_PROJECTION_FLAT_NOSHADOW,
            MESH_STATIC_PROJECTION_POINT_NOSHADOW,
            MESH_STATIC_PROJECTION_DIRECT_NOSHADOW,
            CONE_TEXTURE,
            CONE_NOTEXTURE,
            PROGRAM_COUNT
        };

        bool init(gfx::Device& device);
        void fini();

        void update(gfx::Device& device);

        void resetUniforms();

        void setWorldMatrix(const D3DXMATRIX& world);
        void setViewMatrix(const D3DXMATRIX& view);
        void setProjectionMatrix(const D3DXMATRIX& proj);

        //TODO: obsolete, remove!!!
        void setStdProgram(gfx::Device& device, uint16_t id); //setProgram
        //TODO: obsolete, remove!!!
        void commitConstants(gfx::Device& device); //applyToDevice

        void setProgram(uint16_t id);
        void applyState(gfx::Device& device);

        void setTextureMatrix(uint32_t index, const D3DXMATRIX& matrix);

        void setAmbient(const COL& color);
        void setTextureOffset(const VC2& offset);
        void setDiffuse(const COL& color, float alpha = 1.0f);
        void setFog(float start, float end);
        void setFogColor(const COL& color);
        void setLightmapColor(const COL& color);
        void setDirectLight(const VC3& dir, float strength);
        void setPointLight(const VC3& pos, float range);
        void setBias(float newBias);

    private:
        enum
        {
            VS_OFFSET_SCALE_2UV = SSF_PROGRAM_COUNT,
            VS_TERRAIN_BLEND,
            VS_TERRAIN_LIGHTING,
            VS_TERRAIN_PROJECTION_FLAT,
            VS_TERRAIN_PROJECTION_POINT,
            VS_TERRAIN_PROJECTION_DIRECT,
            VS_DECAL_LIGHTING,
            VS_STATIC_MESH_PROJECTION_FLAT,
            VS_STATIC_MESH_PROJECTION_POINT,
            VS_STATIC_MESH_PROJECTION_DIRECT,
            VS_CONE,
            VS_SHADER_COUNT
        };

        enum
        {
            PS_PROCEDURAL = SSF_PROGRAM_COUNT,
            PS_PROCEDURAL_DISTORSION,
            PS_TERRAIN_BLEND,
            PS_TERRAIN_LIGHTING,
            PS_SHADOW,
            PS_NOSHADOW,
            PS_DECAL_SHADOW,
            PS_DECAL_LIGHTING,
            PS_CONE_TEXTURE,
            PS_CONE_NOTEXTURE,
            PS_SHADER_COUNT
        };

        enum
        {
            MAX_TEXTURE_MATRICES = 4,
        };

        LPDIRECT3DVERTEXSHADER9 vertexShaders[VS_SHADER_COUNT] = { 0 };
        LPDIRECT3DPIXELSHADER9  pixelShaders[PS_SHADER_COUNT] = { 0 };

        const struct { uint16_t vs, ps; } programs[PROGRAM_COUNT] =
        {
            { 0, 0 },
            { 1, 1 },
            { 2, 2 },
            { 3, 3 },
            { 4, 4 },
            { 5, 5 },
            { 6, 6 },
            { 7, 7 },
            { VS_OFFSET_SCALE_2UV, PS_PROCEDURAL },
            { VS_OFFSET_SCALE_2UV, PS_PROCEDURAL_DISTORSION },
            { VS_TERRAIN_BLEND, PS_TERRAIN_BLEND },
            { VS_TERRAIN_LIGHTING, PS_TERRAIN_LIGHTING },
            { VS_TERRAIN_PROJECTION_FLAT, PS_SHADOW },
            { VS_TERRAIN_PROJECTION_POINT, PS_SHADOW },
            { VS_TERRAIN_PROJECTION_DIRECT, PS_SHADOW },
            { VS_TERRAIN_PROJECTION_FLAT, PS_NOSHADOW },
            { VS_TERRAIN_PROJECTION_POINT, PS_NOSHADOW },
            { VS_TERRAIN_PROJECTION_DIRECT, PS_NOSHADOW },
            { SSF_COLOR | SSF_TEXTURE, PS_DECAL_SHADOW },
            { VS_DECAL_LIGHTING, PS_DECAL_LIGHTING },
            { VS_STATIC_MESH_PROJECTION_FLAT, PS_SHADOW },
            { VS_STATIC_MESH_PROJECTION_POINT, PS_SHADOW },
            { VS_STATIC_MESH_PROJECTION_DIRECT, PS_SHADOW },
            { VS_STATIC_MESH_PROJECTION_FLAT, PS_NOSHADOW },
            { VS_STATIC_MESH_PROJECTION_POINT, PS_NOSHADOW },
            { VS_STATIC_MESH_PROJECTION_DIRECT, PS_NOSHADOW },
            { VS_CONE, PS_CONE_TEXTURE },
            { VS_CONE, PS_CONE_NOTEXTURE },
        };

        uint16_t activeProgram = 0;

        float bias = 0.0f;

        D3DXVECTOR4 textureOffset;

        D3DXVECTOR4 diffuse;
        D3DXVECTOR4 ambient;
        D3DXVECTOR4 fogParams;
        D3DXVECTOR4 lightSourceParams;

        D3DXMATRIX worldMatrix;
        D3DXMATRIX viewMatrix;
        D3DXMATRIX projMatrix;
        D3DXMATRIX textureMatrices[MAX_TEXTURE_MATRICES];

        D3DXVECTOR4 fogColor;
        D3DXVECTOR4 lightmapColor;
    };
}