
#include "precompiled.h"

#undef NDEBUG

#ifdef _MSC_VER
#pragma warning(disable:4103)
#pragma warning(disable:4786)
#endif

#include "assert.h"
#include "../system/Logger.h"

#include <string>

namespace frozenbyte {
namespace {

	void removeMessages()
	{
	}

} // unnamed

void assertImp(const char *predicateString, const char *file, int line)
{
	std::string error = predicateString;
	error += " (";
	error += file;
	error += ", ";
	error += std::to_string(line);
	error += ")";

	Logger::getInstance()->error(error.c_str());
	removeMessages();

	// can't use the actual assert clause here, because it 
	// has been lost in the function call (a macro assert would be 
	// the only possible solution, but that would cause problems
	// with the #undef NDEBUG in the header...)
#ifdef FB_TESTBUILD
	assert(!"testbuild fb_assert encountered.");
#endif
}

} // frozenbyte
