#include "GfxProgramManager.h"
#include <memory>

enum ShaderProfile
{
    VERTEX_SHADER_3_0,
    PIXEL_SHADER_3_0,
    SHADER_PROFILE_COUNT
};

typedef std::unique_ptr<char[]> char_array_t;

char_array_t readNewerFile(const char* fileName, uint32_t& size, uint64_t& timestamp)
{
    union { FILETIME ft; uint64_t ts; } writeTime;
    HANDLE hFile;
    DWORD upperSize, bytesRead;
    char* buffer = nullptr;

    hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    GetFileTime(hFile, nullptr, nullptr, &writeTime.ft);
    size = GetFileSize(hFile, &upperSize);

    if ((!timestamp || writeTime.ts > timestamp) && !upperSize)
    {
        buffer = new char[size];
        ReadFile(hFile, buffer, size, &bytesRead, nullptr);
        timestamp = writeTime.ts;
    }

    CloseHandle(hFile);

    return char_array_t(buffer);
}

ID3DBlob* compileShader(ShaderProfile profile, const char* source_name, size_t source_len, const char* source, D3D_SHADER_MACRO* defines)
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
        source_name, defines, NULL,
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
        OutputDebugString("\n--------------------------\nShader compilation failed!\n");
        OutputDebugString(report);
        OutputDebugString("--------------------------\n\n");
        MessageBox(NULL, "D3DCompile failed!", "Error!!!", MB_OK);
    }

    if (msgs) msgs->Release();

    return code;
}

bool createShader(
    IDirect3DVertexShader9** shader,
    gfx::Device& device,
    const char* source_name,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
)
{
    ID3DBlob* vscode = compileShader(
        VERTEX_SHADER_3_0, source_name, source_len, source, defines
    );

    if (!vscode) return false;

    IDirect3DVertexShader9* shader_copy;
    HRESULT hr = device.CreateVertexShader((const DWORD*)vscode->GetBufferPointer(), &shader_copy);

    if (SUCCEEDED(hr))
    {
        if (*shader)
        {
            (*shader)->Release();
        }
        *shader = shader_copy;
        return true;
    }

    return false;
}

bool createShader(
    IDirect3DPixelShader9** shader,
    gfx::Device& device,
    const char* source_name,
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
)
{
    ID3DBlob* pscode = compileShader(
        PIXEL_SHADER_3_0, source_name, source_len, source, defines
    );

    if (!pscode) return false;

    IDirect3DPixelShader9* shader_copy;
    HRESULT hr = device.CreatePixelShader((const DWORD*)pscode->GetBufferPointer(), &shader_copy);

    if (SUCCEEDED(hr))
    {
        if (*shader)
        {
            (*shader)->Release();
        }
        *shader = shader_copy;
        return true;
    }

    return false;
}

namespace gfx
{
    D3D_SHADER_MACRO defines[] =
    {
        // 0, 1
        { nullptr, nullptr },
        // 1, 2
        { "VS_2D_POS", "" },
        { nullptr, nullptr },
        // 3, 3
        { "VS_2D_POS", "" },
        { "ENABLE_COLOR", "" },
        { nullptr, nullptr },
        // 6, 3
        { "VS_2D_POS", "" },
        { "ENABLE_TEXTURE", "" },
        { nullptr, nullptr },
        // 9, 4
        { "VS_2D_POS", "" },
        { "ENABLE_COLOR", "" },
        { "ENABLE_TEXTURE", "" },
        { nullptr, nullptr },
        // 13, 3
        { "MESH_TYPE", "2" },
        { "LIGHT_TYPE", "0" },
        { nullptr, nullptr },
        // 16, 3
        { "MESH_TYPE", "2" },
        { "LIGHT_TYPE", "1" },
        { nullptr, nullptr },
        // 19, 3
        { "MESH_TYPE", "2" },
        { "LIGHT_TYPE", "2" },
        { nullptr, nullptr },
        // 22, 3
        { "TERRAIN_TEXTURE", "1" },
        { "ENABLE_SHADOW", "1" },
        { nullptr, nullptr },
        // 25, 3
        { "TERRAIN_TEXTURE", "1" },
        { "ENABLE_SHADOW", "0" },
        { nullptr, nullptr },
        //------------------
        //------------------
        //------------------
    };

    struct ShaderDesc
    {
        uint16_t id;
        uint16_t defineStart;
    };

    struct ShaderSource
    {
        const char* path;
        uint64_t timestamp;
        uint16_t firstShader;
        uint16_t lastShader;
    };

