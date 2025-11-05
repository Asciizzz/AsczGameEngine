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

// Helper to get tinyHandle from userdata (Node object)
static inline tinyHandle* getNodeHandleFromUserdata(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, "Node");
    if (!ud) {
        luaL_error(L, "Expected Node object");
        return nullptr;
    }
    return static_cast<tinyHandle*>(ud);
}

// Legacy helper for backward compatibility
static inline tinyHandle getNodeHandleFromTable(lua_State* L, int tableIndex) {
    lua_getfield(L, tableIndex, "index");
    lua_getfield(L, tableIndex, "version");
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
    return handle;
}

// Create a new Node userdata with metatable
static inline void pushNode(lua_State* L, tinyHandle handle) {
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = handle;
    
    luaL_getmetatable(L, "Node");
    lua_setmetatable(L, -2);
}

// Helper to get Scene pointer from Scene userdata
static inline tinyRT::Scene** getSceneFromUserdata(lua_State* L, int index) {
    return static_cast<tinyRT::Scene**>(luaL_checkudata(L, index, "Scene"));
}

// Helper to push Scene userdata
static inline void pushScene(lua_State* L, tinyRT::Scene* scene) {
    tinyRT::Scene** ud = static_cast<tinyRT::Scene**>(lua_newuserdata(L, sizeof(tinyRT::Scene*)));
    *ud = scene;
    
    luaL_getmetatable(L, "Scene");
    lua_setmetatable(L, -2);
}

// ========== Scene Methods ==========

// Scene:getNode(handle) - Get a Node object from a scriptHandle
// The handle is lightuserdata with isNode flag in high bit
// Returns nil if handle is invalid or is not a node handle
static inline int scene_getNode(lua_State* L) {
    tinyRT::Scene** scenePtr = getSceneFromUserdata(L, 1);
    if (!scenePtr || !*scenePtr) {
        return luaL_error(L, "Scene:getNode - invalid scene");
    }
    
    if (!lua_islightuserdata(L, 2)) {
        return luaL_error(L, "Scene:getNode expects a handle (lightuserdata from nHandle())");
    }
    
    // Decode scriptHandle from lightuserdata
    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    bool isNodeHandle = (packed & (1ULL << 63)) != 0;
    
    // Only accept node handles
    if (!isNodeHandle) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>((packed & 0x7FFFFFFFFFFFF000ULL) >> 32);
    handle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    
    // Validate handle - return nil if invalid
    if (!handle.valid() || !(*scenePtr)->node(handle)) {
        lua_pushnil(L);
        return 1;
    }
    
    pushNode(L, handle);
    return 1;
}

// ========== Handle Constructors ==========

// Global function: nHandle(index, version) - Create a NODE handle for use in vars()
// Node handles are remapped when scenes are loaded
// Usage in Lua: targetNode = nHandle(42, 1)
static inline int lua_nHandle(lua_State* L) {
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2)) {
        return luaL_error(L, "nHandle(index, version) expects two integers");
    }
    
    uint32_t index = static_cast<uint32_t>(lua_tointeger(L, 1));
    uint32_t version = static_cast<uint32_t>(lua_tointeger(L, 2));
    
    // Pack: high 1 bit = isNode flag, remaining 63 bits = packed handle
    // Format: [1 bit: isNode][32 bits: index][31 bits: version]
    uint64_t packed = (1ULL << 63) | (static_cast<uint64_t>(index) << 32) | version;
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
    return 1;
}

// Global function: fHandle(index, version) - Create a FILE/RESOURCE handle for use in vars()
// File handles are NOT remapped (global references)
// Usage in Lua: meshHandle = fHandle(10, 1)
static inline int lua_fHandle(lua_State* L) {
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2)) {
        return luaL_error(L, "fHandle(index, version) expects two integers");
    }
    
    uint32_t index = static_cast<uint32_t>(lua_tointeger(L, 1));
    uint32_t version = static_cast<uint32_t>(lua_tointeger(L, 2));
    
    // Pack: high 1 bit = 0 (not a node), remaining 63 bits = packed handle
    uint64_t packed = (static_cast<uint64_t>(index) << 32) | version;
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
    return 1;
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

