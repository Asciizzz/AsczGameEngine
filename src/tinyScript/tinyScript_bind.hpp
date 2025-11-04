#pragma once

#include "tinyData/tinyRT_Scene.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <SDL2/SDL.h>

extern "C" {
    #include "lua.h"
}

// ========== Helper Functions ==========

static inline tinyRT::Scene* getSceneFromLua(lua_State* L) {
    lua_getglobal(L, "__scene");
    void* scenePtr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<tinyRT::Scene*>(scenePtr);
}

static inline tinyHandle getNodeHandleFromTable(lua_State* L, int tableIndex) {
    lua_getfield(L, tableIndex, "index");
    lua_getfield(L, tableIndex, "version");
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
    return handle;
}

// Key name to SDL_Scancode mapping
static inline SDL_Scancode getScancodeFromName(const std::string& keyName) {
    // Create a static map for case-insensitive key name lookup
    static std::unordered_map<std::string, SDL_Scancode> keyMap;
    
    // Initialize the map on first call
    if (keyMap.empty()) {
        // Letters
        keyMap["a"] = SDL_SCANCODE_A; keyMap["b"] = SDL_SCANCODE_B; keyMap["c"] = SDL_SCANCODE_C;
        keyMap["d"] = SDL_SCANCODE_D; keyMap["e"] = SDL_SCANCODE_E; keyMap["f"] = SDL_SCANCODE_F;
        keyMap["g"] = SDL_SCANCODE_G; keyMap["h"] = SDL_SCANCODE_H; keyMap["i"] = SDL_SCANCODE_I;
        keyMap["j"] = SDL_SCANCODE_J; keyMap["k"] = SDL_SCANCODE_K; keyMap["l"] = SDL_SCANCODE_L;
        keyMap["m"] = SDL_SCANCODE_M; keyMap["n"] = SDL_SCANCODE_N; keyMap["o"] = SDL_SCANCODE_O;
        keyMap["p"] = SDL_SCANCODE_P; keyMap["q"] = SDL_SCANCODE_Q; keyMap["r"] = SDL_SCANCODE_R;
        keyMap["s"] = SDL_SCANCODE_S; keyMap["t"] = SDL_SCANCODE_T; keyMap["u"] = SDL_SCANCODE_U;
        keyMap["v"] = SDL_SCANCODE_V; keyMap["w"] = SDL_SCANCODE_W; keyMap["x"] = SDL_SCANCODE_X;
        keyMap["y"] = SDL_SCANCODE_Y; keyMap["z"] = SDL_SCANCODE_Z;
        
        // Numbers
        keyMap["0"] = SDL_SCANCODE_0; keyMap["1"] = SDL_SCANCODE_1; keyMap["2"] = SDL_SCANCODE_2;
        keyMap["3"] = SDL_SCANCODE_3; keyMap["4"] = SDL_SCANCODE_4; keyMap["5"] = SDL_SCANCODE_5;
        keyMap["6"] = SDL_SCANCODE_6; keyMap["7"] = SDL_SCANCODE_7; keyMap["8"] = SDL_SCANCODE_8;
        keyMap["9"] = SDL_SCANCODE_9;
        
        // Arrow keys
        keyMap["up"] = SDL_SCANCODE_UP;
        keyMap["down"] = SDL_SCANCODE_DOWN;
        keyMap["left"] = SDL_SCANCODE_LEFT;
        keyMap["right"] = SDL_SCANCODE_RIGHT;
        
        // Modifiers
        keyMap["shift"] = SDL_SCANCODE_LSHIFT;
        keyMap["lshift"] = SDL_SCANCODE_LSHIFT;
        keyMap["rshift"] = SDL_SCANCODE_RSHIFT;
        keyMap["ctrl"] = SDL_SCANCODE_LCTRL;
        keyMap["lctrl"] = SDL_SCANCODE_LCTRL;
        keyMap["rctrl"] = SDL_SCANCODE_RCTRL;
        keyMap["alt"] = SDL_SCANCODE_LALT;
        keyMap["lalt"] = SDL_SCANCODE_LALT;
        keyMap["ralt"] = SDL_SCANCODE_RALT;
        
        // Function keys
        keyMap["f1"] = SDL_SCANCODE_F1; keyMap["f2"] = SDL_SCANCODE_F2;
        keyMap["f3"] = SDL_SCANCODE_F3; keyMap["f4"] = SDL_SCANCODE_F4;
        keyMap["f5"] = SDL_SCANCODE_F5; keyMap["f6"] = SDL_SCANCODE_F6;
        keyMap["f7"] = SDL_SCANCODE_F7; keyMap["f8"] = SDL_SCANCODE_F8;
        keyMap["f9"] = SDL_SCANCODE_F9; keyMap["f10"] = SDL_SCANCODE_F10;
        keyMap["f11"] = SDL_SCANCODE_F11; keyMap["f12"] = SDL_SCANCODE_F12;
        
        // Special keys
        keyMap["space"] = SDL_SCANCODE_SPACE;
        keyMap["enter"] = SDL_SCANCODE_RETURN;
        keyMap["return"] = SDL_SCANCODE_RETURN;
        keyMap["escape"] = SDL_SCANCODE_ESCAPE;
        keyMap["esc"] = SDL_SCANCODE_ESCAPE;
        keyMap["tab"] = SDL_SCANCODE_TAB;
        keyMap["backspace"] = SDL_SCANCODE_BACKSPACE;
        keyMap["delete"] = SDL_SCANCODE_DELETE;
        keyMap["insert"] = SDL_SCANCODE_INSERT;
        keyMap["home"] = SDL_SCANCODE_HOME;
        keyMap["end"] = SDL_SCANCODE_END;
        keyMap["pageup"] = SDL_SCANCODE_PAGEUP;
        keyMap["pagedown"] = SDL_SCANCODE_PAGEDOWN;
    }
    
    auto it = keyMap.find(keyName);
    if (it != keyMap.end()) {
        return it->second;
    }
    
    return SDL_SCANCODE_UNKNOWN;
}

