#pragma once
// Double for es-core/src/SystemConf.h: an in-memory map.
#include <string>
class SystemConf
{
public:
	static SystemConf* getInstance();
	std::string get(const std::string& key);
	bool getBool(const std::string& key, bool def = false);
};
