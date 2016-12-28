#include "GfxProgramManager.h"
//TODO: remove
#include <string>
//TODO: remove
#include "storm3d_terrain_utils.h"

enum ShaderProfile
{
    VERTEX_SHADER_3_0,
    PIXEL_SHADER_3_0,
    SHADER_PROFILE_COUNT
};

ID3DBlob* compileShader(ShaderProfile profile, size_t source_len, const char* source, D3D_SHADER_MACRO* defines)
{
    assert(profile < SHADER_PROFILE_COUNT);

    const char* profile_id_string[SHADER_PROFILE_COUNT] = {
        "vs_3_0",
        "ps_3_0"
    };

    ID3DBlob*  code = 0;
    ID3DBlob*  msgs = 0;

    HRESULT hr = D3DCompile(
        source, source_len,
        NULL, defines, NULL,
        "main", profile_id_string[profile],
#ifdef _DEBUG
        D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_DEBUG,
#else
        D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3,
#endif
        0, &code, &msgs
    );

    if (FAILED(hr))
    {
        char* report = (char *)msgs->GetBufferPointer();
        OutputDebugString(report);
        MessageBox(NULL, "D3DCompile failed!", "Error!!!", MB_OK);
    }

    if (msgs) msgs->Release();

    return code;
}

bool createShader(
    IDirect3DVertexShader9** shader,
    gfx::Device& device,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
)
{
    ID3DBlob* vscode = compileShader(
        VERTEX_SHADER_3_0, source_len, source, defines
    );

    if (!vscode) return false;

    HRESULT hr = device.CreateVertexShader((const DWORD*)vscode->GetBufferPointer(), shader);

    return SUCCEEDED(hr);
}

bool createShader(
    IDirect3DPixelShader9** shader,
    gfx::Device& device,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
)
{
    ID3DBlob* pscode = compileShader(
        PIXEL_SHADER_3_0, source_len, source, defines
    );

    if (!pscode) return false;

    HRESULT hr = device.CreatePixelShader((const DWORD*)pscode->GetBufferPointer(), shader);

    return SUCCEEDED(hr);
}


template <typename IShaderType>
bool compileShaderSet(
    gfx::Device& device,
    size_t path_len, const char* path,
    size_t define_count, const char** defines,
    size_t shader_count, IShaderType** shader_set
)
{
    assert(define_count<32);

    const size_t generated_shader_count = 1 << define_count;
    assert(shader_count >= generated_shader_count);

    bool success = true;

    std::string shader_source;
    frozenbyte::storm::readFile(shader_source, path);

    //TODO: replace with safer variant
    D3D_SHADER_MACRO* macros = (D3D_SHADER_MACRO*)alloca(sizeof(D3D_SHADER_MACRO)*(define_count + 1));
    for (size_t i = 0; i<generated_shader_count; ++i)
    {
        size_t active_define_count = 0;

        for (size_t j = 0; j<define_count; ++j)
        {
            const size_t bit = 1 << j;
            if (i&bit)
            {
                macros[active_define_count].Name = defines[j];
                macros[active_define_count].Definition = "";
                ++active_define_count;
            }
        }

        macros[active_define_count].Definition = 0;
        macros[active_define_count].Name = 0;

        shader_set[i] = 0;

        success &= createShader(
            &shader_set[i],
            device,
            shader_source.length(),
            shader_source.c_str(),
            macros
        );
    }

    return success;
}

bool compileShaderSet(
    gfx::Device& device,
    size_t path_len, const char* path,
    size_t define_count, const char** defines,
    size_t shader_count, IDirect3DVertexShader9** shader_set
)
{
    return compileShaderSet<IDirect3DVertexShader9>(
        device,
        path_len, path,
        define_count, defines,
        shader_count, shader_set
        );
}

bool compileShaderSet(
    gfx::Device& device,
    size_t path_len, const char* path,
    size_t define_count, const char** defines,
    size_t shader_count, IDirect3DPixelShader9** shader_set
)
{
    return compileShaderSet<IDirect3DPixelShader9>(
        device,
        path_len, path,
        define_count, defines,
        shader_count, shader_set
        );
}

namespace gfx
{
    bool ProgramManager::init(gfx::Device& device)
    {
        const char* defines[] = {
            "VS_2D_POS",
            "ENABLE_COLOR",
            "ENABLE_TEXTURE"
        };

        compileShaderSet(device, "Data\\shaders\\std.vs", defines, vertexShaders);
        compileShaderSet(device, "Data\\shaders\\std.ps", defines, pixelShaders);

        return true;
    }

    void ProgramManager::fini()
    {
        for (size_t i = 0; i < SHADER_COUNT; ++i)
        {
            if (vertexShaders[i]) vertexShaders[i]->Release();
            if (pixelShaders[i]) pixelShaders[i]->Release();
        }
    }

    void ProgramManager::resetUniforms()
    {
        D3DXMatrixIdentity(&worldMatrix);
        D3DXMatrixIdentity(&viewMatrix);
        D3DXMatrixIdentity(&projMatrix);
    }

    void ProgramManager::setWorldMatrix(const D3DXMATRIX& world)
    {
        worldMatrix = world;
    }

    void ProgramManager::setViewMatrix(const D3DXMATRIX& view)
    {
        viewMatrix = view;
    }

    void ProgramManager::setProjectionMatrix(const D3DXMATRIX& proj)
    {
        projMatrix = proj;
    }

    void ProgramManager::setStdProgram(gfx::Device& device, size_t id)
    {
        assert(id < SHADER_COUNT);

        device.SetVertexShader(vertexShaders[id]);
        device.SetPixelShader(pixelShaders[id]);
    }

    void ProgramManager::commitConstants(gfx::Device& device)
    {
        D3DXMATRIX MVP;

        D3DXMatrixMultiply(&MVP, &viewMatrix, &projMatrix);
        D3DXMatrixMultiply(&MVP, &worldMatrix, &MVP);
        D3DXMatrixTranspose(&MVP, &MVP);

        device.SetVertexShaderConstantF(0, MVP, 4);
    }
}