// ========== Input API Functions ==========

// Check if a key is currently pressed
// Usage: kState("w") or kState("W") - automatically converts to lowercase
static inline int lua_kState(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        return luaL_error(L, "kState expects (keyName: string)");
    }
    
    std::string keyName = lua_tostring(L, 1);
    
    // Always convert to lowercase for consistency
    std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);
    
    // Get the scancode for this key
    SDL_Scancode scancode = getScancodeFromName(keyName);
    
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        // Key not found, return false
        lua_pushboolean(L, false);
        return 1;
    }
    
    // Get keyboard state from SDL
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    bool isPressed = keyboardState[scancode] != 0;
    
    lua_pushboolean(L, isPressed);
    return 1;
}

// ========== Transform API Functions ==========

// Get node's local position - returns {x, y, z}
static inline int lua_getPosition(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "getPosition expects (nodeHandle)");
    }

    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    auto comps = scene->nComp(handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        lua_newtable(L);
        lua_pushnumber(L, pos.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, pos.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, pos.z); lua_setfield(L, -2, "z");
        return 1;
    }
    return 0;
}

// Set node's local position
static inline int lua_setPosition(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "setPosition expects (nodeHandle, {x, y, z})");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 newPos;
    newPos.x = static_cast<float>(lua_tonumber(L, -3));
    newPos.y = static_cast<float>(lua_tonumber(L, -2));
    newPos.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = scene->nComp(handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), newPos) 
                            * glm::mat4_cast(rot) 
                            * glm::scale(glm::mat4(1.0f), scale);
    }
    return 0;
}

// Get node's rotation as Euler angles (radians) - returns {x, y, z}
static inline int lua_getRotation(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "getRotation expects (nodeHandle)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    auto comps = scene->nComp(handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        glm::vec3 euler = glm::eulerAngles(rot);
        lua_newtable(L);
        lua_pushnumber(L, euler.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, euler.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, euler.z); lua_setfield(L, -2, "z");
        return 1;
    }
    return 0;
}

// Set node's rotation from Euler angles (radians)
static inline int lua_setRotation(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "setRotation expects (nodeHandle, {x, y, z})");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 euler;
    euler.x = static_cast<float>(lua_tonumber(L, -3));
    euler.y = static_cast<float>(lua_tonumber(L, -2));
    euler.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = scene->nComp(handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        glm::quat newRot = glm::quat(euler);
        
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) 
                            * glm::mat4_cast(newRot) 
                            * glm::scale(glm::mat4(1.0f), scale);
    }
    return 0;
}

// Rotate node around an axis by angle (radians)
static inline int lua_rotate(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isnumber(L, 3)) {
        return luaL_error(L, "rotate expects (nodeHandle, {x, y, z} axis, angle)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 axis;
    axis.x = static_cast<float>(lua_tonumber(L, -3));
    axis.y = static_cast<float>(lua_tonumber(L, -2));
    axis.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    float angle = static_cast<float>(lua_tonumber(L, 3));
    
    auto comps = scene->nComp(handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        glm::quat deltaRot = glm::angleAxis(angle, glm::normalize(axis));
        glm::quat newRot = deltaRot * rot;
        
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) 
                            * glm::mat4_cast(newRot) 
                            * glm::scale(glm::mat4(1.0f), scale);
    }
    return 0;
}

// ========== Animation API Functions ==========

