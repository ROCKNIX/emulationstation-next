#include "utils/OfflineProxyUrl.h"

#include <cctype>
#include <cstring>

bool Utils::OfflineProxy::isProxyUrl(const std::string& url)
{
	const size_t baseLength = std::strlen(Base);
	if (url.size() < baseLength)
		return false;

	for (size_t i = 0; i < baseLength; i++)
		if (std::tolower((unsigned char)url[i]) != std::tolower((unsigned char)Base[i]))
			return false;

	if (url.size() == baseLength)
		return true;

	const char next = url[baseLength];
	return next == '/' || next == '?' || next == '#';
}
