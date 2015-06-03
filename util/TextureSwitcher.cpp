
#include "precompiled.h"

#include "TextureSwitcher.h"
#include "TextureCache.h"
#include "../system/Logger.h"
#include <IStorm3D_Texture.h>

#include <string>
#include <map>

using namespace frozenbyte;

namespace util {

struct TextureSwitcher::Data
{
	TextureCache &cache;
	std::map<std::string, IStorm3D_Texture *> textures;

	Data(TextureCache &cache_)
	:	cache(cache_)
	{
	}

	std::map<std::string, IStorm3D_Texture *>::iterator getTexture(const char *name)
	{
		std::string fileName = name;

		std::map<std::string, IStorm3D_Texture *>::iterator it = textures.find(fileName);
		if(it == textures.end())
		{
			textures[fileName] = cache.getTexture(name);
			it = textures.find(fileName);
		}

		return it;
	}

	void switchTexture(const char *fromName, const char *toName)
	{
		if(!fromName || !toName)
		{
			assert(!"Null name given.");
			return;
		}

		std::map<std::string, IStorm3D_Texture *>::iterator fromIt = getTexture(fromName);
		std::map<std::string, IStorm3D_Texture *>::iterator toIt = getTexture(toName);

		IStorm3D_Texture *from = fromIt->second;
		IStorm3D_Texture *to = toIt->second;

		if(!from || !to)
		{
			std::string message = "TextureSwitcher: Can't swap ";
			message += fromName;
			message += " and ";
			message += toName;

			Logger::getInstance()->warning(message.c_str());
			return;
		}

		from->swapTexture(to);

		fromIt->second = to;
		toIt->second = from;
	}
};

TextureSwitcher::TextureSwitcher(TextureCache &cache)
{
	data = new Data(cache);
}

TextureSwitcher::~TextureSwitcher()
{
    assert(data);
    delete data;
}

void TextureSwitcher::switchTexture(const char *from, const char *to)
{
	data->switchTexture(from, to);
}

} // util
