#pragma once

#include <IStorm3D_Line.h>
#include <d3d9.h>
#include <vector>
#include <atlbase.h>

class Storm3D;
namespace gfx
{
    struct Device;
}

class Storm3D_Line: public IStorm3D_Line
{
	Storm3D *storm;

	bool pixel_line;

	std::vector<Vector> points;
	float thickness;
	int color;

	bool rebuild_indices;
	bool rebuild_vertices;

public:
	explicit Storm3D_Line(Storm3D *storm_);
	~Storm3D_Line();

	// Add as many as you like (>= 2)
	void AddPoint(const Vector &position);
	int GetPointCount();
	void RemovePoint(int index);
	
	// Units in world space
	void SetThickness(float thickness);
	void SetColor(int color);

	// Storm-stuff (expose this and remove that cast?)
	void Render(gfx::Device& device);

	void releaseDynamicResources();
	void recreateDynamicResources();
};
