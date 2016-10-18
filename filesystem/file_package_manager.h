// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_FILESYSTEM_FILE_PACKAGE_MANAGER
#define INCLUDED_FILESYSTEM_FILE_PACKAGE_MANAGER

#include <memory>

#include "ifile_package.h"

namespace frozenbyte {
namespace filesystem {

class IFileList;
struct FilePackageManagerData;

class FilePackageManager
{
	FilePackageManagerData* data;

public:
	FilePackageManager();
	~FilePackageManager();

	void addPackage(std::shared_ptr<IFilePackage> filePackage, int priority);	
	std::shared_ptr<IFileList> findFiles(const std::string &dir, const std::string &extension, bool caseSensitive = false);
	InputStream getFile(const std::string &fileName);
	unsigned int getCrc(const std::string &fileName);

	void setInputStreamErrorReporting(bool logNonExisting);

	static FilePackageManager &getInstance();
	static void setInstancePtr(FilePackageManager *instance);

};

} // end of namespace filesystem
} // end of namespace frozenbyte

#endif
