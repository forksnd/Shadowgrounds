struct VS_IN
{
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
};

float4 uScaleOffset0 : register(c0);
float4 uScaleOffset1 : register(c1);
float4 uScaleOffset2 : register(c2);
float4 uScaleOffset3 : register(c3);

struct VS_OUT
{
    float4 pos  : POSITION;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
};

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    vs_out.pos = vs_in.pos;

    vs_out.uv0 = vs_in.uv * uScaleOffset0.xy + uScaleOffset0.zw;
    vs_out.uv1 = vs_in.uv * uScaleOffset1.xy + uScaleOffset1.zw;
    vs_out.uv2 = vs_in.uv * uScaleOffset2.xy + uScaleOffset2.zw;
    vs_out.uv3 = vs_in.uv * uScaleOffset3.xy + uScaleOffset3.zw;

    return vs_out;
}