// Get animation handle by name - returns {index, version} or nil if not found
// Usage: local walkHandle = getAnimHandle(__nodeHandle, "Walking_A")
static inline int lua_getAnimHandle(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "getAnimHandle expects (nodeHandle, animName: string)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    const char* animName = lua_tostring(L, 2);
    
    auto comps = scene->nComp(handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->getHandle(animName);
        if (animHandle.valid()) {
            lua_newtable(L);
            lua_pushinteger(L, animHandle.index);
            lua_setfield(L, -2, "index");
            lua_pushinteger(L, animHandle.version);
            lua_setfield(L, -2, "version");
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// Get current animation handle - returns {index, version} or nil
// Usage: local curHandle = getCurAnimHandle(__nodeHandle)
static inline int lua_getCurAnimHandle(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "getCurAnimHandle expects (nodeHandle)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    auto comps = scene->nComp(handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->curHandle();
        if (animHandle.valid()) {
            lua_newtable(L);
            lua_pushinteger(L, animHandle.index);
            lua_setfield(L, -2, "index");
            lua_pushinteger(L, animHandle.version);
            lua_setfield(L, -2, "version");
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// Play animation by handle with optional restart
// Usage: playAnim(__nodeHandle, walkHandle, true)
static inline int lua_playAnim(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "playAnim expects (nodeHandle, animHandle, restart: bool = true)");
    }
    
    // If animHandle is nil, just return (animation not found)
    if (lua_isnil(L, 2)) {
        return 0;
    }
    
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "playAnim expects animHandle to be a table or nil");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    tinyHandle animHandle = getNodeHandleFromTable(L, 2);
    
    bool restart = true;
    if (lua_isboolean(L, 3)) {
        restart = lua_toboolean(L, 3);
    }
    
    auto comps = scene->nComp(handle);
    if (comps.anim3D) {
        comps.anim3D->play(animHandle, restart);
    }
    return 0;
}

// Set animation playback speed
// Usage: setAnimSpeed(__nodeHandle, 2.0)
static inline int lua_setAnimSpeed(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "setAnimSpeed expects (nodeHandle, speed: number)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    float speed = static_cast<float>(lua_tonumber(L, 2));
    
    auto comps = scene->nComp(handle);
    if (comps.anim3D) {
        comps.anim3D->setSpeed(speed);
    }
    return 0;
}

// Check if animation is currently playing
// Usage: if isAnimPlaying(__nodeHandle) then ... end
static inline int lua_isAnimPlaying(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "isAnimPlaying expects (nodeHandle)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    tinyHandle handle = getNodeHandleFromTable(L, 1);
    
    auto comps = scene->nComp(handle);
    if (comps.anim3D) {
        lua_pushboolean(L, comps.anim3D->isPlaying());
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

// Compare two handles for equality (handles nil values gracefully)
// Usage: if animHandlesEqual(curHandle, walkHandle) then ... end
static inline int lua_animHandlesEqual(lua_State* L) {
    // If either parameter is nil, return false
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "animHandlesEqual expects (handle1, handle2) or accepts nil");
    }
    
    tinyHandle h1 = getNodeHandleFromTable(L, 1);
    tinyHandle h2 = getNodeHandleFromTable(L, 2);
    
    lua_pushboolean(L, h1.index == h2.index && h1.version == h2.version);
    return 1;
}

// ========== Registration Function ==========

static inline void registerNodeBindings(lua_State* L) {
    // Input API
    lua_pushcfunction(L, lua_kState);
    lua_setglobal(L, "kState");
    
    // Transform API
    lua_pushcfunction(L, lua_getPosition);
    lua_setglobal(L, "getPosition");
    
    lua_pushcfunction(L, lua_setPosition);
    lua_setglobal(L, "setPosition");
    
    lua_pushcfunction(L, lua_getRotation);
    lua_setglobal(L, "getRotation");
    
    lua_pushcfunction(L, lua_setRotation);
    lua_setglobal(L, "setRotation");
    
    lua_pushcfunction(L, lua_rotate);
    lua_setglobal(L, "rotate");
    
    // Animation API
    lua_pushcfunction(L, lua_getAnimHandle);
    lua_setglobal(L, "getAnimHandle");
    
    lua_pushcfunction(L, lua_getCurAnimHandle);
    lua_setglobal(L, "getCurAnimHandle");
    
    lua_pushcfunction(L, lua_playAnim);
    lua_setglobal(L, "playAnim");
    
    lua_pushcfunction(L, lua_setAnimSpeed);
    lua_setglobal(L, "setAnimSpeed");
    
    lua_pushcfunction(L, lua_isAnimPlaying);
    lua_setglobal(L, "isAnimPlaying");
    
    lua_pushcfunction(L, lua_animHandlesEqual);
    lua_setglobal(L, "animHandlesEqual");
}
