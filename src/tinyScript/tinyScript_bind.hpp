#pragma once

extern "C" {
    #include "lua.h"
}

// Lua C function bindings for scene node manipulation
// These functions expose node transform operations to Lua scripts

// Transform manipulation functions
int lua_getPosition(lua_State* L);
int lua_setPosition(lua_State* L);
int lua_getRotation(lua_State* L);
int lua_setRotation(lua_State* L);
int lua_rotate(lua_State* L);

// Register all node manipulation functions into the Lua state
void registerNodeBindings(lua_State* L);
