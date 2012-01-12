// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_EDITOR_STRING_CONVERSIONS
#define INCLUDED_EDITOR_STRING_CONVERSIONS

#ifndef INCLUDED_STRING
#define INCLUDED_STRING
#include <string>
#endif

namespace frozenbyte {
namespace editor {

template<class T> inline T convertFromString(const std::string &string, T defaultValue);

template<> inline float convertFromString(const std::string &string, float defaultValue)
{
    float value = defaultValue;
    sscanf(string.c_str(), "%f", &value);
    return value;
}

template<> inline int convertFromString(const std::string &string, int defaultValue)
{
    int value = defaultValue;
    sscanf(string.c_str(), "%d", &value);
    return value;
}

template<> inline bool convertFromString(const std::string &string, bool defaultValue)
{
    int value = defaultValue;
    sscanf(string.c_str(), "%d", &value);
    return value!=0;
}

template<class T>
inline std::string convertToString(T value)
{
	std::stringstream stream;
	std::string result;

	stream << std::fixed;
	if(!(stream << value) || !(stream >> result) || !(stream >> std::ws).eof())
		return "";

	return result;
}

} // end of namespace editor
} // end of namespace frozenbyte

#endif
