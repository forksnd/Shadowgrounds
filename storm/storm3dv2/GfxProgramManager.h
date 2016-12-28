#pragma once

#include "GfxDevice.h"

bool createShader(
    IDirect3DVertexShader9** shader,
    gfx::Device& device,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
);

bool createShader(
    IDirect3DPixelShader9** shader,
    gfx::Device& device,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
);

bool compileShaderSet(
    gfx::Device& device,
    size_t path_len, const char* path,
    size_t define_count, const char** defines,
    size_t shader_count, IDirect3DVertexShader9** shader_set
);

bool compileShaderSet(
    gfx::Device& device,
    size_t path_len, const char* path,
    size_t define_count, const char** defines,
    size_t shader_count, IDirect3DPixelShader9** shader_set
);

template <typename IShaderType, size_t path_len, size_t define_count, size_t shader_count>
bool compileShaderSet(
    gfx::Device& device,
    const char(&path)[path_len],
    const char* (&defines)[define_count],
    IShaderType(&shader_set)[shader_count]
)
{
    return compileShaderSet(
        device,
        path_len, path,
        define_count, defines,
        shader_count, shader_set
    );
}

namespace gfx
{
    struct ProgramManager
    {
        enum
        {
            SSF_2D_POS = (1 << 0),
            SSF_COLOR = (1 << 1),
            SSF_TEXTURE = (1 << 2),

            SSF_SHADER_COUNT = 8,
            SHADER_COUNT = SSF_SHADER_COUNT
        };

        bool init(gfx::Device& device);
        void fini();
        
        void resetUniforms();

        void setWorldMatrix(const D3DXMATRIX& world);
        void setViewMatrix(const D3DXMATRIX& view);
        void setProjectionMatrix(const D3DXMATRIX& proj);
        
        void setStdProgram(gfx::Device& device, size_t id);
        void commitConstants(gfx::Device& device);

    private:
        LPDIRECT3DVERTEXSHADER9 vertexShaders[SHADER_COUNT];
        LPDIRECT3DPIXELSHADER9  pixelShaders[SHADER_COUNT];

        D3DXMATRIX worldMatrix;
        D3DXMATRIX viewMatrix;
        D3DXMATRIX projMatrix;
    };
}