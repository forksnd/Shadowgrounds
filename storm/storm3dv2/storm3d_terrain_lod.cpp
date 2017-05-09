// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <vector>

#include "storm3d_terrain_lod.h"
#include "storm3d_terrain_utils.h"
#include "storm3d.h"
#include "storm3d_scene.h"
#include <d3d9.h>

#include "../../util/Debug_MemoryManager.h"
//#include "../exporters/shared/nvtristrip.h"

enum
{
    LOD_AMOUNT = 3,
    MAX_LOD_VARIANTS = 16,

    LOD_VARIANT_LEFT = 1,
    LOD_VARIANT_RIGHT = 2,
    LOD_VARIANT_UP = 4,
    LOD_VARIANT_DOWN = 8,
};

struct Storm3D_TerrainLodData
{
    Storm3D &storm;

    etlsf_alloc_t allocId = ETLSF_INVALID_ID;
    uint32_t lodDIP[LOD_AMOUNT * MAX_LOD_VARIANTS * 2];

    uint32_t maxVertex;
    float blockSize;

    Storm3D_TerrainLodData(Storm3D &storm_)
        : storm(storm_),
        maxVertex(0),
        blockSize(1)
    {
    }

    ~Storm3D_TerrainLodData()
    {
        gfx::IndexStorage16& storage = storm.renderer.getIndexStorage16();
        storage.free(allocId);
    }

    int getLOD(float range)
    {
        return 0;
        /*
        float start = 2.5f * blockSize;
        for(int i = 0; i < LOD_AMOUNT; ++i)
            if(range < i * 1.5 * blockSize + start)
                return i;

        */
        /*
        float start = 2.f * blockSize;
        for(int i = 1; i < LOD_AMOUNT; ++i)
            if(range < i * 1.5 * blockSize + start)
                return i;
        */

        return LOD_AMOUNT - 1;
    }
};

Storm3D_TerrainLod::Storm3D_TerrainLod(Storm3D &storm)
{
    data = new Storm3D_TerrainLodData(storm);
}

Storm3D_TerrainLod::~Storm3D_TerrainLod()
{
    assert(data);
    delete data;
}

static bool clipped(uint32_t f1, uint32_t f2, uint32_t f3, uint8_t* clipBuffer)
{
    if (!clipBuffer) return false;

    bool f1Clip = clipBuffer[f1] != 0;
    bool f2Clip = clipBuffer[f2] != 0;
    bool f3Clip = clipBuffer[f3] != 0;

    return f1Clip && f2Clip && f3Clip;
}

static uint16_t* createFace(uint16_t* buffer, int direction, uint32_t f1, uint32_t f2, uint32_t f3, uint8_t* clipBuffer)
{
    if (clipped(f1, f2, f3, clipBuffer))
        return buffer;

    if (direction < 0)
    {
        *buffer++ = f1;
        *buffer++ = f2;
        *buffer++ = f3;
    }
    else
    {
        *buffer++ = f2;
        *buffer++ = f1;
        *buffer++ = f3;
    }

    return buffer;
}

static uint16_t* generateHorizontal(uint16_t* buffer, uint32_t resolution, uint32_t lodFactor, uint32_t column, int direction, uint8_t* clipBuffer)
{
    for (uint32_t y = lodFactor; y < resolution - lodFactor; y += 2 * lodFactor)
    {
        uint32_t position = y * resolution + column;

        //if(y > 0)
        {
            uint32_t f1 = position;
            uint32_t f2 = position + direction - resolution * lodFactor;
            uint32_t f3 = position + direction + resolution * lodFactor;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);
        }

        if (y < resolution - 2 * lodFactor)
        {
            uint32_t f1 = position;
            uint32_t f2 = position + direction + resolution * lodFactor;
            uint32_t f3 = position + resolution * lodFactor;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);

            f1 = position + resolution * lodFactor;
            f2 = position + direction + resolution * lodFactor;
            f3 = position + resolution * lodFactor * 2;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);
        }
    }

    return buffer;
}