// ========== Node Class Methods ==========

// Node:getPos() - Get node's local position
static inline int node_getPos(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
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

// Node:setPos(vec3) - Set node's local position
static inline int node_setPos(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "Node:setPos expects ({x, y, z})");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 newPos;
    newPos.x = static_cast<float>(lua_tonumber(L, -3));
    newPos.y = static_cast<float>(lua_tonumber(L, -2));
    newPos.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = scene->nComp(*handle);
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

// Node:getRot() - Get node's rotation as Euler angles (radians)
static inline int node_getRot(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
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

// Node:setRot(vec3) - Set node's rotation from Euler angles (radians)
static inline int node_setRot(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "Node:setRot expects ({x, y, z})");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 euler;
    euler.x = static_cast<float>(lua_tonumber(L, -3));
    euler.y = static_cast<float>(lua_tonumber(L, -2));
    euler.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = scene->nComp(*handle);
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

// Node:getScl() - Get node's local scale
static inline int node_getScl(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        lua_newtable(L);
        lua_pushnumber(L, scale.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, scale.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, scale.z); lua_setfield(L, -2, "z");
        return 1;
    }
    return 0;
}

// Node:setScl(vec3) - Set node's local scale
static inline int node_setScl(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "Node:setScl expects ({x, y, z})");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    lua_getfield(L, 2, "x");
    lua_getfield(L, 2, "y");
    lua_getfield(L, 2, "z");
    glm::vec3 newScale;
    newScale.x = static_cast<float>(lua_tonumber(L, -3));
    newScale.y = static_cast<float>(lua_tonumber(L, -2));
    newScale.z = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = scene->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) 
                            * glm::mat4_cast(rot) 
                            * glm::scale(glm::mat4(1.0f), newScale);
    }
    return 0;
}

// ========== Node Animation Methods ==========

// Node:getAnimHandle(name) - Get animation handle by name
static inline int node_getAnimHandle(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Node:getAnimHandle expects (animName: string)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    const char* animName = lua_tostring(L, 2);
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->getHandle(animName);
        if (animHandle.valid()) {
            // Push as a light userdata (just the uint64_t value)
            lua_pushlightuserdata(L, reinterpret_cast<void*>(
                (static_cast<uint64_t>(animHandle.index) << 32) | animHandle.version));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// Node:getCurAnimHandle() - Get current animation handle
static inline int node_getCurAnimHandle(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->curHandle();
        if (animHandle.valid()) {
            lua_pushlightuserdata(L, reinterpret_cast<void*>(
                (static_cast<uint64_t>(animHandle.index) << 32) | animHandle.version));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// Node:playAnim(animHandle, restart) - Play animation
static inline int node_playAnim(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    // If animHandle is nil, just return
    if (lua_isnil(L, 2)) {
        return 0;
    }
    
    if (!lua_islightuserdata(L, 2)) {
        return luaL_error(L, "Node:playAnim expects (animHandle, restart: bool = true)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    // Decode animHandle from lightuserdata
    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    tinyHandle animHandle;
    animHandle.index = static_cast<uint32_t>(packed >> 32);
    animHandle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    
    bool restart = true;
    if (lua_isboolean(L, 3)) {
        restart = lua_toboolean(L, 3);
    }
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->play(animHandle, restart);
    }
    return 0;
}

// Node:setAnimSpeed(speed) - Set animation playback speed
static inline int node_setAnimSpeed(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "Node:setAnimSpeed expects (speed: number)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    float speed = static_cast<float>(lua_tonumber(L, 2));
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->setSpeed(speed);
    }
    return 0;
}

// Node:isAnimPlaying() - Check if animation is playing
static inline int node_isAnimPlaying(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        lua_pushboolean(L, comps.anim3D->isPlaying());
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

// Node:getAnimTime() - Get current animation time
static inline int node_getAnimTime(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        lua_pushnumber(L, comps.anim3D->getTime());
        return 1;
    }
    lua_pushnumber(L, 0.0f);
    return 1;
}

// Node:setAnimTime(time) - Set current animation time
static inline int node_setAnimTime(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "setAnimTime expects a number");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    float time = static_cast<float>(lua_tonumber(L, 2));
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->setTime(time);
    }
    return 0;
}

// Node:getAnimDuration() - Get duration of current animation
static inline int node_getAnimDuration(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        tinyHandle curHandle = comps.anim3D->curHandle();
        float duration = comps.anim3D->duration(curHandle);
        lua_pushnumber(L, duration);
        return 1;
    }
    lua_pushnumber(L, 0.0f);
    return 1;
}

// Node:getAnimDurationByHandle(animHandle) - Get duration of specific animation
static inline int node_getAnimDurationByHandle(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (lua_isnil(L, 2)) {
        lua_pushnumber(L, 0.0f);
        return 1;
    }
    
    if (!lua_islightuserdata(L, 2)) {
        return luaL_error(L, "getAnimDurationByHandle expects a handle (lightuserdata)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    // Decode animHandle from lightuserdata
    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    tinyHandle animHandle;
    animHandle.index = static_cast<uint32_t>(packed >> 32);
    animHandle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        float duration = comps.anim3D->duration(animHandle);
        lua_pushnumber(L, duration);
        return 1;
    }
    lua_pushnumber(L, 0.0f);
    return 1;
}

// Node:setAnimLoop(loop) - Set animation loop state
static inline int node_setAnimLoop(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isboolean(L, 2)) {
        return luaL_error(L, "setAnimLoop expects a boolean");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    bool loop = lua_toboolean(L, 2);
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->setLoop(loop);
    }
    return 0;
}

// Node:getAnimLoop() - Get animation loop state
static inline int node_getAnimLoop(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        lua_pushboolean(L, comps.anim3D->getLoop());
        return 1;
    }
    lua_pushboolean(L, true); // Default to true
    return 1;
}

