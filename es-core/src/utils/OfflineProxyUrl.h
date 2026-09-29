#pragma once
#ifndef ES_CORE_UTILS_OFFLINE_PROXY_URL_H
#define ES_CORE_UTILS_OFFLINE_PROXY_URL_H

#include <string>

// The offline RetroAchievements proxy's address on this device, and the one
// question the interface asks of a URL: is this the proxy? (fork #199)
//
// Pure: a string and a rule about it, so es-app/tests/unit can hold it to
// its word. The proxy is RAOfflineProxy on the loopback, at the port
// setsettings.sh points RetroArch at for every launch. OfflineAchievementsText
// builds the pages' requests on Base; WebImageComponent asks isProxyUrl before
// it fetches an image, because a request to the proxy carries a header no
// other host is sent. One definition, so the two cannot drift apart.
namespace Utils
{
	namespace OfflineProxy
	{
		// Scheme, host and port, no trailing slash: what every request to
		// the proxy is built on.
		const char* const Base = "http://127.0.0.1:8080";

		// The request header that asks the proxy for its store and nothing
		// else (the fork's proxy patches 010 and 012): a dorequest.php
		// answer from the store whatever the proxy believes about the link,
		// an image it holds or a 404 at once. As HttpReqOptions::customHeaders
		// takes it, name and value in one line.
		const char* const StoreOnlyHeader = "X-RA-Store-Only: 1";

		// Whether url is a request to the proxy: Base, compared without
		// regard to case (a scheme and a host are case-insensitive), followed
		// by the end of the string, a path, a query or a fragment -- so a host
		// that merely begins with it (127.0.0.1:80800, 127.0.0.1:8080.example)
		// is not, and neither is https, another port, or no port.
		bool isProxyUrl(const std::string& url);
	}
}

#endif // ES_CORE_UTILS_OFFLINE_PROXY_URL_H
