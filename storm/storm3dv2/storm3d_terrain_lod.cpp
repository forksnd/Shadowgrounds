// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <vector>

#include "storm3d_terrain_lod.h"
#include "storm3d_terrain_utils.h"
#include "storm3d.h"
#include "storm3d_scene.h"
#include <atlbase.h>
#include <d3d9.h>

#include "../../util/Debug_MemoryManager.h"
//#include "../exporters/shared/nvtristrip.h"

static const int LOD_AMOUNT = 3;

static const int CENTER_BLOCK_SIZE = IStorm3D_Terrain::CENTER_BLOCK_SIZE;
static const int CENTER_BLOCK_AMOUNT = 16;

static_assert(CENTER_BLOCK_SIZE == 2, "ASSERT FAILED: CENTER_BLOCK_SIZE == 2");

struct LodIndexBuffer
{
    uint16_t allocId;
    int faceAmount;
    int lodFactor;
    int baseIndex;

    enum Type
    {
        FullBuffer,
        CenterBuffer,
        LinkBuffer
    };

    LodIndexBuffer():
        lodFactor(1),
        faceAmount(0),
        allocId(0),
        baseIndex(0)
    {
    }

    void cleanup(IndexStorage16& storage)
    {
        storage.free(allocId);
    }

    void generate(IndexStorage16& storage, Type type, int resolution, int lodFactor_, bool leftLod, bool rightLod, bool upLod, bool downLod, unsigned char *clipBuffer)
    {
        faceAmount = 0;
        lodFactor = lodFactor_;

        int idxCount = (resolution - 1) * (resolution - 1) * 6;

        allocId = storage.alloc(idxCount);
        if (!allocId)
            return;

        uint16_t* indices = storage.lock(allocId);

        if (type == FullBuffer)
            generateCenter(indices, resolution, clipBuffer);

        generateLink(indices, resolution, leftLod, rightLod, upLod, downLod, clipBuffer);

        storage.unlock();

        faceAmount /= 3;
        baseIndex = storage.baseIndex(allocId);
    }

