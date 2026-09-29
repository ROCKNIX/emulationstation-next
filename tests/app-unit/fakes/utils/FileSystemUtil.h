#pragma once
// Double for es-core/src/utils/FileSystemUtil.h: each test program says
// what exists (the ProxyCards cases: nothing).
#include <string>
namespace Utils { namespace FileSystem {
	bool exists(const std::string& path, bool useCache = true);
	std::string readAllText(const std::string& path);
	bool removeFile(const std::string& path);
	std::string getFileName(const std::string& path);
}}
