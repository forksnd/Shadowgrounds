#ifndef INCLUDED_UTIL_TEXTURE_SWITCHER_H
#define INCLUDED_UTIL_TEXTURE_SWITCHER_H

namespace frozenbyte {
	class TextureCache;
}

namespace util {

class TextureSwitcher 
{
	struct Data;
	Data* data;

public:
	explicit TextureSwitcher(frozenbyte::TextureCache &cache);
	~TextureSwitcher();

	void switchTexture(const char *from, const char *to);
}; 

} // util

#endif
