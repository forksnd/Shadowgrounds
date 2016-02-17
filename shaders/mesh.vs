//#define ENABLE_SKELETAL_ANIMATION 0
//#define ENABLE_LIGHTING
//#define ENABLE_LIGHTMAP
//#define ENABLE_FAKE_LIGHT
//#define ENABLE_FOG
//#define TEXTURE_SOURCE 0

#define TEXSRC_NO_TEXTURE 0
#define TEXSRC_TEXTURE 1
#define TEXSRC_REFLECTION 2
#define TEXSRC_LOCAL_REFLECTION 3

struct VS_IN
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float4 uv2 : TEXCOORD2;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float2 uv0       : TEXCOORD0;
    float2 uv1       : TEXCOORD1;
#if ENABLE_LIGHTING
    float3 lighting  : COLOR1;
    #if ENABLE_FAKE_LIGHT
        float4 uv2       : TEXCOORD2;
    #endif
#endif
#if TEXTURE_SOURCE == TEXSRC_REFLECTION
    float3 uv3       : TEXCOORD3;
#elif TEXTURE_SOURCE == TEXSRC_LOCAL_REFLECTION
    float4 uv3       : TEXCOORD3;
#endif
#if ENABLE_FOG
    float fogFactor  : FOG;
#endif
};

// World x View x Projection matrix
float4x4 uWVP        : register(c0);
// World matrix (first 3 rows)   
float3x4 uW          : register(c4);
// Texture scrolling animation offset
float2   uTexOffset  : register(c9);
// Camera position in world space
float3   uCameraPos : register(c18);
// Diffuse color
float4   uDiffuse   : register(c8);

#if ENABLE_FAKE_LIGHT
    float4x4 uFakeLightMatrix : register(c14);
#endif
#if TEXTURE_SOURCE == TEXSRC_LOCAL_REFLECTION
    float uReflMatrix : register(c27);
#endif
#if ENABLE_SKELETAL_ANIMATION
    // Skeleton bones
    float3x4 uBoneXF[48] : register(c42);
#endif

#if ENABLE_FOG
    // Fog parameters
    float2   uFog       : register(c19);
#endif

#if ENABLE_LIGHTING

// Sun direction
float3   uSunDir    : register(c11);
// Ambient color (.w transparency)
float3   uAmbient   : register(c7);

// Num point lights
int uNumLights : register(i0);

struct Light
{
    float4 posInvRange; // position and w - inverse light range
    float3 color; // light color
};

// Per-light data
Light uLights[5] : register(c32);

float3 calcLighting(float3 world_pos, float3 normal)
{
    float3 lighting = 0;

    for (int i = 0; i < uNumLights; ++i)
    {
        float3 vec = uLights[i].posInvRange.xyz - world_pos;

        float3 dir = normalize(vec);

        float LdotN = max(0.0, dot(normal, dir));

        float dist  = length(vec);
        float att   = max(0.0, 1.0 - dist * uLights[i].posInvRange.w);

        lighting += (att * LdotN) * uLights[i].color;
    }

    float sunNdotL = max(0.0, dot(normal, uSunDir));

    lighting += sunNdotL;
    lighting += uAmbient;

    return lighting;
}

#endif

//TODO: Remove!!! UGLY HACK!!!
static const float hack_base_index = 42;

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float index0, weight0;
    float index1, weight1;
    float weight2;
    float3 pos, weightedPos, worldPos;
    float3 normal;
    float4 projPos;

    pos = vs_in.p.xyz;

#if ENABLE_SKELETAL_ANIMATION
    //FIXME: Should be copy!!! UGLY HACK!!!
    index0      = (vs_in.uv2.x - hack_base_index) / 3;
    weight0     = vs_in.uv2.y;
    weightedPos = weight0 * mul(uBoneXF[index0], float4(pos, 1.0));

    //FIXME: Should be copy!!! UGLY HACK!!!
    index1       = (vs_in.uv2.z - hack_base_index) / 3;
    weight1      = vs_in.uv2.w;
    weightedPos += weight1 * mul(uBoneXF[index1], float4(pos, 1.0));

    weight2 = 1.0 - weight0 - weight1;
    weightedPos += weight2 * pos;

    worldPos = mul(uW, float4(weightedPos, 1.0));
    projPos  = mul(uWVP, float4(weightedPos, 1.0));
    normal   = mul(uBoneXF[index0], float4(vs_in.n, 0.0));
    normal   = mul(uW, float4(normal, 0.0));
#else
    worldPos = mul(uW, float4(pos, 1.0));
    projPos  = mul(uWVP, float4(pos, 1.0));
    normal   = mul(uW, float4(vs_in.n, 0.0));
#endif

    vs_out.pos = projPos;
    vs_out.diffuse = uDiffuse;
    vs_out.uv0 = vs_in.uv0 + uTexOffset; //apply texture offset
    vs_out.uv1 = vs_in.uv1;
#if ENABLE_LIGHTING
    vs_out.lighting = calcLighting(worldPos, normal);
    #if ENABLE_FAKE_LIGHT
        vs_out.uv2 = mul(uFakeLightMatrix, projPos);
    #endif
#endif
#if TEXTURE_SOURCE == TEXSRC_REFLECTION
    float3 v = uCameraPos - worldPos;
    vs_out.uv3 = v - (2 * dot(v, normal)) * normal;
#elif TEXTURE_SOURCE == TEXSRC_LOCAL_REFLECTION
    vs_out.uv3 = mul(uReflMatrix, projPos);
#endif
#if ENABLE_FOG
    vs_out.fogFactor = saturate((worldPos.y - uFog.x) * uFog.y); //Make a mad
#endif

    return vs_out;
}
