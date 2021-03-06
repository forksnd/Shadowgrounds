#ifndef INCLUDED_SPOTLIGHTCALCULATOR_H
#define INCLUDED_SPOTLIGHTCALCULATOR_H

#include <DatatypeDef.h>

class IStorm3D_Terrain;
namespace ui {
	class IVisualObjectData;
} // ui

namespace util {

class SpotLightCalculator
{
	struct Data;
	Data* data;

public:
	SpotLightCalculator(float fov, float range, ui::IVisualObjectData *visualData);
	~SpotLightCalculator();

	// Call often enough, clears ray cache
	void update(const VC3 &position, const VC3 &direction);

	float getLightAmount(const VC3 &position, const IStorm3D_Terrain &terrain, float rayHeight = 1.5f) const;
	ui::IVisualObjectData *getData() const;
};

} // utils

#endif