// Node:pauseAnim() - Pause animation
static inline int node_pauseAnim(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->pause();
    }
    return 0;
}

// Node:resumeAnim() - Resume animation
static inline int node_resumeAnim(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    auto comps = scene->nComp(*handle);
    if (comps.anim3D) {
        comps.anim3D->resume();
    }
    return 0;
}

// ========== Node Variable Access Methods ==========

// Node:getVar(name) - Get a script variable from this node's script component
// Returns the variable value or nil if not found
static inline int node_getVar(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Node:getVar expects (varName: string)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    const char* varName = lua_tostring(L, 2);
    
    auto comps = scene->nComp(*handle);
    if (comps.script && comps.script->vHas(varName)) {
        const tinyVar& var = comps.script->vMap().at(varName);
        
        // Convert tinyVar to Lua value
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            
            if constexpr (std::is_same_v<T, float>) {
                lua_pushnumber(L, val);
            }
            else if constexpr (std::is_same_v<T, int>) {
                lua_pushinteger(L, val);
            }
            else if constexpr (std::is_same_v<T, bool>) {
                lua_pushboolean(L, val);
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                lua_newtable(L);
                lua_pushnumber(L, val.x);
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y);
                lua_setfield(L, -2, "y");
                lua_pushnumber(L, val.z);
                lua_setfield(L, -2, "z");
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                lua_pushstring(L, val.c_str());
            }
            else if constexpr (std::is_same_v<T, scriptHandle>) {
                uint64_t packed = (val.isNodeHandle ? (1ULL << 63) : 0ULL) |
                                 (static_cast<uint64_t>(val.handle.index) << 32) | 
                                 val.handle.version;
                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
        }, var);
        
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

