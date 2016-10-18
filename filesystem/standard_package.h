// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_FILESYSTEM_STANDARD_PACKAGE
#define INCLUDED_FILESYSTEM_STANDARD_PACKAGE

#include "ifile_package.h"

namespace frozenbyte {
namespace filesystem {

class IFileList;

class StandardPackage: public IFilePackage
{
public:
	StandardPackage();
	~StandardPackage();

	void findFiles(const std::string &dir, const std::string &extension, IFileList &result);
	InputStream getFile(const std::string &fileName);
	unsigned int getCrc(const std::string &fileName);
};

} // end of namespace filesystem
} // end of namespace frozenbyte

#endif