    ShaderDesc vsShaderDescs[] =
    {
        { 0, 0 },
        { 1, 1 },
        { 2, 4 },
        { 3, 3 },
        { 4, 7 },
        { 5, 6 },
        { 6, 10 },
        { 7, 9 },
        { 8, 0 },
        { 9, 0 },
        { 10, 0 },
        { 11, 13 },
        { 12, 16 },
        { 13, 19 },
        { 14, 0 },
    };

    ShaderSource vsShaderSources[] = 
    {
        { "Data\\shaders\\std.vs", 0, 0, 8 },
        { "Data\\shaders\\procedural_texture.vs", 0, 8, 9},
        { "Data\\shaders\\terrain_blend.vs", 0, 9, 10 },
        { "Data\\shaders\\terrain_lighting.vs", 0, 10, 11 },
        { "Data\\shaders\\light_source.vs", 0, 11, 14 },
        { "Data\\shaders\\decal_lighting.vs", 0, 14, 15 },
    };

    ShaderDesc psShaderDescs[] =
    {
        { 0, 0 },
        { 1, 1 },
        { 2, 4 },
        { 3, 3 },
        { 4, 7 },
        { 5, 6 },
        { 6, 10 },
        { 7, 9 },
        { 8, 0 },
        { 9, 0 },
        { 10, 0 },
        { 11, 0 },
        { 12, 22 },
        { 13, 25 },
        { 14, 0 },
        { 15, 0 },
    };

    ShaderSource psShaderSources[] =
    {
        { "Data\\shaders\\std.ps", 0, 0, 8 },
        { "Data\\shaders\\procedural_texture.ps", 0, 8, 9 },
        { "Data\\shaders\\procedural_texture_distortion.ps", 0, 9, 10 },
        { "Data\\shaders\\terrain_blend.ps", 0, 10, 11 },
        { "Data\\shaders\\terrain_lighting.ps", 0, 11, 12 },
        { "Data\\shaders\\shadow.ps", 0, 12, 14 },
        { "Data\\shaders\\decal_shadow.ps", 0, 14, 15 },
        { "Data\\shaders\\decal_lighting.ps", 0, 15, 16 },
    };

    template<typename ShaderType, size_t NUM_SHADERS, size_t NUM_DESCS>
    void compileShaderSource(gfx::Device& device, ShaderType (&shaders)[NUM_SHADERS], ShaderSource& source, ShaderDesc (&descs)[NUM_DESCS])
    {
        uint32_t size;
        char_array_t source_array = readNewerFile(source.path, size, source.timestamp);
        
        if (!source_array) return;

        for (size_t i = source.firstShader; i < source.lastShader; ++i)
        {
            assert(i < NUM_DESCS);
            ShaderDesc& desc = descs[i];
            assert(desc.id < NUM_SHADERS);
            createShader(
                &shaders[desc.id],
                device,
                source.path,
                size,
                source_array.get( ),
                defines + desc.defineStart
            );
        }
    }

    template<typename ShaderType, size_t NUM_SHADERS, size_t NUM_SOURCES, size_t NUM_DESCS>
    void compileShaders(gfx::Device& device, ShaderType (&shaders)[NUM_SHADERS], ShaderSource (&sources)[NUM_SOURCES], ShaderDesc (&descs)[NUM_DESCS])
    {
        for (auto& source : sources)
        {
            compileShaderSource(device, shaders, source, descs);
        }
    }

    bool ProgramManager::init(gfx::Device& device)
    {
        compileShaders(device, vertexShaders, vsShaderSources, vsShaderDescs);
        compileShaders(device, pixelShaders, psShaderSources, psShaderDescs);

        return true;
    }

    void ProgramManager::fini()
    {
        for (size_t i = 0; i < VS_SHADER_COUNT; ++i)
        {
            if (vertexShaders[i])
            {
                vertexShaders[i]->Release();
                vertexShaders[i] = nullptr;
            }
        }

        for (size_t i = 0; i < PS_SHADER_COUNT; ++i)
        {
            if (pixelShaders[i])
            {
                pixelShaders[i]->Release();
                pixelShaders[i] = nullptr;
            }
        }
    }