// Node:setVar(name, value) - Set a script variable on this node's script component
static inline int node_setVar(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    if (!lua_isstring(L, 2)) {
        return luaL_error(L, "Node:setVar expects (varName: string, value)");
    }
    
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return luaL_error(L, "Scene pointer is null");
    
    const char* varName = lua_tostring(L, 2);
    
    auto comps = scene->nComp(*handle);
    if (comps.script && comps.script->vHas(varName)) {
        tinyVar& var = comps.script->vMap().at(varName);
        
        // Convert Lua value to tinyVar based on existing type
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            
            if constexpr (std::is_same_v<T, float>) {
                val = static_cast<float>(lua_tonumber(L, 3));
            }
            else if constexpr (std::is_same_v<T, int>) {
                val = static_cast<int>(lua_tointeger(L, 3));
            }
            else if constexpr (std::is_same_v<T, bool>) {
                val = static_cast<bool>(lua_toboolean(L, 3));
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                if (lua_istable(L, 3)) {
                    lua_getfield(L, 3, "x");
                    lua_getfield(L, 3, "y");
                    lua_getfield(L, 3, "z");
                    val.x = static_cast<float>(lua_tonumber(L, -3));
                    val.y = static_cast<float>(lua_tonumber(L, -2));
                    val.z = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 3);
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                val = std::string(lua_tostring(L, 3));
            }
            else if constexpr (std::is_same_v<T, scriptHandle>) {
                if (lua_islightuserdata(L, 3)) {
                    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 3));
                    val.isNodeHandle = (packed & (1ULL << 63)) != 0;
                    val.handle.index = static_cast<uint32_t>((packed & 0x7FFFFFFFFFFFF000ULL) >> 32);
                    val.handle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
                }
            }
        }, var);
    }
    
    return 0;
}

// ========== Generic Handle Comparison ==========

// Global function: handleEqual(h1, h2) - Compare any handles (lightuserdata)
// Works for animation handles, node handles (if converted to lightuserdata), etc.
static inline int lua_handleEqual(lua_State* L) {
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!lua_islightuserdata(L, 1) || !lua_islightuserdata(L, 2)) {
        return luaL_error(L, "handleEqual expects (handle1, handle2) or nil");
    }
    
    uint64_t h1 = reinterpret_cast<uint64_t>(lua_touserdata(L, 1));
    uint64_t h2 = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    
    lua_pushboolean(L, h1 == h2);
    return 1;
}

// Legacy alias for animation handles (backward compatibility)
static inline int lua_animHandlesEqual(lua_State* L) {
    return lua_handleEqual(L);
}

// ========== Debug Print Function ==========
static inline int lua_print(lua_State* L) {
    // Get rtScript pointer from Lua registry
    lua_getglobal(L, "__rtScript");
    void* rtScriptPtr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    if (!rtScriptPtr) {
        return luaL_error(L, "print(): __rtScript not found");
    }
    
    tinyRT::Script* rtScript = static_cast<tinyRT::Script*>(rtScriptPtr);
    
    // Get number of arguments
    int n = lua_gettop(L);
    
    // Build message string
    std::string message;
    for (int i = 1; i <= n; i++) {
        if (i > 1) message += "\t";  // Tab separator between arguments
        
        if (lua_isstring(L, i)) {
            message += lua_tostring(L, i);
        } else if (lua_isnumber(L, i)) {
            message += std::to_string(lua_tonumber(L, i));
        } else if (lua_isboolean(L, i)) {
            message += lua_toboolean(L, i) ? "true" : "false";
        } else if (lua_isnil(L, i)) {
            message += "nil";
        } else {
            message += lua_typename(L, lua_type(L, i));
        }
    }
    
    // Log to runtime debug (white color)
    rtScript->debug().log(message, 1.0f, 1.0f, 1.0f);
    
    return 0;
}

// ========== Registration Function ==========

