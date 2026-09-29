#pragma once
// Double for es-app/src/ApiSystem.h: the scripts the code under test runs,
// recorded, and nothing executed.
#include <functional>
#include <string>
#include <utility>
#include <vector>
class ApiSystem
{
public:
	static std::pair<std::string, int> executeScriptLegacy(const std::string& command, const std::function<void(const std::string)>& func);
};
