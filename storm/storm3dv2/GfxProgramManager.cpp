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
    size_t source_len,
    const char* source,
    D3D_SHADER_MACRO* defines
)
{
    ID3DBlob* pscode = compileShader(
        PIXEL_SHADER_3_0, source_len, source, defines
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
        // 13, ?
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
    };

    ShaderSource vsShaderSources[] = 
    {
        { "Data\\shaders\\std.vs", 0, 0, 8 },
        { "Data\\shaders\\procedural_texture.vs", 0, 8, 9},
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
    };

    ShaderSource psShaderSources[] =
    {
        { "Data\\shaders\\std.ps", 0, 0, 8 },
        { "Data\\shaders\\procedural_texture.ps", 0, 8, 9 },
        { "Data\\shaders\\procedural_texture_distortion.ps", 0, 9, 10 },
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

    void ProgramManager::setStdProgram(gfx::Device& device, size_t id)
    {
        assert(id < PROGRAM_COUNT);

        device.SetVertexShader(vertexShaders[programs[id].vs]);
        device.SetPixelShader(pixelShaders[programs[id].ps]);
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