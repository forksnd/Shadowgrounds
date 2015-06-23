// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include "Storm3D_Line.h"
#include "storm3d.h"
#include <cassert>

#include "Storm3D_ShaderManager.h"
#include "VertexFormats.h"
#include "../../util/Debug_MemoryManager.h"

extern int storm3d_dip_calls;

Storm3D_Line::Storm3D_Line(Storm3D *storm_)
:	storm(storm_)
{
	thickness = 0.5f;
	color = RGB(255,255,255);

	rebuild_indices = true;
	rebuild_vertices = true;
	pixel_line = false;
}

Storm3D_Line::~Storm3D_Line()
{
	if(storm)
		storm->lines.erase(this);
}

void Storm3D_Line::AddPoint(const Vector &position)
{
	points.push_back(position);

	rebuild_indices = true;
	rebuild_vertices = true;
}

int Storm3D_Line::GetPointCount()
{
	return points.size();
}

void Storm3D_Line::RemovePoint(int index)
{
	assert((index >= 0) && (index < static_cast<int> (points.size()) ));
	points.erase(points.begin() + index);

	rebuild_indices = true;
	rebuild_vertices = true;
}
	
void Storm3D_Line::SetThickness(float thickness)
{
	this->thickness = thickness;
	if(thickness < 0)
		pixel_line = true;
	else
		pixel_line = false;

	rebuild_indices = true;
	rebuild_vertices = true;
}

void Storm3D_Line::SetColor(int color)
{
	this->color = color;
	rebuild_indices = true;
	rebuild_vertices = true;
}

void Storm3D_Line::Render(GfxDevice& device)
{
	if(points.size() < 2)
		return;
	
	int faces = (points.size() - 1) * 2;
	int vertices = (points.size() - 1) * 4;

	rebuild_vertices = false;
	rebuild_indices = false;

	device.SetVertexShader(0);
	device.SetFVF(FVF_P3D);

    // Render
    if (!pixel_line)
    {
        uint16_t* ip = 0;
        UINT      baseIdx = 0;

        device.lockDynIdx16(faces*3, &ip, &baseIdx);
        for (int i = 0; i < static_cast<int> (points.size() - 1); ++i)
        {
            // First face
            *ip++ = i * 4 + 0;
            *ip++ = i * 4 + 1;
            *ip++ = i * 4 + 2;

            // Second face
            *ip++ = i * 4 + 1;
            *ip++ = i * 4 + 3;
            *ip++ = i * 4 + 2;
        }
        device.unlockDynIdx16();

        UINT        baseVtx = 0;
        Vertex_P3D* v = 0;

        device.lockDynVtx<Vertex_P3D>(vertices, &v, &baseVtx);
        // last corner points (next part will be connected to these)
        // -jpk
        Vertex_P3D *lastv1 = NULL;
        Vertex_P3D *lastv2 = NULL;

        for (unsigned int i = 0; i < points.size() - 1; ++i)
        {
            Vector direction = points[i];
            direction -= points[i + 1];

            if (fabs(direction.x) < 0.0001f
                && fabs(direction.y) < 0.0001f
                && fabs(direction.z) < 0.0001f)
            {
                // to avoid division by zero
                direction.x = 0.1f;
            }

            direction.Normalize();
            Vector side = direction.GetCrossWith(Vector(0, 1, 0));

            v->p = (i > 1) ? lastv1->p : points[i] + (side * .5f * thickness);
            v->d = color;
            ++v;

            v->p = (i > 1) ? lastv2->p : points[i] - (side * .5f * thickness);
            v->d = color;
            ++v;

            v->p = points[i + 1] + (side * .5f * thickness) + (direction * .5f * -thickness);
            v->d = color;
            lastv1 = v;
            ++v;

            v->p = points[i + 1] - (side * .5f * thickness) + (direction * .5f * -thickness);
            v->d = color;
            lastv2 = v;
            ++v;
        }
        device.unlockDynVtx();

        device.SetDynVtxBuffer<Vertex_P3D>();
        device.SetDynIdx16Buffer();
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVtx, 0, vertices, baseIdx, faces);
        ++storm3d_dip_calls;
    }
    else
    {
        UINT        baseVtx = 0;
        Vertex_P3D* v = 0;

        device.lockDynVtx<Vertex_P3D>(points.size(), &v, &baseVtx);
        for (unsigned int i = 0; i < points.size(); i++)
        {
            v[i].p = points[i];
            v[i].d = color;
        }
        device.unlockDynVtx();

        device.SetDynVtxBuffer<Vertex_P3D>();
        device.DrawPrimitive(D3DPT_LINELIST, baseVtx, points.size() / 2);
        ++storm3d_dip_calls;
    }
}

void Storm3D_Line::releaseDynamicResources()
{
}

void Storm3D_Line::recreateDynamicResources()
{
}
