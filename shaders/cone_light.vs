struct VS_IN
{
    float4 p  : POSITION;
    float3 n  : NORMAL;
    float4 d  : COLOR;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float4 uv0       : TEXCOORD0;
    float2 uv1       : TEXCOORD1;
    float2 uv2       : TEXCOORD2;
    float  uv3       : TEXCOORD3;
};

// World x View x Projection matrix
float4x4 uWVP         : register(c0);

float3x4 uW     : register(c4);

// Shadow texture matrix
float4x4 uShadowMat  : register(c8);
float3x4 uConeTextureMat1 : register(c12);
float3x4 uConeTextureMat2 : register(c16);

// Diffuse color
float4 uDiffuse     : register(c21);
float4 uLightSource : register(c23); 
float4 uBias        : register(c24);

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    // ; ** POSITION **
    // ; Transform position to clip space
    // m4x4 r0, v0, c[0]
    // mov oPos, r0

    // ; To spots space
    // m4x4 r1, v0, c[4]
    // add r1.z, r1.z, c[10].w
    // m4x4 r2, v0, c[12]
    // add r2.z, r2.z, c[10].w
    // mov oT0, r2

    // mul r1.z, r1.z, c[11].w
    // mov oT2.xy, r1.z
    // m4x3 r4, v1, c[16]
    // mov oT3.xyz, r4
    // m4x3 r4, v1, c[19]
    // mov oT1.xyz, r4

    // ;dp3 r0, v2.xyz, c[10].xyz
    // ;max r0.x, r0.x, c[10].w
    // ; Spot color
    // ;mul r0, r0.x, c[9]

    // mov r0, c[9]

    // mov oD0, r0

    float4 bias = float4(0, 0, uBias.w, 0);
    float4 pos = float4(vs_in.p.xyz, 1.0);

    vs_out.pos = mul(uWVP, pos);
    
    float4 shadowPos = mul(uShadowMat, pos);
    vs_out.uv0 = shadowPos;

    vs_out.uv1 = mul(uConeTextureMat1, float4(vs_in.uv, 0.0, 1.0)).xy;
    vs_out.uv2 = mul(uConeTextureMat2, float4(vs_in.uv, 0.0, 1.0)).xy;
 
    vs_out.uv3 = shadowPos.z*uLightSource.w;

    vs_out.diffuse = uDiffuse;

    return vs_out;
}
