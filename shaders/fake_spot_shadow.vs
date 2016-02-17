struct VS_IN
{
    float4 p  : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float  dist      : COLOR0;
    float4 uv0       : TEXCOORD0;
    float4 uv1       : TEXCOORD1;
    float4 uv2       : TEXCOORD2;
    float4 uv3       : TEXCOORD3;
};

// World x View x Projection matrix
float4x4 uWVP        : register(c0);

// Shadow projection matrix
float4x4 uShadowProj  : register(c12);

// Shadow texture sampling offset
float4   uOffset0  : register(c5);
float4   uOffset1  : register(c6);
float4   uOffset2  : register(c7);
float4   uOffset3  : register(c8);

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    //dcl_position v0
    //dcl_texcoord0 v1

    //; ** POSITION **
    //; Transform position to clip space
    //m4x4 r0, v0, c[0]
    //mov oPos, r0
    vs_out.pos  = mul(uWVP, float4(vs_in.p.xyz, 1.0));
    //mov oD0, v1.x
    vs_out.dist = vs_in.uv.x;

    //; ** TEXTURING **
    //m4x4 r1, v0, c[12]
    float4 uvShadow = mul(uShadowProj, float4(vs_in.p.xyz, 1.0));

    //add oT0, r1, c[5]
    //add oT1, r1, c[6]
    //add oT2, r1, c[7]
    //add oT3, r1, c[8]
    vs_out.uv0 = uvShadow + uOffset0;
    vs_out.uv1 = uvShadow + uOffset1;
    vs_out.uv2 = uvShadow + uOffset2;
    vs_out.uv3 = uvShadow + uOffset3;


    return vs_out;
}