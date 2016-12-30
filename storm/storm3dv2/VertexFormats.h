// Copyright 2002-2004 Frozenbyte Ltd.

#pragma once

#include <d3d9.h>
#include <stdint.h>
#include <DatatypeDef.h>

enum FVF //Fixed Vertex Format
{
    FVF_P3NUV2,
    FVF_P3NUV2BW,
    FVF_P3D,
    FVF_P3DUV2,
    FVF_P3UV,
    FVF_P4DUV,
    FVF_P4UV,
    FVF_P2DUV,
    FVF_P2UV,
    FVF_COUNT
};

struct Vertex_P3NUV2
{
    VC3 p;
    VC3 n;
    VC2 uv0;
    VC2 uv1;
};

struct Vertex_P3NUV2BW
{
    VC3 p;
    VC3 n;
    VC2 uv0;
    VC2 uv1;
    VC4 uv2;
};

struct Vertex_P3D
{
    VC3      p;
    uint32_t d;
};

struct Vertex_P3DUV2
{
    VC3      p;
    uint32_t d;
    VC2      uv0;
    VC2      uv1;
};

struct Vertex_P4DUV
{
    VC4      p;
    uint32_t d;
    VC2      uv;
};

struct Vertex_P2DUV
{
    VC2      p;
    uint32_t d;
    VC2      uv;
};