static inline void registerNodeBindings(lua_State* L) {
    // Create Node metatable
    luaL_newmetatable(L, "Node");
    
    // Create methods table for __index
    lua_newtable(L);
    
    // Transform methods
    lua_pushcfunction(L, node_getPos);
    lua_setfield(L, -2, "getPos");
    
    lua_pushcfunction(L, node_setPos);
    lua_setfield(L, -2, "setPos");
    
    lua_pushcfunction(L, node_getRot);
    lua_setfield(L, -2, "getRot");
    
    lua_pushcfunction(L, node_setRot);
    lua_setfield(L, -2, "setRot");
    
    lua_pushcfunction(L, node_getScl);
    lua_setfield(L, -2, "getScl");
    
    lua_pushcfunction(L, node_setScl);
    lua_setfield(L, -2, "setScl");
    
    // Animation methods
    lua_pushcfunction(L, node_getAnimHandle);
    lua_setfield(L, -2, "getAnimHandle");
    
    lua_pushcfunction(L, node_getCurAnimHandle);
    lua_setfield(L, -2, "getCurAnimHandle");
    
    lua_pushcfunction(L, node_playAnim);
    lua_setfield(L, -2, "playAnim");
    
    lua_pushcfunction(L, node_setAnimSpeed);
    lua_setfield(L, -2, "setAnimSpeed");
    
    lua_pushcfunction(L, node_isAnimPlaying);
    lua_setfield(L, -2, "isAnimPlaying");
    
    lua_pushcfunction(L, node_getAnimTime);
    lua_setfield(L, -2, "getAnimTime");
    
    lua_pushcfunction(L, node_setAnimTime);
    lua_setfield(L, -2, "setAnimTime");
    
    lua_pushcfunction(L, node_getAnimDuration);
    lua_setfield(L, -2, "getAnimDuration");
    
    lua_pushcfunction(L, node_getAnimDurationByHandle);
    lua_setfield(L, -2, "getAnimDurationByHandle");
    
    lua_pushcfunction(L, node_setAnimLoop);
    lua_setfield(L, -2, "setAnimLoop");
    
    lua_pushcfunction(L, node_getAnimLoop);
    lua_setfield(L, -2, "getAnimLoop");
    
    lua_pushcfunction(L, node_pauseAnim);
    lua_setfield(L, -2, "pauseAnim");
    
    lua_pushcfunction(L, node_resumeAnim);
    lua_setfield(L, -2, "resumeAnim");
    
    // Variable access methods
    lua_pushcfunction(L, node_getVar);
    lua_setfield(L, -2, "getVar");
    
    lua_pushcfunction(L, node_setVar);
    lua_setfield(L, -2, "setVar");
    
    // Set methods table as __index
    lua_setfield(L, -2, "__index");
    
    // Add __tostring for debugging
    lua_pushcfunction(L, [](lua_State* L) -> int {
        tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
        if (handle) {
            lua_pushfstring(L, "Node(%d, %d)", handle->index, handle->version);
        } else {
            lua_pushstring(L, "Node(invalid)");
        }
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    
    // Add __eq for equality comparison (node1 == node2)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        tinyHandle* h1 = getNodeHandleFromUserdata(L, 1);
        tinyHandle* h2 = getNodeHandleFromUserdata(L, 2);
        
        if (h1 && h2) {
            lua_pushboolean(L, h1->index == h2->index && h1->version == h2->version);
        } else {
            lua_pushboolean(L, false);
        }
        return 1;
    });
    lua_setfield(L, -2, "__eq");
    
    // Pop metatable
    lua_pop(L, 1);
    
    // Create Scene metatable
    luaL_newmetatable(L, "Scene");
    
    // Create methods table for __index
    lua_newtable(L);
    
    lua_pushcfunction(L, scene_getNode);
    lua_setfield(L, -2, "getNode");
    
    // Set methods table as __index
    lua_setfield(L, -2, "__index");
    
    // Add __tostring for debugging
    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_pushstring(L, "Scene");
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    
    // Pop metatable
    lua_pop(L, 1);
    
    // Input API (global)
    lua_pushcfunction(L, lua_kState);
    lua_setglobal(L, "kState");
    
    // Handle constructors (global)
    lua_pushcfunction(L, lua_nHandle);
    lua_setglobal(L, "nHandle");
    
    lua_pushcfunction(L, lua_fHandle);
    lua_setglobal(L, "fHandle");
    
    lua_pushcfunction(L, lua_handleEqual);
    lua_setglobal(L, "handleEqual");
    
    // Legacy animation handle comparison (calls handleEqual)
    lua_pushcfunction(L, lua_animHandlesEqual);
    lua_setglobal(L, "animHandlesEqual");
    
    // Debug print function (global)
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");
}
