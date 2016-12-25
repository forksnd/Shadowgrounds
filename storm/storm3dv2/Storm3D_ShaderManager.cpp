// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "Storm3D_ShaderManager.h"
#include "storm3d_model.h"
#include "storm3d_model_object.h"
#include "Storm3D_Bone.h"
#include "storm3d_mesh.h"

#include "../../filesystem/input_stream_wrapper.h"
#include "../../util/Debug_MemoryManager.h"

using namespace frozenbyte;

//HAXHAX
bool enableLocalReflection = false;
float reflection_height = 0.f;
D3DXMATRIX reflection_matrix;
D3DXMATRIX clip_matrix;

// 106 = 21
// 107 = 22
// 108 = 23
// 109 = 24
// 110 = 25
// 111 = 26

// 21, 25
const int Storm3D_ShaderManager::BONE_INDEX_START = 42;
const int Storm3D_ShaderManager::BONE_INDICES = 48;

Storm3D_ShaderManager::Storm3D_ShaderManager(gfx::Device& device)
:	ambient_color(.7f,.7f,.7f,0),
	ambient_force_color(0,0,0,0),
    fog_color(0.0f, 0.0f, 0.0f, 1.0f),
	fog(-20.f,1.f / 100.f,0.0f,1.f),

	object_ambient_color(0,0,0,0),
	object_diffuse_color(1.f,1.f,1.f,0),

	update_values(true),

	projected_shaders(false),
	fake_depth_shaders(false),
	fake_shadow_shaders(false),
	model(0),

	transparency_factor(1.f),

	reflection(false),
	local_reflection(false),
	light_count(0),
	light_params_changed(true),
	spot_type(Directional)
{
	for(int i = 0; i < LIGHT_MAX_AMOUNT; ++i)
	{
		light_position[i].x = 0;
		light_position[i].y = 25.f;
		light_position[i].z = 0;
		light_color[i].x = .5f;
		light_color[i].y = .5f;
		light_color[i].z = .5f;
	}

	sun_properties.x = 0.f;
	sun_properties.y = 0.f;
	sun_properties.z = 0.f;
	sun_properties.w = 0.f;

	D3DXMatrixIdentity(&reflection_matrix);
	reflection_matrix._22 = -1.f;
	reflection_matrix._42 = 2 * reflection_height;

    std::string vs_shader_source, ps_shader_source;

    frozenbyte::storm::readFile(vs_shader_source, "Data\\shaders\\mesh.vs");
    frozenbyte::storm::readFile(ps_shader_source, "Data\\shaders\\lighting.ps");

    D3D_SHADER_MACRO definesSimple[] = {
        {"ENABLE_SKELETAL_ANIMATION", "1"},
        {"ENABLE_LIGHTING", "1"},
        {"ENABLE_LIGHTMAP", "0"},
        {"ENABLE_FAKE_LIGHT", "0"},
        {"ENABLE_FOG", "0"},
        {"TEXTURE_SOURCE", "1"},
        {0, 0}
    };

    createShader(&meshVS[MESH_BONE_SIMPLE], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesSimple);
    createShader(&meshPS[LIGHTING_SIMPLE_TEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesSimple);
    definesSimple[5].Definition = "0";
    createShader(&meshPS[LIGHTING_SIMPLE_NOTEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesSimple);

    D3D_SHADER_MACRO definesStandard[] = {
        {"ENABLE_SKELETAL_ANIMATION", "1"},
        {"ENABLE_LIGHTING", "1"},
        {"ENABLE_LIGHTMAP", "1"},
        {"ENABLE_FAKE_LIGHT", "1"},
        {"ENABLE_FOG", "1"},
        {"TEXTURE_SOURCE", "1"},
        {0, 0}
    };

    createShader(&meshVS[MESH_BONE_NOREFLECTION], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "2";
    createShader(&meshVS[MESH_BONE_REFLECTION], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesStandard);
    definesStandard[0].Definition = "0";
    definesStandard[5].Definition = "1";
    createShader(&meshVS[MESH_NOREFLECTION], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "2";
    createShader(&meshVS[MESH_REFLECTION], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "3";
    createShader(&meshVS[MESH_LOCAL_REFLECTION], &device, vs_shader_source.length(), vs_shader_source.c_str(), definesStandard);

    definesStandard[5].Definition = "0";
    createShader(&meshPS[LIGHTING_LMAP_NOTEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "1";
    createShader(&meshPS[LIGHTING_LMAP_TEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "2";
    createShader(&meshPS[LIGHTING_LMAP_REFLECTION], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "3";
    createShader(&meshPS[LIGHTING_LMAP_LOCAL_REFLECTION], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);

    definesStandard[5].Definition = "0";
    createShader(&meshPS[LIGHTING_NOLMAP_NOTEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "1";
    createShader(&meshPS[LIGHTING_NOLMAP_TEXTURE], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "2";
    createShader(&meshPS[LIGHTING_NOLMAP_REFLECTION], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);
    definesStandard[5].Definition = "3";
    createShader(&meshPS[LIGHTING_NOLMAP_LOCAL_REFLECTION], &device, ps_shader_source.length(), ps_shader_source.c_str(), definesStandard);

    std::string vs_skybox_source;
    frozenbyte::storm::readFile(vs_skybox_source, "Data\\shaders\\skybox.vs");
    createShader(&meshVS[VERTEX_SKYBOX], &device, vs_skybox_source.length(), vs_skybox_source.c_str(), NULL);

    std::string ps_std_source;
    frozenbyte::storm::readFile(ps_std_source, "Data\\shaders\\std.ps");

    D3D_SHADER_MACRO definesSTD[] = {
        {"ENABLE_COLOR", "1"},
        {"ENABLE_TEXTURE", "1"},
        {0, 0}
    };
    createShader(&meshPS[WHITE_ONLY], &device, ps_std_source.length(), ps_std_source.c_str(), NULL);
    createShader(&meshPS[TEXTURE_ONLY], &device, ps_std_source.length(), ps_std_source.c_str(), definesSTD+1);
    createShader(&meshPS[TEXTURExCOLOR], &device, ps_std_source.length(), ps_std_source.c_str(), definesSTD);

    std::string ps_debug_source;
    frozenbyte::storm::readFile(ps_debug_source, "Data\\shaders\\debug.ps");
    createShader(&meshPS[DEBUG_PIXEL_SHADER], &device, ps_debug_source.length(), ps_debug_source.c_str(), NULL);

    std::string ps_fake_depth_source;
    frozenbyte::storm::readFile(ps_fake_depth_source, "Data\\shaders\\fake_depth.ps");
    createShader(&meshPS[FAKE_DEPTH], &device, ps_fake_depth_source.length(), ps_fake_depth_source.c_str(), NULL);

    std::string vs_fake_spot_shadow_source;
    frozenbyte::storm::readFile(vs_fake_spot_shadow_source, "Data\\shaders\\fake_spot_shadow.vs");
    createShader(&meshVS[VERTEX_FAKE_SPOT_SHADOW], &device, vs_fake_spot_shadow_source.length(), vs_fake_spot_shadow_source.c_str(), NULL);

    std::string ps_fake_spot_shadow_source;
    frozenbyte::storm::readFile(ps_fake_spot_shadow_source, "Data\\shaders\\fake_spot_shadow.ps");
    createShader(&meshPS[PIXEL_FAKE_SPOT_SHADOW], &device, ps_fake_spot_shadow_source.length(), ps_fake_spot_shadow_source.c_str(), NULL);

    std::string vs_projected_source;
    frozenbyte::storm::readFile(vs_projected_source, "Data\\shaders\\mesh_projected.vs");

    D3D_SHADER_MACRO definesProjected[] = {
        {"ENABLE_SKELETAL_ANIMATION", "1"},
        {"LIGHT_TYPE", "0"},
        {0, 0}
    };

    createShader(&meshVS[MESH_BONE_PROJECTED_FLAT], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);
    definesProjected[1].Definition = "1";
    createShader(&meshVS[MESH_BONE_PROJECTED_POINT], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);
    definesProjected[1].Definition = "2";
    createShader(&meshVS[MESH_BONE_PROJECTED_DIRECTIONAL], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);

    definesProjected[0].Definition = "0";

    definesProjected[1].Definition = "0";
    createShader(&meshVS[MESH_PROJECTED_FLAT], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);
    definesProjected[1].Definition = "1";
    createShader(&meshVS[MESH_PROJECTED_POINT], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);
    definesProjected[1].Definition = "2";
    createShader(&meshVS[MESH_PROJECTED_DIRECTIONAL], &device, vs_projected_source.length(), vs_projected_source.c_str(), definesProjected);

    D3D_SHADER_MACRO definesShadow[] = {
        {"ENABLE_SHADOW", "0"},
        {0, 0}
    };

    std::string ps_shadow_source;
    frozenbyte::storm::readFile(ps_shadow_source, "Data\\shaders\\shadow.ps");
    createShader(&meshPS[PIXEL_NO_SHADOW], &device, ps_shadow_source.length(), ps_shadow_source.c_str(), definesShadow);
    definesShadow[0].Definition = "1";
    createShader(&meshPS[PIXEL_SHADOW], &device, ps_shadow_source.length(), ps_shadow_source.c_str(), definesShadow);
}

Storm3D_ShaderManager::~Storm3D_ShaderManager()
{
    for (size_t i=0; i<MESH_VS_SHADER_COUNT; ++i)
    {
        if (meshVS[i]) meshVS[i]->Release();
    }

    for (size_t i=0; i<MESH_PS_SHADER_COUNT; ++i)
    {
        if (meshPS[i]) meshPS[i]->Release();
    }
}

void Storm3D_ShaderManager::CreateShaders(gfx::Device& device)
{
	// Set identity matrix on card
	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device.SetVertexShaderConstantF(BONE_INDEX_START, identity, 3);
}

void Storm3D_ShaderManager::setLightingParameters(bool reflection_, bool local_reflection_, int light_count_)
{
	if(light_count_ > LIGHT_MAX_AMOUNT)
		light_count_ = LIGHT_MAX_AMOUNT;
	if(light_count_ < 0)
		light_count_ = 0;

	if(reflection != reflection_ || light_count != light_count_ || local_reflection != local_reflection_)
	{
		reflection = reflection_;
		local_reflection = local_reflection_;
		light_count = light_count_;
		light_params_changed = true;
	}
}

void Storm3D_ShaderManager::SetTransparencyFactor(float factor)
{
	transparency_factor = factor;
}

void Storm3D_ShaderManager::SetViewProjectionMatrix(const D3DXMATRIX &vp, const D3DXMATRIX &view)
{
	view_projection_tm = vp;

	D3DXMatrixIdentity(&clip_matrix);
	D3DXMatrixIdentity(&reflection_matrix);

	// HAXHAX
	if(enableLocalReflection)
	{
#ifdef PROJECT_CLAW_PROTO
		//reflection_height = 17.8f;
#endif
		{
			reflection_matrix._22 = -1.f;
			reflection_matrix._42 = 2 * reflection_height;
		}

		D3DXMatrixMultiply(&view_projection_tm, &reflection_matrix, &view_projection_tm);

		// Oblique depth projection
		{
			// Wanted plane
			D3DXPLANE plane;
			D3DXVECTOR3 point(0.f, reflection_height, 0.f);
			D3DXVECTOR3 normal(0.f, 1.f, 0.f);
			D3DXPlaneFromPointNormal(&plane, &point, &normal);
			D3DXVECTOR4 clipPlane(plane.a, plane.b, plane.c, plane.d);

			// Transform plane
			D3DXMATRIX normalizedViewProjection;
			D3DXMatrixInverse(&normalizedViewProjection, 0, &view_projection_tm);
			D3DXMatrixTranspose(&normalizedViewProjection, &normalizedViewProjection);

			D3DXVECTOR4 projectedPlane;
			D3DXVec4Transform(&projectedPlane, &clipPlane, &normalizedViewProjection);

			if(projectedPlane.w > 0)
			{
				D3DXVECTOR4 tempPlane = -clipPlane;
				D3DXVec4Transform(&projectedPlane, &tempPlane, &normalizedViewProjection);
			}

			// Create skew matrix
			D3DXMatrixIdentity(&clip_matrix);
			clip_matrix(0, 2) = projectedPlane.x;
			clip_matrix(1, 2) = projectedPlane.y;
			clip_matrix(2, 2) = projectedPlane.z;
			clip_matrix(3, 2) = projectedPlane.w;
			view_projection_tm = view_projection_tm * clip_matrix;
		}
	}
}

void Storm3D_ShaderManager::SetViewPosition(const D3DXVECTOR4 &p)
{
	view_position = p;
}

void Storm3D_ShaderManager::SetAmbient(const Color &color)
{
	ambient_color.x = color.r;
	ambient_color.y = color.g;
	ambient_color.z = color.b;

	object_ambient_color = ambient_color;
	update_values = true;
}

void Storm3D_ShaderManager::SetForceAmbient(const Color &color)
{
	ambient_force_color.x = color.r;
	ambient_force_color.y = color.g;
	ambient_force_color.z = color.b;
}

void Storm3D_ShaderManager::SetFogColor(const Color& color)
{
	fog_color.x = color.r;
	fog_color.y = color.g;
	fog_color.z = color.b;
}

void Storm3D_ShaderManager::SetLight(int index, const Vector &direction, const Color &color, float range)
{
	if(index >= 0 && index < LIGHT_MAX_AMOUNT)
	{
		light_position[index].x = direction.x;
		light_position[index].y = direction.y;
		light_position[index].z = direction.z;
		if(index == 0)
			light_position[index].w = 0.f;

		light_color[index].x = color.r;
		light_color[index].y = color.g;
		light_color[index].z = color.b;
		light_color[index].w = 1.f / range;
	}
}

void Storm3D_ShaderManager::SetSun(const Vector &direction, float strength)
{
	sun_properties.x = direction.x;
	sun_properties.y = direction.y;
	sun_properties.z = direction.z;
	sun_properties.w = strength;

	update_values = true;
}

void Storm3D_ShaderManager::SetFog(float start, float range)
{
	fog.x = start - range;
	fog.y = 1.f / range;
	fog.z = 0.f;
	fog.w = 1.f;

	update_values = true;
}

void Storm3D_ShaderManager::SetTextureOffset(const VC2 &offset)
{
	textureOffset.x = offset.x;
	textureOffset.y = offset.y;
}

void Storm3D_ShaderManager::setFakeProperties(float plane, float factor, float add)
{
	fake_properties.x = plane;
	fake_properties.y = factor;
	fake_properties.z = add;
}

void Storm3D_ShaderManager::SetModelAmbient(const Color &color)
{
	model_ambient_color.x = color.r;
	model_ambient_color.y = color.g;
	model_ambient_color.z = color.b;

	update_values = true;
}

void Storm3D_ShaderManager::SetObjectAmbient(const Color &color)
{
	object_ambient_color.x = color.r;
	object_ambient_color.y = color.g;
	object_ambient_color.z = color.b;

	update_values = true;
}

void Storm3D_ShaderManager::SetObjectDiffuse(const Color &color)
{
	object_diffuse_color.x = color.r;
	object_diffuse_color.y = color.g;
	object_diffuse_color.z = color.b;

	update_values = true;
}

void Storm3D_ShaderManager::setProjectedShaders()
{
	update_values = true;
	model = 0;

	lighting_shaders = false;
	projected_shaders = true;
	fake_depth_shaders = false;
	fake_shadow_shaders = false;
}

void Storm3D_ShaderManager::setLightingShaders()
{
	lighting_shaders = true;
	projected_shaders = false;
	fake_depth_shaders = false;
	fake_shadow_shaders = false;
}

void Storm3D_ShaderManager::setNormalShaders()
{
	update_values = true;
	model = 0;

	lighting_shaders = false;
	projected_shaders = false;
	fake_depth_shaders = false;
	fake_shadow_shaders = false;
}

void Storm3D_ShaderManager::setFakeDepthShaders()
{
	update_values = true;
	model = 0;

	lighting_shaders = false;
	projected_shaders = false;
	fake_depth_shaders = true;
	fake_shadow_shaders = false;
}

void Storm3D_ShaderManager::setFakeShadowShaders()
{
	update_values = true;
	model = 0;

	lighting_shaders = false;
	projected_shaders = false;
	fake_depth_shaders = false;
	fake_shadow_shaders = true;
}

void Storm3D_ShaderManager::setTextureTm(D3DXMATRIX &matrix)
{
	D3DXMatrixTranspose(&texture_matrix, &matrix);
}

void Storm3D_ShaderManager::setSpot(const COL &color, const VC3 &position, const VC3 &direction, float range, float fadeFactor)
{
	spot_color.x = color.r;
	spot_color.y = color.g;
	spot_color.z = color.b;
	spot_color.w = 1.f;

	spot_position.x = position.x;
	spot_position.y = position.y;
	spot_position.z = position.z;
	spot_position.w = 1.f / range;

	spot_properties.x = -direction.x;
	spot_properties.y = -direction.y;
	spot_properties.z = -direction.z;
	spot_properties.w = 1.f / range;
}

void Storm3D_ShaderManager::setSpotTarget(const D3DXMATRIX &matrix)
{
	D3DXMatrixTranspose(&target_matrix, &matrix);
}

void Storm3D_ShaderManager::setSpotType(SpotType type)
{
	spot_type = type;
}

bool Storm3D_ShaderManager::BoneShader()
{
    return currentVertexShader == MESH_BONE_SIMPLE ||
           currentVertexShader == MESH_BONE_NOREFLECTION ||
           currentVertexShader == MESH_BONE_REFLECTION ||
           currentVertexShader == MESH_BONE_PROJECTED_FLAT ||
           currentVertexShader == MESH_BONE_PROJECTED_DIRECTIONAL||
           currentVertexShader == MESH_BONE_PROJECTED_POINT;
}

void Storm3D_ShaderManager::SetShaders(
    gfx::Device& device,
    uint32_t vertexShader,
    uint32_t pixelShader,
    Storm3D_Model_Object *object
)
{
    assert(device.device);
    assert(vertexShader<MESH_VS_SHADER_COUNT);
    assert(pixelShader<MESH_PS_SHADER_COUNT);

    currentVertexShader = vertexShader;
    currentPixelShader  = pixelShader;

    device.SetVertexShader(meshVS[vertexShader]);
    device.SetPixelShader(meshPS[pixelShader]);

    D3DXMATRIX object_tm;
    float alpha = 1.f;

    if (object)
    {
        object->GetMXG().GetAsD3DCompatible4x4((float *)&object_tm);
        SetWorldTransform(device, object_tm);

        bool noBones = object->parent_model->bones.empty()
                        || (static_cast<Storm3D_Mesh *> (object->GetMesh())->HasWeights() == false);

        model = noBones ? NULL : object->parent_model;

        IStorm3D_Material *m = object->GetMesh()->GetMaterial();
        float force_alpha = object->force_alpha;
        if (projected_shaders && object->force_lighting_alpha_enable)
            force_alpha = object->force_lighting_alpha;

        IStorm3D_Material::ATYPE a = m->GetAlphaType();
        if (a == IStorm3D_Material::ATYPE_USE_TRANSPARENCY)
            alpha = 1.f - m->GetTransparency() - force_alpha;
        else if (a == IStorm3D_Material::ATYPE_USE_TEXTRANSPARENCY || force_alpha > 0.0001f)
            alpha = 1.f - m->GetTransparency() - force_alpha;
        else if (a == IStorm3D_Material::ATYPE_USE_ALPHATEST)
            alpha = 1.f - m->GetTransparency();

        if (alpha < 0)
            alpha = 0;
    }

    if (projected_shaders)
    {
        D3DXVECTOR4 dif = object_diffuse_color;
        dif.x *= spot_color.x;
        dif.y *= spot_color.y;
        dif.z *= spot_color.z;

        dif.w = alpha * transparency_factor;
        device.SetVertexShaderConstantF(17, dif, 1);
    }

    if (vertexShader == VERTEX_FAKE_SPOT_SHADOW ||
        vertexShader == MESH_BONE_PROJECTED_FLAT ||
        vertexShader == MESH_BONE_PROJECTED_POINT ||
        vertexShader == MESH_BONE_PROJECTED_DIRECTIONAL ||
        vertexShader == MESH_PROJECTED_FLAT ||
        vertexShader == MESH_PROJECTED_POINT ||
        vertexShader == MESH_PROJECTED_DIRECTIONAL)
    {
        //TODO: remove later - atm constants should be set in SetWorldTransform,
        //      due to fake_shadow_shaders==true
        return;
    }

    {
        D3DXMatrixTranspose(&object_tm, &object_tm);
        device.SetVertexShaderConstantF(4, object_tm, 3);

        // Set transparency?
        D3DXVECTOR4 ambient = ambient_color;
        ambient *= sun_properties.w;
        ambient += model_ambient_color + ambient_force_color;
        //ambient += object_ambient_color;

        ambient.x = max(ambient.x, object_ambient_color.x);
        ambient.y = max(ambient.y, object_ambient_color.y);
        ambient.z = max(ambient.z, object_ambient_color.z);

#ifdef HACKY_SG_AMBIENT_LIGHT_FIX
        // EVIL HAX around too dark characters etc.
        const float MIN_AMBIENT_LIGHT = 0.05f;
        ambient.x = max(ambient.x, MIN_AMBIENT_LIGHT);
        ambient.y = max(ambient.y, MIN_AMBIENT_LIGHT);
        ambient.z = max(ambient.z, MIN_AMBIENT_LIGHT);

        ambient.x = min(ambient.x, 1.0f);
        ambient.y = min(ambient.y, 1.0f);
        ambient.z = min(ambient.z, 1.0f);
#else
        if (ambient.x > 1.f)
            ambient.x = 1.f;
        if (ambient.y > 1.f)
            ambient.y = 1.f;
        if (ambient.z > 1.f)
            ambient.z = 1.f;
#endif

        D3DXVECTOR4 diffuse = object_diffuse_color;
        diffuse.w = alpha; //1.f;

        device.SetVertexShaderConstantF(7, ambient, 1);
        if (!fake_depth_shaders)
            device.SetVertexShaderConstantF(8, diffuse, 1);
    }

    // Set actual shader
    D3DXVECTOR4 sun_temp = sun_properties;
    sun_temp.w = textureOffset.x;
    device.SetVertexShaderConstantF(11, sun_temp, 1);

    device.SetVertexShaderConstantF(9, textureOffset, 1);

    device.SetVertexShaderConstantF(18, view_position, 1);
    device.SetVertexShaderConstantF(18, view_position, 1);
    device.SetVertexShaderConstantF(19, fog, 1);
    device.SetPixelShaderConstantF(20, fog_color, 1);

    if (local_reflection)
    {
        D3DXMATRIX reflection_matrix(0.5f, 0.0f, 0.0f, 0.5f,
            0.0f, -0.5f, 0.0f, 0.5f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        device.SetVertexShaderConstantF(27, reflection_matrix, 4);
    }

    int lightCount[4] = { LIGHT_MAX_AMOUNT, 0, 0, 0 };
    device.SetVertexShaderConstantI(0, lightCount, 1);

    static_assert(LIGHT_MAX_AMOUNT >= 2 && LIGHT_MAX_AMOUNT <= 5, "Light count should be 2-5.");
    for (int i = 0; i < LIGHT_MAX_AMOUNT; ++i)
    {
        D3DXVECTOR4 lightPosInvRange = light_position[i];
        D3DXVECTOR4 lightColor = light_color[i];
        lightPosInvRange.w = lightColor.w;
        lightColor.w = 1.0f;

        device.SetVertexShaderConstantF(32 + i * 2, lightPosInvRange, 1);
        device.SetVertexShaderConstantF(33 + i * 2, lightColor, 1);
    }
}

void Storm3D_ShaderManager::SetPixelShader(uint32_t pixelShader)
{
    currentPixelShader = pixelShader;
    use_custom_shader = true;
}

void Storm3D_ShaderManager::SetShader(gfx::Device &device, Storm3D_Model_Object *object)
{
    assert(device.device);

    bool noBones = object->parent_model->bones.empty()
                    || !static_cast<Storm3D_Mesh*>(object->GetMesh())->HasWeights();

    if (!use_custom_shader)
    {
        __asm int 3;
    }

    uint32_t vertexShader = 0;
    if (projected_shaders)
    {
        if (noBones)
        {
            if (spot_type == Directional)
            {
                vertexShader = MESH_PROJECTED_DIRECTIONAL;
            }
            else if (spot_type == Point)
            {
                vertexShader = MESH_PROJECTED_POINT;
            }

            if (spot_type == Flat)
            {
                vertexShader = MESH_PROJECTED_FLAT;
            }
        }
        else
        {
            if (spot_type == Directional)
            {
                vertexShader = MESH_BONE_PROJECTED_DIRECTIONAL;
            }
            else if (spot_type == Point)
            {
                vertexShader = MESH_BONE_PROJECTED_POINT;
            }

            if (spot_type == Flat)
            {
                vertexShader = MESH_BONE_PROJECTED_FLAT;
            }
        }
    }
    else
    {
        if (noBones)
        {
            if (reflection)
            {
                vertexShader = MESH_REFLECTION;
            }
            else if (local_reflection)
            {
                vertexShader = MESH_LOCAL_REFLECTION;
            }
            else
            {
                vertexShader = MESH_NOREFLECTION;
            }
        }
        else
        {
            if (reflection)
            {
                vertexShader = MESH_BONE_REFLECTION;
            }
            else
            {
                vertexShader = MESH_BONE_NOREFLECTION;
            }
        }

    }

    SetShaders(device, vertexShader, currentPixelShader, object);
    use_custom_shader = false;
}

void Storm3D_ShaderManager::SetShader(gfx::Device &device, const std::vector<int> &bone_indices)
{
    bool setIndices = BoneShader() && !bone_indices.empty() && model;

    if (!setIndices) return;

    float array[96 * 4];
    int bone_amount = model->bones.size();

    D3DXMATRIX foo;
    for (unsigned int i = 0; i < bone_indices.size(); ++i)
    {
        int index = bone_indices[i];
        int shader_index = i; //bone_indices[i].second;

        if (index >= bone_amount)
            continue;

        const MAT &vertexTm = model->bones[index]->GetVertexTransform();
        //vertexTm.GetAsD3DCompatible4x4((float *) foo);
        //D3DXMatrixTranspose(&foo, &foo);
        //device->SetVertexShaderConstantF(BONE_INDEX_START + ((shader_index)*3), foo, 3);

        int arrayIndex = shader_index * 3 * 4;
        //for(unsigned int j = 0; j < 12; ++j)
        //	array[arrayIndex++] = foo[j];

        array[arrayIndex++] = vertexTm.Get(0);
        array[arrayIndex++] = vertexTm.Get(4);
        array[arrayIndex++] = vertexTm.Get(8);
        array[arrayIndex++] = vertexTm.Get(12);
        array[arrayIndex++] = vertexTm.Get(1);
        array[arrayIndex++] = vertexTm.Get(5);
        array[arrayIndex++] = vertexTm.Get(9);
        array[arrayIndex++] = vertexTm.Get(13);
        array[arrayIndex++] = vertexTm.Get(2);
        array[arrayIndex++] = vertexTm.Get(6);
        array[arrayIndex++] = vertexTm.Get(10);
        array[arrayIndex++] = vertexTm.Get(14);
    }

    device.SetVertexShaderConstantF(BONE_INDEX_START, array, 3 * bone_indices.size());
}

void Storm3D_ShaderManager::ResetShader()
{
	light_count = 1000000000;
	model = 0;
	update_values = true;
	projected_shaders = false;
	lighting_shaders = false;
	// ...
	fake_depth_shaders = false;
	fake_shadow_shaders = false;

	object_ambient_color = ambient_color;
	transparency_factor = 1.f;
}

void Storm3D_ShaderManager::ClearCache()
{
	transparency_factor = 1.f;
	model = 0;
	update_values = true;

	object_ambient_color = ambient_color;
	object_diffuse_color.x = 1.f;
	object_diffuse_color.y = 1.f;
	object_diffuse_color.z = 1.f;
	object_diffuse_color.w = 1.f;
	sun_properties.w = 1.f;

	model_ambient_color.x = 0.f;
	model_ambient_color.y = 0.f;
	model_ambient_color.z = 0.f;
	model_ambient_color.w = 0.f;
}

void Storm3D_ShaderManager::SetShaderDefaultValues(gfx::Device &device)
{
	// Set values
	D3DXVECTOR4 foo(1,1,1,1);
	device.SetVertexShaderConstantF(7, foo, 1);
	device.SetVertexShaderConstantF(8, foo, 1);
}

void Storm3D_ShaderManager::SetShaderAmbient(gfx::Device &device, const COL &color)
{
	D3DXVECTOR4 ambient(color.r, color.g, color.b, 0.f);
	D3DXVECTOR4 diffuse(1, 1, 1, 0);

	device.SetVertexShaderConstantF(7, ambient, 1);
	device.SetVertexShaderConstantF(8, diffuse, 1);
}

void Storm3D_ShaderManager::SetShaderDiffuse(gfx::Device &device, const COL &color)
{
	D3DXVECTOR4 diffuse(color.r, color.g, color.b, 0.f);
	device.SetVertexShaderConstantF(8, diffuse, 1);
}

void Storm3D_ShaderManager::SetLightmapFactor(float xf, float yf)
{
	lightmap_factor.x = xf;
	lightmap_factor.y = yf;
	lightmap_factor.z = 0;
	lightmap_factor.w = 0;
}

void Storm3D_ShaderManager::SetWorldTransform(gfx::Device &device, const D3DXMATRIX &tm, bool forceTextureTm, bool terrain)
{
	update_values = true;

	D3DXMATRIX result;
	D3DXMatrixMultiply(&result, &tm, &view_projection_tm);
	D3DXMatrixTranspose(&result, &result);

	// ViewProj matrix
	device.SetVertexShaderConstantF(0, result, 4);

	if(projected_shaders || fake_shadow_shaders || fake_depth_shaders || forceTextureTm)
	{
		D3DXMATRIX foo = tm;
		D3DXMatrixTranspose(&foo, &foo);
		D3DXMatrixMultiply(&foo, &texture_matrix, &foo);
		device.SetVertexShaderConstantF(4, foo, 4);

		foo = tm;
		D3DXMatrixTranspose(&foo, &foo);

		if(!fake_depth_shaders)
			device.SetVertexShaderConstantF(8, foo, 3);
	}

	if(lighting_shaders)
	{
		device.SetVertexShaderConstantF(9, light_position[0], 1);
		device.SetVertexShaderConstantF(10, light_color[0], 1);

		D3DXVECTOR4 sun_temp = sun_properties;
		sun_temp.w = textureOffset.x;
		device.SetVertexShaderConstantF(11, sun_temp, 1);

		if(LIGHT_MAX_AMOUNT > 1)
		{
			D3DXVECTOR4 light_position2_temp = light_position[1];
			light_position2_temp.w = textureOffset.y;
			device.SetVertexShaderConstantF(12, light_position2_temp, 1);
			device.SetVertexShaderConstantF(13, light_color[1], 1);
		}
		else
		{
			D3DXVECTOR4 position_temp;
			D3DXVECTOR4 color_temp;
			position_temp.w = textureOffset.y;
			device.SetVertexShaderConstantF(12, position_temp, 1);
			device.SetVertexShaderConstantF(13, color_temp, 1);
		}

		if(terrain)
		{
			device.SetVertexShaderConstantF(25, fog, 1);
		}
		else
		{
			device.SetVertexShaderConstantF(18, view_position, 1);
			device.SetVertexShaderConstantF(19, fog, 1);
		}

		if(local_reflection)
		{
			D3DXMATRIX reflection_matrix(	0.5f,	0.0f,	0.0f,	0.5f,
											0.0f,	-0.5f,	0.0f,	0.5f,
											0.0f,	0.0f,	0.0f,	0.0f,
											0.0f,   0.0f,	0.0f,	1.0f);

			device.SetVertexShaderConstantF(27, reflection_matrix, 4);
		}

		if(!terrain)
		{
			static_assert(LIGHT_MAX_AMOUNT >= 2 && LIGHT_MAX_AMOUNT <= 5, "Light count should be 2-5.");
			for(int i = 2; i < LIGHT_MAX_AMOUNT; ++i)
			{
				device.SetVertexShaderConstantF(21 + ((i - 2) * 2), light_position[i], 1);
				device.SetVertexShaderConstantF(22 + ((i - 2) * 2), light_color[i], 1);
			}
		}
	}	
	else
	{
		if(spot_type == Point || fake_shadow_shaders)
			device.SetVertexShaderConstantF(11, spot_position, 1);
		else
			device.SetVertexShaderConstantF(11, spot_properties, 1);

		if(!terrain)
			device.SetVertexShaderConstantF(19, fog, 1);
	}

	//if(projected_shaders || ati_shadow_shaders || fake_depth_shaders)
	if(projected_shaders || fake_depth_shaders || fake_shadow_shaders)
	{
		D3DXMATRIX foo = tm;
		D3DXMatrixTranspose(&foo, &foo);
		D3DXMatrixMultiply(&foo, &target_matrix, &foo);
		device.SetVertexShaderConstantF(12, foo, 4);

		device.SetVertexShaderConstantF(16, textureOffset, 1);
	}

	if(!projected_shaders && !fake_depth_shaders && !fake_shadow_shaders)
	{
		if(!lighting_shaders)
			device.SetVertexShaderConstantF(12, textureOffset, 1);
	}

	if(fake_depth_shaders)
	{
		device.SetVertexShaderConstantF(12, fake_properties, 1);
	}
}

void Storm3D_ShaderManager::ApplyForceAmbient(gfx::Device &device)
{
	D3DXVECTOR4 v = ambient_force_color + ambient_color;
	device.SetVertexShaderConstantF(7, v, 1);
}
