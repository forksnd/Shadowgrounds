
#include "precompiled.h"

#include "BlinkerManager.h"
#include "BuildingBlinker.h"
#include <vector>

namespace util {

struct BlinkerManager::Data
{
	std::vector<boost::shared_ptr<BuildingBlinker> > blinkers;

	void update(int timeDelta)
	{
		for(unsigned int i = 0; i < blinkers.size(); ++i)
			blinkers[i]->update(timeDelta);
	}
};

BlinkerManager::BlinkerManager()
{
    data = new Data();
}

BlinkerManager::~BlinkerManager()
{
    assert(data);
    delete data;
}

void BlinkerManager::addBlinker(boost::shared_ptr<BuildingBlinker> blinker)
{
	if(!blinker)
	{
		assert(!"Whopps");
		return;
	}

	data->blinkers.push_back(blinker);
}

void BlinkerManager::update(int timeDelta)
{
	data->update(timeDelta);
}

} // util
