// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"
#include "VertexFormats.h"

#include "../../util/Debug_MemoryManager.h"

//------------------------------------------------------------------
// Clip2DRectangle
// Returns false if rectancle is completely outside screen
//------------------------------------------------------------------
bool Clip2DRectangle(Storm3D *st,Vertex_P2DUV &ul,Vertex_P2DUV &dr)
{
	// Get Screen coordinates
	Storm3D_SurfaceInfo ss=st->GetScreenSize();
	int sxm=0;
	int sym=0;

	// Cull
	if (ul.p.x>dr.p.x) return false;
	if (ul.p.y>dr.p.y) return false;

	// Check if completely outside screen
	if (ul.p.x>(ss.width-1)) return false;
	if (ul.p.y>(ss.height-1)) return false;
	if (dr.p.x<sxm) return false;
	if (dr.p.y<sym) return false;

	// Test up-left X (ul)
	if (ul.p.x<sxm)
	{
		float tcp=(dr.uv.x-ul.uv.x)/(dr.p.x-ul.p.x);
		ul.uv.x += tcp*(sxm-ul.p.x);
		ul.p.x = float(sxm);
	}

	// Test up-left Y (ul)
	if (ul.p.y<sym)
	{
		float tcp=(dr.uv.y-ul.uv.y)/(dr.p.y-ul.p.y);
		ul.uv.y += tcp*(sym-ul.p.y);
		ul.p.y = float(sym);
	}

	// Test down-right X (dr)
	if (dr.p.x>(ss.width-1))
	{
		float tcp=(dr.uv.x-ul.uv.x)/(dr.p.x-ul.p.x);
		dr.uv.x += tcp*((ss.width-1)-dr.p.x);
		dr.p.x = float(ss.width-1);
	}

	// Test down-right Y (dr)
	if (dr.p.y>(ss.height-1))
	{
		float tcp=(dr.uv.y-ul.uv.y)/(dr.p.y-ul.p.y);
		dr.uv.y += tcp*((ss.height-1)-dr.p.y);
		dr.p.y = float(ss.height-1);
	}

	// Visible
	return true;
}
