struct VS_IN
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float2 uv0       : TEXCOORD0;
    float4 uv1       : TEXCOORD1;
    float4 uv2       : TEXCOORD2;
    float  fogFactor : FOG;
};

// World x View x Projection matrix
float4x4 uWVP        : register(c0);

float4x4 uTerrainTextureMatrix : register(c12);
float4x4 uFakeLightMatrix      : register(c16);

float2   uFog       : register(c20);
float4   uDiffuse   : register(c21);
float3   uAmbient   : register(c22);
float3   uSunDir    : register(c23);

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float3 pos, worldPos;
    float3 normal;
    float4 projPos;

    pos = vs_in.p.xyz;
    worldPos = pos;
    projPos  = mul(uWVP, float4(pos, 1.0));
    normal   = vs_in.n;

    float3 lighting = saturate(dot(normal, uSunDir)) + uAmbient;

    vs_out.pos = projPos;
    vs_out.diffuse = float4(lighting, 1) * uDiffuse;
    vs_out.uv0 = vs_in.uv0;
    vs_out.uv1 = mul(uTerrainTextureMatrix, projPos);
    vs_out.uv2 = mul(uFakeLightMatrix, projPos);

    vs_out.fogFactor = saturate((worldPos.y - uFog.x) * uFog.y); //Make a mad

    return vs_out;
}

/*
vs.1.1

; Default shader:
;   -> Transform vertex
;   -> Apply ambient color
;   -> Apply surface color
;   -> Apply directional light
;   -> Apply base texturing 

; Light direction could be transformed to 
; model space and avoid rotating normal.

; Constant declarations:
;   -> c[0..3] -> World x View x Projection matrix
;   -> c[4..7] -> World matrix (first 3 rows)   
;   -> c[7]    -> Ambient color (.w transparency)
;   -> c[8]    -> Diffuse color
;   -> c[9]    -> Light direction (.w 0.f for clamping)
;   -> c[10]   -> Light color
;   -> c[25]   -> Fog

; Vertex data:
;   -> v0 -> position
;   -> v3 -> normal
;   -> v7,v8,v9 -> texture coordinates

dcl_position v0
dcl_normal v3
dcl_texcoord0 v7
dcl_texcoord1 v8
dcl_texcoord2 v9

; Position
m4x4 r0, v0, c[0]
mov oPos, r0

dp3 r1.x, v3.xyz, c[11].xyz
max r1.x, r1.x, c[9].w
add r2.xyz, c[7].xyz, r1.x
mul oD0, r2.xyz, c[8].xyz

mov oT0, v7

m4x4 r1, r0, c[14]
mov oT2, r1
m4x4 r1, r0, c[18]
mov oT3, r1

; Height fog
sub r0, v0.y, c[25].x
mul oFog, r0.x, c[25].y
*/
