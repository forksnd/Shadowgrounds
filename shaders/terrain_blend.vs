uniform float4x4  uWVP : register(c0);

struct VS_IN
{
    float3 p     : POSITION;
    float3 n     : NORMAL;
    float2 uv0  : TEXCOORD0;
    float2 uv1  : TEXCOORD1;
    float2 uv2  : TEXCOORD2;
};

struct VS_OUT
{
    float4 p     : POSITION;
    float2 uv0  : TEXCOORD0;
    float2 uv1  : TEXCOORD1;
    float2 uv2  : TEXCOORD2;
};

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    vs_out.p = mul(uWVP, float4(vs_in.p, 1.0));

    vs_out.uv0 = vs_in.uv0;
    vs_out.uv1 = vs_in.uv1;
    vs_out.uv2 = vs_in.uv2;

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
;   -> c[11]   -> Fog start/end/range (.w 1.f)

; Vertex data:
;   -> v0 -> position
;   -> v3 -> normal
;   -> v7,v8,v9 -> texture coordinates

dcl_position v0
dcl_normal v3
dcl_texcoord0 v4
dcl_texcoord1 v5
dcl_texcoord2 v6

; ** POSITION **
; Transform position to clip space
m4x4 oPos, v0, c[0]

; ** TEXTURING **
; Move texture coordinates
mov oT0.xy, v4.xy
mov oT1.xy, v5.xy
mov oT2.xy, v6.xy
*/