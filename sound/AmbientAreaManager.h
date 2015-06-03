// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef SFX_AMBIENT_AREA_MANAGER_H
#define SFX_AMBIENT_AREA_MANAGER_H

#include <string>

namespace sfx {

class SoundMixer;

class AmbientAreaManager
{
	struct Data;
	Data* data;

public:
	explicit AmbientAreaManager(SoundMixer *mixer);
	~AmbientAreaManager();

	enum AreaType
	{
		Indoor = 0,
		Outdoor = 1
	};

	void loadList(const std::string &file);
	void enterArea(AreaType type);
	void setArea(AreaType type, const std::string &name);
	void setEaxArea(AreaType type, const std::string &name);

	void fadeIn();
	void update(int ms);
	void fadeOut(int ms);

	void enableAmbient();
	void disableAmbient();
	void enableEax();
	void disableEax();
};

} // sfx

#endif
