#include "stdafx.h"

static int lua_CreateHookableEvent(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	HANDLE res = ::CreateHookableEvent(name);
	lua_pushlightuserdata(L, res);

	return 1;
}

static int lua_DestroyHookableEvent(lua_State *L)
{
	HANDLE hEvent = (HANDLE)lua_touserdata(L, 1);

	int res = ::DestroyHookableEvent(hEvent);
	lua_pushinteger(L, res);

	return 1;
}

static int lua_NotifyEventHooks(lua_State *L)
{
	HANDLE hEvent = (HANDLE)lua_touserdata(L, 1);
	WPARAM wParam = (WPARAM)lua_tointeger(L, 2);
	LPARAM lParam = (LPARAM)lua_tointeger(L, 3);

	int res = ::NotifyEventHooks(hEvent, wParam, lParam);
	lua_pushinteger(L, res);

	return 1;
}

static int HookEventObjParam(void *obj, WPARAM wParam, LPARAM lParam, LPARAM param)
{
	lua_State *L = (lua_State*)obj;

	int ref = param;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	lua_pushnumber(L, wParam);
	lua_pushnumber(L, lParam);
	if(lua_pcall(L, 2, 1, 0))
		printf("%s\n", lua_tostring(L, -1));
	
	int res = (int)lua_tointeger(L, 1);

	//luaL_unref(L, LUA_REGISTRYINDEX, ref);

	return res;
}

static int lua_HookEvent(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (!lua_isfunction(L, 2))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE res = ::HookEventObjParam(name, HookEventObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	return 1;
}

static int lua_UnhookEvent(lua_State *L)
{
	HANDLE hEvent = (HANDLE)lua_touserdata(L, 1);

	int res = ::UnhookEvent(hEvent);
	lua_pushinteger(L, res);

	return 1;
}

static int lua_OnModulesLoaded(lua_State *L)
{
	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}
	
	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	
	HANDLE res = ::HookEventObjParam(ME_SYSTEM_MODULESLOADED, HookEventObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	return 1;
}

static int lua_OnPreShutdown(lua_State *L)
{
	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE res = ::HookEventObjParam(ME_SYSTEM_PRESHUTDOWN, HookEventObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	return 1;
}

static INT_PTR ServiceFunctionObjParam(void *obj, WPARAM wParam, LPARAM lParam, LPARAM param)
{
	lua_State *L = (lua_State*)obj;

	int ref = param;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	lua_pushnumber(L, wParam);
	lua_pushnumber(L, lParam);
	if (lua_pcall(L, 2, 1, 0))
		printf("%s\n", lua_tostring(L, -1));

	INT_PTR res = (INT_PTR)lua_tointeger(L, 1);

	//luaL_unref(L, LUA_REGISTRYINDEX, ref);

	return res;
}

static int lua_CreateServiceFunction(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (!lua_isfunction(L, 2))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE res = ::CreateServiceFunctionObjParam(name, ServiceFunctionObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	return 1;
}

static int lua_DestroyServiceFunction(lua_State *L)
{
	HANDLE hService = (HANDLE)lua_touserdata(L, 1);

	int res = ::DestroyServiceFunction(hService);
	lua_pushinteger(L, res);

	return 1;
}

static int lua_ServiceExists(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	int res = ::ServiceExists(name);
	lua_pushboolean(L, res);

	return 1;
}

static int lua_CallService(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	WPARAM wParam = (WPARAM)luaL_checkinteger(L, 2);
	LPARAM lParam = (LPARAM)luaL_checkinteger(L, 3);

	INT_PTR res = ::CallService(name, wParam, lParam);
	lua_pushinteger(L, res);

	return 1;
}

luaL_Reg CMLua::coreLib[] =
{
	{ "CreateHookableEvent", lua_CreateHookableEvent },
	{ "DestroyHookableEvent", lua_DestroyHookableEvent },

	{ "NotifyEventHooks", lua_NotifyEventHooks },

	{ "HookEvent", lua_HookEvent },
	{ "UnhookEvent", lua_UnhookEvent },
	{ "OnModulesLoaded", lua_OnModulesLoaded },
	{ "OnPreShutdown", lua_OnPreShutdown },

	{ "CreateServiceFunction", lua_CreateServiceFunction },
	{ "DestroyServiceFunction", lua_DestroyServiceFunction },

	{ "ServiceExists", lua_ServiceExists },
	{ "CallService", lua_CallService },

	{ NULL, NULL }
};