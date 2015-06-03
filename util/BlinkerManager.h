#ifndef INCLUDED_UTIL_BLINKER_MANAGER_H
#define INCLUDED_UTIL_BLINKER_MANAGER_H

#include <boost/shared_ptr.hpp>

namespace util {

class BuildingBlinker;

class BlinkerManager 
{
	struct Data;
	Data* data;

public:
	BlinkerManager();
	~BlinkerManager();

	void addBlinker(boost::shared_ptr<BuildingBlinker> blinker);
	void update(int timeDelta);
}; 

} // util

#endif