    void ProgramManager::update(gfx::Device& device)
    {
        compileShaders(device, vertexShaders, vsShaderSources, vsShaderDescs);
        compileShaders(device, pixelShaders, psShaderSources, psShaderDescs);
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

    void ProgramManager::setTextureMatrix(uint32_t index, const D3DXMATRIX& matrix)
    {
        assert(index < MAX_TEXTURE_MATRICES);

        textureMatrices[index] = matrix;
    }

    void ProgramManager::setAmbient(const COL& color)
    {
        ambient = D3DXVECTOR4(color.r, color.g, color.b, 1.0f);
    }

    void ProgramManager::setDiffuse(const COL& color)
    {
        diffuse = D3DXVECTOR4(color.r, color.g, color.b, 1.0f);
    }

    void ProgramManager::setFog(float start, float end)
    {
        float range = start - end;
        fogParams = D3DXVECTOR4(end, 1.0f / range, 0.0f, 0.0f);
    }

    void ProgramManager::setFogColor(const COL& color)
    {
        fogColor = D3DXVECTOR4(color.r, color.g, color.b, 1.0f);
    }

    void ProgramManager::setLightmapColor(const COL& color)
    {
        lightmapColor = D3DXVECTOR4(color.r, color.g, color.b, 1.0f);
    }

    void ProgramManager::setDirectLight(const VC3& dir, float strength)
    {
        lightSourceParams = D3DXVECTOR4(dir.x, dir.y, dir.z, strength);
    }

    void ProgramManager::setPointLight(const VC3& pos, float range)
    {
        lightSourceParams = D3DXVECTOR4(pos.x, pos.y, pos.z, 1.0f / range);
    }

    void ProgramManager::setStdProgram(gfx::Device& device, uint16_t id)
    {
        assert(id < PROGRAM_COUNT);

        activeProgram = id;

        device.SetVertexShader(vertexShaders[programs[activeProgram].vs]);
        device.SetPixelShader(pixelShaders[programs[activeProgram].ps]);
    }

    void ProgramManager::setProgram(uint16_t id)
    {
        assert(id < PROGRAM_COUNT);

        activeProgram = id;
    }

    void ProgramManager::commitConstants(gfx::Device& device)
    {
        D3DXMATRIX MVP;

        D3DXMatrixMultiply(&MVP, &viewMatrix, &projMatrix);
        D3DXMatrixMultiply(&MVP, &worldMatrix, &MVP);
        D3DXMatrixTranspose(&MVP, &MVP);

        device.SetVertexShaderConstantF(0, MVP, 4);
    }

    void ProgramManager::applyState(gfx::Device& device)
    {
        D3DXMATRIX MVP;

        device.SetVertexShader(vertexShaders[programs[activeProgram].vs]);
        device.SetPixelShader(pixelShaders[programs[activeProgram].ps]);

        D3DXMatrixMultiply(&MVP, &viewMatrix, &projMatrix);
        D3DXMatrixMultiply(&MVP, &worldMatrix, &MVP);
        D3DXMatrixTranspose(&MVP, &MVP);

        device.SetVertexShaderConstantF(0, MVP, 4);

        if (activeProgram == TERRAIN_LIGHTING)
        {
            device.SetVertexShaderConstantF(12, textureMatrices[1], 4);
            device.SetVertexShaderConstantF(16, textureMatrices[2], 4);
            device.SetVertexShaderConstantF(20, fogParams, 1);
            device.SetVertexShaderConstantF(21, diffuse, 1);
            device.SetVertexShaderConstantF(22, ambient, 1);
            device.SetVertexShaderConstantF(23, lightSourceParams, 1);

            device.SetPixelShaderConstantF(0, lightmapColor, 1);
            device.SetPixelShaderConstantF(1, fogColor, 1);
        }
        else if (activeProgram == TERRAIN_PROJECTION_FLAT_SHADOW ||
            activeProgram == TERRAIN_PROJECTION_POINT_SHADOW ||
            activeProgram == TERRAIN_PROJECTION_DIRECT_SHADOW ||
            activeProgram == TERRAIN_PROJECTION_FLAT_SHADOW ||
            activeProgram == TERRAIN_PROJECTION_POINT_SHADOW ||
            activeProgram == TERRAIN_PROJECTION_DIRECT_SHADOW)
        {
            device.SetVertexShaderConstantF(4, worldMatrix, 3);
            device.SetVertexShaderConstantF(8, textureMatrices[0], 4);
            device.SetVertexShaderConstantF(12, textureMatrices[1], 4);
            device.SetVertexShaderConstantF(16, textureMatrices[2], 4);
            device.SetVertexShaderConstantF(21, diffuse, 1);
            device.SetVertexShaderConstantF(23, lightSourceParams, 1);
        }
        else if (activeProgram == DECAL_LIGHTING)
        {
            device.SetVertexShaderConstantF(4, worldMatrix, 3);
            device.SetVertexShaderConstantF(16, textureMatrices[2], 4);
            device.SetVertexShaderConstantF(20, fogParams, 1);
            device.SetVertexShaderConstantF(21, diffuse, 1);

            device.SetPixelShaderConstantF(1, fogColor, 1);
        }
    }
}