    void generateCenters(IndexStorage16& storage, int resolution, int lodFactor_, int mask, unsigned char *clipBuffer)
    {
        faceAmount = 0;
        lodFactor = lodFactor_;

        int idxCount = (resolution - lodFactor) * (resolution - lodFactor);

        allocId = storage.alloc(idxCount);
        if (!allocId)
            return;

        uint16_t* buffer = storage.lock(allocId);

        int startVertex = 1;
        int endVertex = resolution - 1;
        int delta = (endVertex - startVertex) / lodFactor;

        // ToDo: general version
        assert(mask >= 0);
        assert(mask <= CENTER_BLOCK_AMOUNT);

        if (mask & 1)
        {
            int x = 0;
            int y = 0;

            int x1 = (startVertex + x * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y1 = (startVertex + y * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int x2 = (startVertex + (x + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y2 = (startVertex + (y + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;

            generateTriangles(buffer, resolution, x1, y1, x2, y2, clipBuffer);
        }
        if (mask & 2)
        {
            int x = 1;
            int y = 0;

            int x1 = (startVertex + x * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y1 = (startVertex + y * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int x2 = (startVertex + (x + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y2 = (startVertex + (y + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;

            generateTriangles(buffer, resolution, x1, y1, x2, y2, clipBuffer);
        }
        if (mask & 4)
        {
            int x = 0;
            int y = 1;

            int x1 = (startVertex + x * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y1 = (startVertex + y * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int x2 = (startVertex + (x + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y2 = (startVertex + (y + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;

            generateTriangles(buffer, resolution, x1, y1, x2, y2, clipBuffer);
        }
        if (mask & 8)
        {
            int x = 1;
            int y = 1;

            int x1 = (startVertex + x * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y1 = (startVertex + y * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int x2 = (startVertex + (x + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;
            int y2 = (startVertex + (y + 1) * delta) / CENTER_BLOCK_SIZE * lodFactor;

            generateTriangles(buffer, resolution, x1, y1, x2, y2, clipBuffer);
        }

        storage.unlock();

        faceAmount /= 3;
        baseIndex = storage.baseIndex(allocId);
    }

private:
    void generateCenter(unsigned short *buffer, int resolution, unsigned char *clipBuffer)
    {
        int startVertex = lodFactor;
        int endVertex = resolution - lodFactor - 1;

        generateTriangles(buffer, resolution, startVertex, startVertex, endVertex, endVertex, clipBuffer);
    }

    void generateLink(unsigned short *buffer, int resolution, bool leftLod, bool rightLod, bool upLod, bool downLod, unsigned char *clipBuffer)
    {
        int startVertex = lodFactor;
        int endVertex = resolution - lodFactor - 1;

        if (!leftLod)
        {
            generateTriangles(buffer, resolution, 0, startVertex, startVertex, endVertex, clipBuffer);

            createFace(buffer, -1, 0, lodFactor * resolution, lodFactor * resolution + lodFactor, clipBuffer);
            createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution, (resolution - 1) * resolution, (resolution - 1 - lodFactor) * resolution + lodFactor, clipBuffer);

            //createFace(buffer, -1, 0, lodFactor * resolution, lodFactor);
            //createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution, (resolution - 1 ) * resolution, (resolution - 1 - lodFactor) * resolution + lodFactor);
        }
        else
            generateHorizontal(buffer, resolution, startVertex, -lodFactor, clipBuffer);

        if (!rightLod)
        {
            generateTriangles(buffer, resolution, endVertex, startVertex, resolution - 1, endVertex, clipBuffer);

            createFace(buffer, -1, resolution - 1, resolution * lodFactor + resolution - 1 - lodFactor, resolution * lodFactor + resolution - 1, clipBuffer);
            createFace(buffer, -1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1, clipBuffer);

            //createFace(buffer, -1, resolution - 1, resolution * lodFactor + resolution - 1 - lodFactor, resolution * lodFactor + resolution - 1);
            //createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1);
        }
        else
            generateHorizontal(buffer, resolution, endVertex, lodFactor, clipBuffer);

        if (!upLod)
        {
            generateTriangles(buffer, resolution, startVertex, 0, endVertex, startVertex, clipBuffer);

            createFace(buffer, -1, 0, resolution * lodFactor + lodFactor, lodFactor, clipBuffer);
            createFace(buffer, -1, resolution - 1, endVertex, lodFactor * resolution + endVertex, clipBuffer);

            //createFace(buffer, -1, resolution * lodFactor, resolution * lodFactor + lodFactor, lodFactor);
            //createFace(buffer, -1, resolution - 1, endVertex, lodFactor * resolution + endVertex);
        }
        else
            generateVertical(buffer, resolution, startVertex, -lodFactor, clipBuffer);

        if (!downLod)
        {
            generateTriangles(buffer, resolution, startVertex, endVertex, endVertex, resolution - 1, clipBuffer);

            createFace(buffer, -1, (resolution - 1) * resolution, (resolution - 1) * resolution + lodFactor, (resolution - 1 - lodFactor) * resolution + lodFactor, clipBuffer);
            createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor, clipBuffer);

            //createFace(buffer, -1, (resolution - 1) * resolution, (resolution - 1) * resolution + lodFactor, (resolution - 1 - lodFactor) * resolution + lodFactor);
            //createFace(buffer, -1, (resolution - 1) * resolution + resolution - 1 - lodFactor, (resolution - 1 - lodFactor) * resolution + resolution - 1, (resolution - 1 - lodFactor) * resolution + resolution - 1 - lodFactor);
        }
        else
            generateVertical(buffer, resolution, endVertex, lodFactor, clipBuffer);
    }

    void generateTriangles(unsigned short *buffer, int resolution, int x1, int y1, int x2, int y2, unsigned char *clipBuffer)
    {
        for (int y = y1; y < y2; y += lodFactor)
            for (int x = x1; x < x2; x += lodFactor)
            {
                int position = y * resolution + x;

                //int f1 = position + lodFactor;
                //int f2 = position;
                //int f3 = position + resolution * lodFactor;
                int f1 = position + lodFactor + resolution * lodFactor;
                int f2 = position;
                int f3 = position + resolution * lodFactor;
                if (!clipped(f1, f2, f3, clipBuffer))
                {
                    buffer[faceAmount++] = f1;
                    buffer[faceAmount++] = f2;
                    buffer[faceAmount++] = f3;
                }

                //int f4 = position + resolution * lodFactor;
                //int f5 = position + resolution * lodFactor + lodFactor;
                //int f6 = position + lodFactor;
                int f4 = position;
                int f5 = position + resolution * lodFactor + lodFactor;
                int f6 = position + lodFactor;
                if (!clipped(f1, f2, f3, clipBuffer))
                {
                    buffer[faceAmount++] = f4;
                    buffer[faceAmount++] = f5;
                    buffer[faceAmount++] = f6;
                }
            }
    }

    void generateHorizontal(unsigned short *buffer, int resolution, int column, int direction, unsigned char *clipBuffer)
    {
        for (int y = lodFactor; y < resolution - lodFactor; y += 2 * lodFactor)
        {
            int position = y * resolution + column;

            //if(y > 0)
            {
                int f1 = position;
                int f2 = position + direction - resolution * lodFactor;
                int f3 = position + direction + resolution * lodFactor;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);
            }

            if (y < resolution - 2 * lodFactor)
            {
                int f1 = position;
                int f2 = position + direction + resolution * lodFactor;
                int f3 = position + resolution * lodFactor;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);

                f1 = position + resolution * lodFactor;
                f2 = position + direction + resolution * lodFactor;
                f3 = position + resolution * lodFactor * 2;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);
            }
        }
    }

    void generateVertical(unsigned short *buffer, int resolution, int row, int direction, unsigned char *clipBuffer)
    {
        for (int x = lodFactor; x < resolution - lodFactor; x += 2 * lodFactor)
        {
            int position = row * resolution + x;

            //if(x > 0)
            {
                int f1 = position;
                int f2 = position + lodFactor + direction * resolution;
                int f3 = position - lodFactor + direction * resolution;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);
            }

            if (x < resolution - 2 * lodFactor)
            {
                int f1 = position;
                int f2 = position + lodFactor;
                int f3 = position + lodFactor + direction * resolution;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);

                f1 = position + lodFactor;
                f2 = position + 2 * lodFactor;
                f3 = position + lodFactor + direction * resolution;

                createFace(buffer, direction, f1, f2, f3, clipBuffer);
            }
        }
    }

    static bool clipped(int f1, int f2, int f3, unsigned char *clipBuffer)
    {
        if (!clipBuffer) return false;

        bool f1Clip = clipBuffer[f1] != 0;
        bool f2Clip = clipBuffer[f2] != 0;
        bool f3Clip = clipBuffer[f3] != 0;

        return f1Clip && f2Clip && f3Clip;
    }

    void createFace(unsigned short *buffer, int direction, int f1, int f2, int f3, unsigned char *clipBuffer)
    {
        if (clipped(f1, f2, f3, clipBuffer))
            return;

        if (direction < 0)
        {
            buffer[faceAmount++] = f1;
            buffer[faceAmount++] = f2;
            buffer[faceAmount++] = f3;
        }
        else
        {
            buffer[faceAmount++] = f2;
            buffer[faceAmount++] = f1;
            buffer[faceAmount++] = f3;
        }
    }
};

struct LOD
{
	// left (no lod / lod), right, up, down

	// Center + links
	LodIndexBuffer fullBuffers[16];
	// Links only
	LodIndexBuffer linkBuffers[16];

	// Center in 16 pieces (2x2 chunks, same ordering)
	LodIndexBuffer centerBuffers[16];

	void generate(IndexStorage16& storage, int resolution, int lod, unsigned char *clipBuffer)
	{
		for(int l = 0; l < 2; ++l)
		for(int r = 0; r < 2; ++r)
		for(int u = 0; u < 2; ++u)
		for(int d = 0; d < 2; ++d)
		{
			bool leftLod = l == 1;
			bool rightLod = r == 1;
			bool upLod = u == 1;
			bool downLod = d == 1;

			int index = l + r*2 + u*4 + d*8;
				
			fullBuffers[index].generate(storage, LodIndexBuffer::FullBuffer, resolution, lod, leftLod, rightLod, upLod, downLod, clipBuffer);
			linkBuffers[index].generate(storage, LodIndexBuffer::LinkBuffer, resolution, lod, leftLod, rightLod, upLod, downLod, clipBuffer);
		}

		for(int i = 0; i < CENTER_BLOCK_AMOUNT; ++i)
			centerBuffers[i].generateCenters(storage, resolution, lod,  i, clipBuffer);
	}

    void cleanup(IndexStorage16& storage)
    {
        for (auto& buf: fullBuffers)   buf.cleanup(storage);
        for (auto& buf: linkBuffers)   buf.cleanup(storage);
        for (auto& buf: centerBuffers) buf.cleanup(storage);
    }
};

struct Storm3D_TerrainLodData
{
	Storm3D &storm;
	LOD lodBuffers[LOD_AMOUNT];

	int maxVertex;
	float blockSize;

	Storm3D_TerrainLodData(Storm3D &storm_)
	:	storm(storm_),
		maxVertex(0),
		blockSize(1)
	{
	}

    ~Storm3D_TerrainLodData()
    {
        IndexStorage16& storage = storm.getIndexStorage16();
        for (auto& lod: lodBuffers) lod.cleanup(storage);
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

void Storm3D_TerrainLod::generate(int resolution, unsigned char *clipBuffer)
{
    IndexStorage16& storage = data->storm.getIndexStorage16();
	data->maxVertex = (resolution * resolution);
	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		LOD &lod = data->lodBuffers[i];
		lod.generate(storage, resolution, 1 << i, clipBuffer);
	}
}

void Storm3D_TerrainLod::setBlockRadius(float size)
{
	data->blockSize = size;
}

void Storm3D_TerrainLod::render(Storm3D_Scene &scene, int subMask, float range, float rangeX1, float rangeY1, float rangeX2, float rangeY2)
{
	GfxDevice &device = data->storm.GetD3DDevice();

	int lod = data->getLOD(range);

	bool lodX1 = data->getLOD(rangeX1) > lod;
	bool lodY1 = data->getLOD(rangeY1) > lod;
	bool lodX2 = data->getLOD(rangeX2) > lod;
	bool lodY2 = data->getLOD(rangeY2) > lod;

	int index = 0;
	if(lodX1)
		index += 1;
	if(lodX2)
		index += 2;
	if(lodY1)
		index += 4;
	if(lodY2)
		index += 8;

    IndexStorage16& indices = data->storm.getIndexStorage16();
    device.SetIndices(indices.indices);

	// Render as whole
	if(subMask == -1)
	{
		LodIndexBuffer &indexBuffer = data->lodBuffers[lod].fullBuffers[index];
        assert(indexBuffer.faceAmount);
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, data->maxVertex, indexBuffer.baseIndex, indexBuffer.faceAmount);

		scene.AddPolyCounter(indexBuffer.faceAmount);
	}
	else
	{
		LodIndexBuffer &indexBuffer = data->lodBuffers[lod].centerBuffers[subMask];
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, data->maxVertex, indexBuffer.baseIndex, indexBuffer.faceAmount);

		scene.AddPolyCounter(indexBuffer.faceAmount);

		LodIndexBuffer &linkBuffer = data->lodBuffers[lod].linkBuffers[index];
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, data->maxVertex, linkBuffer.baseIndex, linkBuffer.faceAmount);

		scene.AddPolyCounter(linkBuffer.faceAmount);
	}
}
