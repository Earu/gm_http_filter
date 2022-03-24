#include <GarrysMod/Lua/Interface.h>
#include <steam/isteamhttp.h>
#include "vtable.h"
#include <map>
#include <string>

#define CREATE_REQUEST_INDEX 0
#define SET_HEADER_VALUE_INDEX 3
#define SEND_HTTP_REQUEST_INDEX 5
#define SEND_HTTP_REQUEST_STREAM_INDEX 6

GarrysMod::Lua::ILuaBase* MENU = nullptr;
ISteamHTTP* HTTP = nullptr;
VTable* hooker = nullptr;

const char* MethodFromEnum(EHTTPMethod method) 
{
	switch (method) {
	case k_EHTTPMethodDELETE:
		return "DELETE";
	case k_EHTTPMethodGET:
		return "GET";
	case k_EHTTPMethodHEAD:
		return "HEAD";
	case k_EHTTPMethodOPTIONS:
		return "OPTIONS";
	case k_EHTTPMethodPATCH:
		return "PATCH";
	case k_EHTTPMethodPOST:
		return "POST";
	case k_EHTTPMethodPUT:
		return "PUT";
	default:
		return "GET";
	}
}

struct HTTPRequest {
	std::string URL;
	EHTTPMethod Method;
	std::map<std::string, std::string> Headers;
};

std::map<HTTPRequestHandle, HTTPRequest> reqHeaderCache;

typedef bool(__thiscall* SetHTTPHeaderValueFn)(void*, HTTPRequestHandle, const char*, const char*);
bool SetHTTPHeaderValue(void* inst, HTTPRequestHandle reqHandle, const char* headerName, const char* headerValue)
{
	bool success = SetHTTPHeaderValueFn(hooker->getold(SET_HEADER_VALUE_INDEX))(inst, reqHandle, headerName, headerValue);
	if (success && reqHeaderCache.find(reqHandle) != reqHeaderCache.end())
	{
		reqHeaderCache[reqHandle].Headers.emplace(std::string(headerName), std::string(headerValue));
	}

	return success;
}

typedef bool(__thiscall* SendHTTPRequestFn)(void*, HTTPRequestHandle, SteamAPICall_t*);
bool SendHTTPRequestBase(int fnIndex, void* inst, HTTPRequestHandle reqHandle, SteamAPICall_t* apiCall) {
	if (MENU == nullptr) return false;

	if (reqHeaderCache.find(reqHandle) == reqHeaderCache.end())
		return SendHTTPRequestFn(hooker->getold(fnIndex))(inst, reqHandle, apiCall);

	HTTPRequest req = reqHeaderCache[reqHandle];

	MENU->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	MENU->GetField(-1, "hook");

	MENU->GetField(-1, "Run");
	MENU->PushString("OnHTTPRequest");
	MENU->PushString(req.URL.c_str());
	MENU->PushString(MethodFromEnum(req.Method));

	MENU->CreateTable();
	if (reqHeaderCache.find(reqHandle) != reqHeaderCache.end())
	{
		for (auto it = req.Headers.begin(); it != req.Headers.end(); it++)
		{
			MENU->PushString(it->second.c_str());
			MENU->SetField(-2, it->first.c_str());
		}
	}

	MENU->Call(4, 1);

	bool ret = MENU->GetType(-1) == (int)GarrysMod::Lua::Type::BOOL && MENU->GetBool(-1) == true;

	MENU->Pop(3);

	reqHeaderCache.erase(reqHandle);

	if (!ret) {
		return SendHTTPRequestFn(hooker->getold(fnIndex))(inst, reqHandle, apiCall);
	}

	return false;
}

bool SendHTTPRequest(void* inst, HTTPRequestHandle reqHandle, SteamAPICall_t* apiCall)
{
	return SendHTTPRequestBase(SEND_HTTP_REQUEST_INDEX, inst, reqHandle, apiCall);
}

bool SendHTTPRequestStream(void* inst, HTTPRequestHandle reqHandle, SteamAPICall_t* apiCall)
{
	return SendHTTPRequestBase(SEND_HTTP_REQUEST_STREAM_INDEX, inst, reqHandle, apiCall);
}

typedef HTTPRequestHandle (__thiscall* CreateHTTPRequestFn)(void*, EHTTPMethod, const char*);
HTTPRequestHandle CreateHTTPRequest(void* inst, EHTTPMethod reqMethod, const char* absoluteUrl) 
{
	HTTPRequestHandle reqHandle = CreateHTTPRequestFn(hooker->getold(CREATE_REQUEST_INDEX))(inst, reqMethod, absoluteUrl);
	HTTPRequest req;
	req.Method = reqMethod;
	req.URL = std::string(absoluteUrl);

	reqHeaderCache.emplace(reqHandle, req);

	return reqHandle;
}

GMOD_MODULE_OPEN() 
{
	MENU = LUA;
	HTTP = SteamHTTP(); 

	hooker = new VTable(HTTP);
	hooker->hook(CREATE_REQUEST_INDEX, &CreateHTTPRequest);
	hooker->hook(SET_HEADER_VALUE_INDEX, &SetHTTPHeaderValue);
	hooker->hook(SEND_HTTP_REQUEST_INDEX, &SendHTTPRequest);
	hooker->hook(SEND_HTTP_REQUEST_STREAM_INDEX, &SendHTTPRequestStream);

	return 0;
}

GMOD_MODULE_CLOSE()		
{
	hooker->unhook(CREATE_REQUEST_INDEX);
	hooker->unhook(SET_HEADER_VALUE_INDEX);
	hooker->unhook(SEND_HTTP_REQUEST_INDEX);
	hooker->unhook(SEND_HTTP_REQUEST_STREAM_INDEX);

	delete hooker;

	MENU = nullptr;

	return 0;
}