static uint16_t* generateVertical(uint16_t* buffer, uint32_t resolution, uint32_t lodFactor, uint32_t row, int direction, uint8_t* clipBuffer)
{
    for (uint32_t x = lodFactor; x < resolution - lodFactor; x += 2 * lodFactor)
    {
        int position = row * resolution + x;

        //if(x > 0)
        {
            uint32_t f1 = position;
            uint32_t f2 = position + lodFactor + direction * resolution;
            uint32_t f3 = position - lodFactor + direction * resolution;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);
        }

        if (x < resolution - 2 * lodFactor)
        {
            uint32_t f1 = position;
            uint32_t f2 = position + lodFactor;
            uint32_t f3 = position + lodFactor + direction * resolution;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);

            f1 = position + lodFactor;
            f2 = position + 2 * lodFactor;
            f3 = position + lodFactor + direction * resolution;

            buffer = createFace(buffer, direction, f1, f2, f3, clipBuffer);
        }
    }

    return buffer;
}

static uint16_t* generateTriangles(uint16_t* buffer, uint32_t resolution, uint32_t lodFactor, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint8_t* clipBuffer)
{
    for (uint32_t y = y1; y < y2; y += lodFactor)
    {
        for (uint32_t x = x1; x < x2; x += lodFactor)
        {
            uint32_t position = y * resolution + x;

            //int f1 = position + lodFactor;
            //int f2 = position;
            //int f3 = position + resolution * lodFactor;
            uint32_t f1 = position + lodFactor + resolution * lodFactor;
            uint32_t f2 = position;
            uint32_t f3 = position + resolution * lodFactor;
            if (!clipped(f1, f2, f3, clipBuffer))
            {
                *buffer++ = f1;
                *buffer++ = f2;
                *buffer++ = f3;
            }

            //int f4 = position + resolution * lodFactor;
            //int f5 = position + resolution * lodFactor + lodFactor;
            //int f6 = position + lodFactor;
            uint32_t f4 = position;
            uint32_t f5 = position + resolution * lodFactor + lodFactor;
            uint32_t f6 = position + lodFactor;
            if (!clipped(f1, f2, f3, clipBuffer))
            {
                *buffer++ = f4;
                *buffer++ = f5;
                *buffer++ = f6;
            }
        }
    }

    return buffer;
}

static inline uint16_t* generateLODCenter(uint16_t* buffer, uint32_t resolution, uint32_t lodFactor, uint8_t* clipBuffer)
{
    assert(resolution > lodFactor + 1);

    uint32_t start = lodFactor;
    uint32_t end = resolution - lodFactor - 1;

    return generateTriangles(buffer, resolution, lodFactor, start, start, end, end, clipBuffer);
}

static inline uint16_t* generateLODLink(uint16_t* buffer, uint32_t resolution, uint32_t lodFactor, uint32_t lodVariant, uint8_t* clipBuffer)
{
    assert(resolution > lodFactor);

    uint32_t start = lodFactor;
    uint32_t end = resolution - lodFactor - 1;

    if (lodVariant&LOD_VARIANT_LEFT)
    {
        buffer = generateHorizontal(buffer, resolution, lodFactor, start, -(int)lodFactor, clipBuffer);
    }
    else
    {
        buffer = generateTriangles(buffer, resolution, lodFactor, 0, start, start, end, clipBuffer);

        buffer = createFace(buffer, -1, 0, lodFactor * resolution, lodFactor * resolution + lodFactor, clipBuffer);
        buffer = createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution, (resolution - 1) * resolution, (resolution - 1 - lodFactor) * resolution + lodFactor, clipBuffer);

        //buffer = createFace(buffer, -1, 0, lodFactor * resolution, lodFactor);
        //buffer = createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution, (resolution - 1 ) * resolution, (resolution - 1 - lodFactor) * resolution + lodFactor);
    }

    if (lodVariant&LOD_VARIANT_RIGHT)
    {
        buffer = generateHorizontal(buffer, resolution, lodFactor, end, lodFactor, clipBuffer);

    }
    else
    {
        buffer = generateTriangles(buffer, resolution, lodFactor, end, start, resolution - 1, end, clipBuffer);

        buffer = createFace(buffer, -1, resolution - 1, resolution * lodFactor + resolution - 1 - lodFactor, resolution * lodFactor + resolution - 1, clipBuffer);
        buffer = createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1, clipBuffer);

        //buffer = createFace(buffer, -1, resolution - 1, resolution * lodFactor + resolution - 1 - lodFactor, resolution * lodFactor + resolution - 1);
        //buffer = createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1);
    }

    if (lodVariant&LOD_VARIANT_UP)
    {
        buffer = generateVertical(buffer, resolution, lodFactor, start, -(int)lodFactor, clipBuffer);
    }
    else
    {
        buffer = generateTriangles(buffer, resolution, lodFactor, start, 0, end, start, clipBuffer);

        buffer = createFace(buffer, -1, 0, resolution * lodFactor + lodFactor, lodFactor, clipBuffer);
        buffer = createFace(buffer, -1, resolution - 1, end, lodFactor * resolution + end, clipBuffer);

        //buffer = createFace(buffer, -1, resolution * lodFactor, resolution * lodFactor + lodFactor, lodFactor);
        //buffer = createFace(buffer, -1, resolution - 1, end, lodFactor * resolution + end);
    }

    if (lodVariant&LOD_VARIANT_DOWN)
    {
        buffer = generateVertical(buffer, resolution, lodFactor, end, lodFactor, clipBuffer);
    }
    else
    {
        buffer = generateTriangles(buffer, resolution, lodFactor, start, end, end, resolution - 1, clipBuffer);

        buffer = createFace(buffer, -1, (resolution - 1) * resolution, (resolution - 1) * resolution + lodFactor, (resolution - 1 - lodFactor) * resolution + lodFactor, clipBuffer);
        buffer = createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor, clipBuffer);

        //buffer = createFace(buffer, -1, (resolution - 1) * resolution, (resolution - 1) * resolution + lodFactor, (resolution - 1 - lodFactor) * resolution + lodFactor);
        //buffer = createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1 - lodFactor) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor);
    }

    return buffer;
}

