#include <GarrysMod/Lua/Interface.h>
#include <steam/isteamhttp.h>
#include "vtable.h"

#define CREATE_REQUEST_INDEX 0

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
		return "";
	}
}

typedef HTTPRequestHandle (__thiscall* CreateHTTPRequestFn)(void*, EHTTPMethod, const char*);
HTTPRequestHandle CreateHTTPRequest(void* inst, EHTTPMethod reqMethod, const char* absoluteUrl) 
{
	if (MENU == nullptr) {
		return CreateHTTPRequestFn(hooker->getold(CREATE_REQUEST_INDEX))(inst, reqMethod, absoluteUrl);
	}

	MENU->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	MENU->GetField(-1, "hook");

	MENU->GetField(-1, "Run");
	MENU->PushString("OnHTTPRequest");
	MENU->PushString(absoluteUrl);
	MENU->PushString(MethodFromEnum(reqMethod));
	MENU->Call(3, 1);

	bool ret = MENU->GetType(-1) == (int)GarrysMod::Lua::Type::BOOL && MENU->GetBool(-1) == true;

	MENU->Pop(3);

	if (ret) {
		return INVALID_HTTPREQUEST_HANDLE;
	}

	return CreateHTTPRequestFn(hooker->getold(CREATE_REQUEST_INDEX))(inst, reqMethod, absoluteUrl);
}

GMOD_MODULE_OPEN() 
{
	MENU = LUA;
	HTTP = SteamHTTP(); 

	hooker = new VTable(HTTP);
	hooker->hook(CREATE_REQUEST_INDEX, (void*)&CreateHTTPRequest);

	return 0;
}

GMOD_MODULE_CLOSE()		
{
	hooker->unhook(CREATE_REQUEST_INDEX);
	delete hooker;

	MENU = nullptr;

	return 0;
}