static inline uint16_t* generateLODVariant(uint16_t* indices, uint32_t resolution, uint32_t lodFactor, uint32_t lodVariant, uint8_t* clipBuffer)
{
    indices = generateLODCenter(indices, resolution, lodFactor, clipBuffer);
    indices = generateLODLink(indices, resolution, lodFactor, lodVariant, clipBuffer);

    return indices;
}

static inline uint32_t calcLODIndexCount(uint32_t lodCount, uint32_t resolution)
{
    // TODO: optimize!!!
    uint32_t maxIndexCount = (resolution - 1) * (resolution - 1) * 6;
    return lodCount * maxIndexCount * MAX_LOD_VARIANTS;
}

void Storm3D_TerrainLod::generate(int resolution, uint8_t* clipBuffer)
{
    gfx::IndexStorage16& storage = data->storm.renderer.getIndexStorage16();

    uint32_t idxCount = calcLODIndexCount(LOD_AMOUNT, resolution);

    etlsf_alloc_t allocId = storage.alloc(idxCount);
    if (!allocId.value)
        return;

    data->maxVertex = (resolution * resolution);
    data->allocId = allocId;

    uint16_t* indices = storage.lock(allocId);
    uint32_t baseIndex = storage.baseIndex(allocId);
    uint32_t faceCount = 0;
    uint32_t index = 0;
    for (uint32_t lod = 0; lod < LOD_AMOUNT; ++lod)
    {
        for (uint32_t lodVariant = 0; lodVariant < MAX_LOD_VARIANTS; ++lodVariant)
        {
            uint16_t* indicesEnd = generateLODVariant(indices, resolution, 1 << lod, lodVariant, clipBuffer);
            uint32_t indexCount = static_cast<uint32_t>(indicesEnd - indices);
            data->lodDIP[index++] = baseIndex;
            data->lodDIP[index++] = indexCount / 3;
            baseIndex += indexCount;
            indices = indicesEnd;
        }
    }
    storage.unlock();
}

void Storm3D_TerrainLod::setBlockRadius(float size)
{
    data->blockSize = size;
}

void Storm3D_TerrainLod::render(float range, float rangeX1, float rangeY1, float rangeX2, float rangeY2)
{
    gfx::Device &device = data->storm.GetD3DDevice();

    int lod = data->getLOD(range);

    bool lodX1 = data->getLOD(rangeX1) > lod;
    bool lodY1 = data->getLOD(rangeY1) > lod;
    bool lodX2 = data->getLOD(rangeX2) > lod;
    bool lodY2 = data->getLOD(rangeY2) > lod;

    int index = 0;
    if (lodX1)
        index += 1;
    if (lodX2)
        index += 2;
    if (lodY1)
        index += 4;
    if (lodY2)
        index += 8;

    gfx::IndexStorage16& indices = data->storm.renderer.getIndexStorage16();
    device.SetIndices(indices.indices);

    // Render as whole
    uint32_t paramsDIP = lod * (MAX_LOD_VARIANTS * 2) + index * 2;
    uint32_t baseIndex = data->lodDIP[paramsDIP + 0];
    uint32_t faceCount = data->lodDIP[paramsDIP + 1];
    assert(faceCount);
    device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, data->maxVertex, baseIndex, faceCount);
}
