#pragma once

#include "tinyData/tinyRT_Scene.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <SDL2/SDL.h>

extern "C" {
    #include "lua.h"
}

// ========================================
// HELPER FUNCTIONS
// ========================================

static inline tinyRT::Scene* getSceneFromLua(lua_State* L) {
    lua_getglobal(L, "__scene");
    void* ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<tinyRT::Scene*>(ptr);
}

static inline tinyHandle* getNodeHandleFromUserdata(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, "Node");
    return ud ? static_cast<tinyHandle*>(ud) : nullptr;
}

static inline tinyRT::Scene** getSceneFromUserdata(lua_State* L, int index) {
    return static_cast<tinyRT::Scene**>(luaL_checkudata(L, index, "Scene"));
}

static inline void pushNode(lua_State* L, tinyHandle handle) {
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = handle;
    luaL_getmetatable(L, "Node");
    lua_setmetatable(L, -2);
}

static inline void pushScene(lua_State* L, tinyRT::Scene* scene) {
    tinyRT::Scene** ud = static_cast<tinyRT::Scene**>(lua_newuserdata(L, sizeof(tinyRT::Scene*)));
    *ud = scene;
    luaL_getmetatable(L, "Scene");
    lua_setmetatable(L, -2);
}

// ========================================
// HANDLE PACKING/UNPACKING UTILITIES
// ========================================

// Pack nHandle (node handle) with bit 63 set to 1
static inline uint64_t packNHandle(const tinyHandle& handle) {
    return (1ULL << 63) | (static_cast<uint64_t>(handle.index) << 32) | handle.version;
}

// Pack rHandle (registry/resource handle) with bit 63 set to 0
static inline uint64_t packRHandle(const tinyHandle& handle) {
    return (static_cast<uint64_t>(handle.index) << 32) | handle.version;
}

// Unpack nHandle (expects bit 63 to be 1)
static inline tinyHandle unpackNHandle(uint64_t packed) {
    tinyHandle handle;
    handle.index = static_cast<uint32_t>((packed & 0x7FFFFFFFFFFFF000ULL) >> 32);
    handle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    return handle;
}

// Unpack rHandle (expects bit 63 to be 0)
static inline tinyHandle unpackRHandle(uint64_t packed) {
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(packed >> 32);
    handle.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    return handle;
}

// Check if packed handle is an nHandle (bit 63 == 1)
static inline bool isNHandle(uint64_t packed) {
    return (packed & (1ULL << 63)) != 0;
}

// Check if packed handle is an rHandle (bit 63 == 0)
static inline bool isRHandle(uint64_t packed) {
    return (packed & (1ULL << 63)) == 0;
}

// Push nHandle as lightuserdata
static inline void pushNHandle(lua_State* L, const tinyHandle& handle) {
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packNHandle(handle)));
}

// Push rHandle as lightuserdata
static inline void pushRHandle(lua_State* L, const tinyHandle& handle) {
    lua_pushlightuserdata(L, reinterpret_cast<void*>(packRHandle(handle)));
}

// ========================================
// HANDLE CONSTRUCTORS & UTILITIES
// ========================================

static inline int lua_nHandle(lua_State* L) {
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return luaL_error(L, "nHandle(index, version) expects two integers");
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, 1));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, 2));
    pushNHandle(L, handle);
    return 1;
}

static inline int lua_rHandle(lua_State* L) {
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return luaL_error(L, "rHandle(index, version) expects two integers");
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, 1));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, 2));
    pushRHandle(L, handle);
    return 1;
}

static inline int lua_handleEqual(lua_State* L) {
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (!lua_islightuserdata(L, 1) || !lua_islightuserdata(L, 2))
        return luaL_error(L, "handleEqual expects two handles or nil");
    
    uint64_t h1 = reinterpret_cast<uint64_t>(lua_touserdata(L, 1));
    uint64_t h2 = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    lua_pushboolean(L, h1 == h2);
    return 1;
}

// ========================================
// SCENE METHODS
// ========================================

static inline int scene_node(lua_State* L) {
    tinyRT::Scene** scenePtr = getSceneFromUserdata(L, 1);
    if (!scenePtr || !*scenePtr)
        return luaL_error(L, "Invalid scene");
    
    if (lua_isnil(L, 2)) {
        lua_pushnil(L);
        return 1;
    }
    
    if (!lua_islightuserdata(L, 2))
        return luaL_error(L, "Expected handle from nHandle()");
    
    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    if (!isNHandle(packed)) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyHandle handle = unpackNHandle(packed);
    
    if (!handle.valid() || !(*scenePtr)->node(handle)) {
        lua_pushnil(L);
        return 1;
    }
    
    pushNode(L, handle);
    return 1;
}

static inline int scene_addScene(lua_State* L) {
    tinyRT::Scene** scenePtr = getSceneFromUserdata(L, 1);
    if (!scenePtr || !*scenePtr)
        return luaL_error(L, "Invalid scene");
    
    // Argument 2: rHandle (registry handle to Scene resource)
    if (!lua_islightuserdata(L, 2))
        return luaL_error(L, "addScene(sceneRHandle, parentNHandle) expects rHandle as first argument");
    
    uint64_t packedRHandle = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
    
    // Check if it's an rHandle (bit 63 should be 0 for rHandle)
    if (!isRHandle(packedRHandle)) {
        return luaL_error(L, "addScene expects rHandle (registry handle), not nHandle (node handle) as first argument");
    }
    
    tinyHandle sceneRHandle = unpackRHandle(packedRHandle);
    
    // Argument 3: nHandle (node handle for parent) - optional
    tinyHandle parentNHandle;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        if (!lua_islightuserdata(L, 3))
            return luaL_error(L, "addScene expects nHandle as second argument (optional)");
        
        uint64_t packedNHandle = reinterpret_cast<uint64_t>(lua_touserdata(L, 3));
        
        // Check if it's an nHandle (bit 63 should be 1 for nHandle)
        if (!isNHandle(packedNHandle)) {
            return luaL_error(L, "addScene expects nHandle (node handle) as second argument, not rHandle");
        }
        
        parentNHandle = unpackNHandle(packedNHandle);
    }
    // If no parent provided, it will default to root in the C++ function
    
    // Call the existing C++ addScene function
    // The existing function uses sharedRes_.fsGet<Scene>(sceneRHandle) internally
    (*scenePtr)->addScene(sceneRHandle, parentNHandle);
    
    return 0; // Returns void, no return value
}

// ========================================
// INPUT SYSTEM
// ========================================

static inline SDL_Scancode getScancodeFromName(const std::string& keyName) {
    static std::unordered_map<std::string, SDL_Scancode> keyMap;
    
    if (keyMap.empty()) {
        keyMap["a"] = SDL_SCANCODE_A; keyMap["b"] = SDL_SCANCODE_B; keyMap["c"] = SDL_SCANCODE_C;
        keyMap["d"] = SDL_SCANCODE_D; keyMap["e"] = SDL_SCANCODE_E; keyMap["f"] = SDL_SCANCODE_F;
        keyMap["g"] = SDL_SCANCODE_G; keyMap["h"] = SDL_SCANCODE_H; keyMap["i"] = SDL_SCANCODE_I;
        keyMap["j"] = SDL_SCANCODE_J; keyMap["k"] = SDL_SCANCODE_K; keyMap["l"] = SDL_SCANCODE_L;
        keyMap["m"] = SDL_SCANCODE_M; keyMap["n"] = SDL_SCANCODE_N; keyMap["o"] = SDL_SCANCODE_O;
        keyMap["p"] = SDL_SCANCODE_P; keyMap["q"] = SDL_SCANCODE_Q; keyMap["r"] = SDL_SCANCODE_R;
        keyMap["s"] = SDL_SCANCODE_S; keyMap["t"] = SDL_SCANCODE_T; keyMap["u"] = SDL_SCANCODE_U;
        keyMap["v"] = SDL_SCANCODE_V; keyMap["w"] = SDL_SCANCODE_W; keyMap["x"] = SDL_SCANCODE_X;
        keyMap["y"] = SDL_SCANCODE_Y; keyMap["z"] = SDL_SCANCODE_Z;
        
        keyMap["0"] = SDL_SCANCODE_0; keyMap["1"] = SDL_SCANCODE_1; keyMap["2"] = SDL_SCANCODE_2;
        keyMap["3"] = SDL_SCANCODE_3; keyMap["4"] = SDL_SCANCODE_4; keyMap["5"] = SDL_SCANCODE_5;
        keyMap["6"] = SDL_SCANCODE_6; keyMap["7"] = SDL_SCANCODE_7; keyMap["8"] = SDL_SCANCODE_8;
        keyMap["9"] = SDL_SCANCODE_9;
        
        keyMap["up"] = SDL_SCANCODE_UP; keyMap["down"] = SDL_SCANCODE_DOWN;
        keyMap["left"] = SDL_SCANCODE_LEFT; keyMap["right"] = SDL_SCANCODE_RIGHT;
        
        keyMap["shift"] = SDL_SCANCODE_LSHIFT; keyMap["lshift"] = SDL_SCANCODE_LSHIFT; keyMap["rshift"] = SDL_SCANCODE_RSHIFT;
        keyMap["ctrl"] = SDL_SCANCODE_LCTRL; keyMap["lctrl"] = SDL_SCANCODE_LCTRL; keyMap["rctrl"] = SDL_SCANCODE_RCTRL;
        keyMap["alt"] = SDL_SCANCODE_LALT; keyMap["lalt"] = SDL_SCANCODE_LALT; keyMap["ralt"] = SDL_SCANCODE_RALT;
        
        keyMap["f1"] = SDL_SCANCODE_F1; keyMap["f2"] = SDL_SCANCODE_F2; keyMap["f3"] = SDL_SCANCODE_F3;
        keyMap["f4"] = SDL_SCANCODE_F4; keyMap["f5"] = SDL_SCANCODE_F5; keyMap["f6"] = SDL_SCANCODE_F6;
        keyMap["f7"] = SDL_SCANCODE_F7; keyMap["f8"] = SDL_SCANCODE_F8; keyMap["f9"] = SDL_SCANCODE_F9;
        keyMap["f10"] = SDL_SCANCODE_F10; keyMap["f11"] = SDL_SCANCODE_F11; keyMap["f12"] = SDL_SCANCODE_F12;
        
        keyMap["space"] = SDL_SCANCODE_SPACE; keyMap["enter"] = SDL_SCANCODE_RETURN; keyMap["return"] = SDL_SCANCODE_RETURN;
        keyMap["escape"] = SDL_SCANCODE_ESCAPE; keyMap["esc"] = SDL_SCANCODE_ESCAPE; keyMap["tab"] = SDL_SCANCODE_TAB;
        keyMap["backspace"] = SDL_SCANCODE_BACKSPACE; keyMap["delete"] = SDL_SCANCODE_DELETE;
        keyMap["insert"] = SDL_SCANCODE_INSERT; keyMap["home"] = SDL_SCANCODE_HOME; keyMap["end"] = SDL_SCANCODE_END;
        keyMap["pageup"] = SDL_SCANCODE_PAGEUP; keyMap["pagedown"] = SDL_SCANCODE_PAGEDOWN;
    }
    
    auto it = keyMap.find(keyName);
    return (it != keyMap.end()) ? it->second : SDL_SCANCODE_UNKNOWN;
}

static inline int lua_kState(lua_State* L) {
    if (!lua_isstring(L, 1))
        return luaL_error(L, "kState expects string");
    
    std::string keyName = lua_tostring(L, 1);
    std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);
    
    SDL_Scancode scancode = getScancodeFromName(keyName);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    lua_pushboolean(L, keyboardState[scancode] != 0);
    return 1;
}

// ========================================
// COMPONENT: TRANSFORM3D
// ========================================

static inline tinyHandle* getTransform3DHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Transform3D"));
}

static inline int transform3d_getPos(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
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

static inline int transform3d_setPos(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle || !lua_istable(L, 2)) return 0;
    
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z");
    glm::vec3 newPos(lua_tonumber(L, -3), lua_tonumber(L, -2), lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), newPos) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.0f), scale);
    }
    return 0;
}

static inline int transform3d_getRot(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
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

static inline int transform3d_setRot(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle || !lua_istable(L, 2)) return 0;
    
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z");
    glm::vec3 euler(lua_tonumber(L, -3), lua_tonumber(L, -2), lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        glm::quat newRot = glm::quat(euler);
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(newRot) * glm::scale(glm::mat4(1.0f), scale);
    }
    return 0;
}

static inline int transform3d_getScl(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
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

static inline int transform3d_setScl(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle || !lua_istable(L, 2)) return 0;
    
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z");
    glm::vec3 newScale(lua_tonumber(L, -3), lua_tonumber(L, -2), lua_tonumber(L, -1));
    lua_pop(L, 3);
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.0f), newScale);
    }
    return 0;
}

static inline int node_transform3D(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (!comps.trfm3D) { lua_pushnil(L); return 1; }
    
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = *handle;
    luaL_getmetatable(L, "Transform3D");
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// COMPONENT: ANIMATION3D
// ========================================

static inline tinyHandle* getAnim3DHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Anim3D"));
}

static inline int anim3d_get(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isstring(L, 2)) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->getHandle(lua_tostring(L, 2));
        if (animHandle.valid()) {
            uint64_t packed = (static_cast<uint64_t>(animHandle.index) << 32) | animHandle.version;
            lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static inline int anim3d_current(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->curHandle();
        if (animHandle.valid()) {
            uint64_t packed = (static_cast<uint64_t>(animHandle.index) << 32) | animHandle.version;
            lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static inline int anim3d_play(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) return 0;
    
    tinyHandle animHandle;
    if (lua_isstring(L, 2)) {
        auto comps = getSceneFromLua(L)->nComp(*handle);
        if (!comps.anim3D) return 0;
        animHandle = comps.anim3D->getHandle(lua_tostring(L, 2));
    } else if (lua_islightuserdata(L, 2)) {
        uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
        animHandle = {static_cast<uint32_t>(packed >> 32), static_cast<uint32_t>(packed & 0xFFFFFFFF)};
    } else {
        return 0;
    }
    
    bool restart = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : true;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->play(animHandle, restart);
    return 0;
}

static inline int anim3d_setSpeed(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isnumber(L, 2)) return 0;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->setSpeed(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

static inline int anim3d_isPlaying(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushboolean(L, false); return 1; }
    auto comps = getSceneFromLua(L)->nComp(*handle);
    lua_pushboolean(L, comps.anim3D && comps.anim3D->isPlaying());
    return 1;
}

static inline int anim3d_getTime(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnumber(L, 0.0f); return 1; }
    auto comps = getSceneFromLua(L)->nComp(*handle);
    lua_pushnumber(L, comps.anim3D ? comps.anim3D->getTime() : 0.0f);
    return 1;
}

static inline int anim3d_setTime(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isnumber(L, 2)) return 0;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->setTime(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

static inline int anim3d_getDuration(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnumber(L, 0.0f); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (!comps.anim3D) { lua_pushnumber(L, 0.0f); return 1; }
    
    tinyHandle animHandle;
    if (lua_islightuserdata(L, 2)) {
        uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 2));
        animHandle = {static_cast<uint32_t>(packed >> 32), static_cast<uint32_t>(packed & 0xFFFFFFFF)};
    } else {
        animHandle = comps.anim3D->curHandle();
    }
    
    lua_pushnumber(L, comps.anim3D->duration(animHandle));
    return 1;
}

static inline int anim3d_setLoop(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isboolean(L, 2)) return 0;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->setLoop(lua_toboolean(L, 2));
    return 0;
}

static inline int anim3d_isLoop(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushboolean(L, true); return 1; }
    auto comps = getSceneFromLua(L)->nComp(*handle);
    lua_pushboolean(L, comps.anim3D ? comps.anim3D->getLoop() : true);
    return 1;
}

static inline int anim3d_pause(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) return 0;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->pause();
    return 0;
}

static inline int anim3d_resume(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) return 0;
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.anim3D) comps.anim3D->resume();
    return 0;
}

static inline int node_anim3D(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (!comps.anim3D) { lua_pushnil(L); return 1; }
    
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = *handle;
    luaL_getmetatable(L, "Anim3D");
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// COMPONENT: SCRIPT
// ========================================

static inline tinyHandle* getScriptHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Script"));
}

static inline int script_getVar(lua_State* L) {
    tinyHandle* handle = getScriptHandle(L, 1);
    if (!handle || !lua_isstring(L, 2)) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    const char* varName = lua_tostring(L, 2);
    
    if (comps.script && comps.script->vHas(varName)) {
        const tinyVar& var = comps.script->vMap().at(varName);
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) lua_pushnumber(L, val);
            else if constexpr (std::is_same_v<T, int>) lua_pushinteger(L, val);
            else if constexpr (std::is_same_v<T, bool>) lua_pushboolean(L, val);
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                lua_newtable(L);
                lua_pushnumber(L, val.x); lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y); lua_setfield(L, -2, "y");
                lua_pushnumber(L, val.z); lua_setfield(L, -2, "z");
            }
            else if constexpr (std::is_same_v<T, std::string>) lua_pushstring(L, val.c_str());
            else if constexpr (std::is_same_v<T, typeHandle>) {
                bool isNodeHandle = val.isType<int>();
                uint64_t packed = (isNodeHandle ? (1ULL << 63) : 0ULL) |
                                 (static_cast<uint64_t>(val.handle.index) << 32) | val.handle.version;
                lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
            }
        }, var);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static inline int script_setVar(lua_State* L) {
    tinyHandle* handle = getScriptHandle(L, 1);
    if (!handle || !lua_isstring(L, 2)) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    const char* varName = lua_tostring(L, 2);
    
    if (comps.script && comps.script->vHas(varName)) {
        tinyVar& var = comps.script->vMap().at(varName);
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) val = static_cast<float>(lua_tonumber(L, 3));
            else if constexpr (std::is_same_v<T, int>) val = static_cast<int>(lua_tointeger(L, 3));
            else if constexpr (std::is_same_v<T, bool>) val = static_cast<bool>(lua_toboolean(L, 3));
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                if (lua_istable(L, 3)) {
                    lua_getfield(L, 3, "x"); lua_getfield(L, 3, "y"); lua_getfield(L, 3, "z");
                    val.x = static_cast<float>(lua_tonumber(L, -3));
                    val.y = static_cast<float>(lua_tonumber(L, -2));
                    val.z = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 3);
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) val = std::string(lua_tostring(L, 3));
            else if constexpr (std::is_same_v<T, typeHandle>) {
                if (lua_islightuserdata(L, 3)) {
                    uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L, 3));
                    bool isNodeHandle = (packed & (1ULL << 63)) != 0;
                    tinyHandle h{static_cast<uint32_t>((packed & 0x7FFFFFFFFFFFF000ULL) >> 32), 
                                 static_cast<uint32_t>(packed & 0xFFFFFFFF)};
                    val = isNodeHandle ? typeHandle::make<int>(h) : typeHandle::make<void>(h);
                }
            }
        }, var);
    }
    return 0;
}

static inline int node_script(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (!comps.script) { lua_pushnil(L); return 1; }
    
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = *handle;
    luaL_getmetatable(L, "Script");
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// NODE COMPONENT: HIERARCHY
// ========================================

static inline int node_parent(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyHandle parentHandle = getSceneFromLua(L)->nodeParent(*handle);
    if (parentHandle.valid()) {
        pushNode(L, parentHandle);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static inline int node_children(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    std::vector<tinyHandle> children = getSceneFromLua(L)->nodeChildren(*handle);
    lua_newtable(L);
    for (int i = 0; i < children.size(); i++) {
        pushNode(L, children[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static inline int node_parentHandle(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    tinyHandle parentHandle = getSceneFromLua(L)->nodeParent(*handle);
    if (parentHandle.valid()) {
        uint64_t packed = (1ULL << 63) | (static_cast<uint64_t>(parentHandle.index) << 32) | parentHandle.version;
        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static inline int node_childrenHandles(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) return 0;
    
    std::vector<tinyHandle> children = getSceneFromLua(L)->nodeChildren(*handle);
    lua_newtable(L);
    for (int i = 0; i < children.size(); i++) {
        uint64_t packed = (1ULL << 63) | (static_cast<uint64_t>(children[i].index) << 32) | children[i].version;
        lua_pushlightuserdata(L, reinterpret_cast<void*>(packed));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ========================================
// UTILITY FUNCTIONS
// ========================================

static inline int lua_print(lua_State* L) {
    lua_getglobal(L, "__rtScript");
    void* rtScriptPtr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (!rtScriptPtr) return luaL_error(L, "print(): __rtScript not found");
    
    tinyRT::Script* rtScript = static_cast<tinyRT::Script*>(rtScriptPtr);
    int n = lua_gettop(L);
    std::string message;
    
    for (int i = 1; i <= n; i++) {
        if (i > 1) message += "\t";
        if (lua_isstring(L, i)) message += lua_tostring(L, i);
        else if (lua_isnumber(L, i)) message += std::to_string(lua_tonumber(L, i));
        else if (lua_isboolean(L, i)) message += lua_toboolean(L, i) ? "true" : "false";
        else if (lua_isnil(L, i)) message += "nil";
        else message += lua_typename(L, lua_type(L, i));
    }
    
    rtScript->debug().log(message, 1.0f, 1.0f, 1.0f);
    return 0;
}

// ========================================
// REGISTRATION FUNCTION
// ========================================

// Registration helper macros (only used in this implementation file)
#define LUA_REG_METHOD(func, name) \
    lua_pushcfunction(L, func); lua_setfield(L, -2, name)

#define LUA_REG_GLOBAL(func, name) \
    lua_pushcfunction(L, func); lua_setglobal(L, name)

#define LUA_BEGIN_METATABLE(type_name) \
    luaL_newmetatable(L, type_name); \
    lua_newtable(L)

#define LUA_END_METATABLE(type_name) \
    lua_setfield(L, -2, "__index"); \
    lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushstring(L, type_name); return 1; }); \
    lua_setfield(L, -2, "__tostring"); \
    lua_pop(L, 1)

static inline void registerNodeBindings(lua_State* L) {
    // Transform3D metatable
    LUA_BEGIN_METATABLE("Transform3D");
    LUA_REG_METHOD(transform3d_getPos, "getPos");
    LUA_REG_METHOD(transform3d_setPos, "setPos");
    LUA_REG_METHOD(transform3d_getRot, "getRot");
    LUA_REG_METHOD(transform3d_setRot, "setRot");
    LUA_REG_METHOD(transform3d_getScl, "getScl");
    LUA_REG_METHOD(transform3d_setScl, "setScl");
    LUA_END_METATABLE("Transform3D");
    
    // Anim3D metatable
    LUA_BEGIN_METATABLE("Anim3D");
    LUA_REG_METHOD(anim3d_get, "get");
    LUA_REG_METHOD(anim3d_current, "current");
    LUA_REG_METHOD(anim3d_play, "play");
    LUA_REG_METHOD(anim3d_setSpeed, "setSpeed");
    LUA_REG_METHOD(anim3d_isPlaying, "isPlaying");
    LUA_REG_METHOD(anim3d_getTime, "getTime");
    LUA_REG_METHOD(anim3d_setTime, "setTime");
    LUA_REG_METHOD(anim3d_getDuration, "getDuration");
    LUA_REG_METHOD(anim3d_setLoop, "setLoop");
    LUA_REG_METHOD(anim3d_isLoop, "isLoop");
    LUA_REG_METHOD(anim3d_pause, "pause");
    LUA_REG_METHOD(anim3d_resume, "resume");
    LUA_END_METATABLE("Anim3D");
    
    // Script metatable
    LUA_BEGIN_METATABLE("Script");
    LUA_REG_METHOD(script_getVar, "getVar");
    LUA_REG_METHOD(script_setVar, "setVar");
    LUA_END_METATABLE("Script");
    
    // Node metatable
    LUA_BEGIN_METATABLE("Node");
    LUA_REG_METHOD(node_transform3D, "transform3D");
    LUA_REG_METHOD(node_anim3D, "anim3D");
    LUA_REG_METHOD(node_script, "script");
    LUA_REG_METHOD(node_parent, "parent");
    LUA_REG_METHOD(node_children, "children");
    LUA_REG_METHOD(node_parentHandle, "parentHandle");
    LUA_REG_METHOD(node_childrenHandles, "childrenHandles");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        tinyHandle* h = getNodeHandleFromUserdata(L, 1);
        lua_pushfstring(L, h ? "Node(%d, %d)" : "Node(invalid)", h ? h->index : 0, h ? h->version : 0);
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        tinyHandle* h1 = getNodeHandleFromUserdata(L, 1);
        tinyHandle* h2 = getNodeHandleFromUserdata(L, 2);
        lua_pushboolean(L, h1 && h2 && (*h1 == *h2));
        return 1;
    });
    lua_setfield(L, -2, "__eq");
    lua_pop(L, 1);
    
    // Scene metatable
    LUA_BEGIN_METATABLE("Scene");
    LUA_REG_METHOD(scene_node, "node");
    LUA_REG_METHOD(scene_addScene, "addScene");
    LUA_END_METATABLE("Scene");

    // Global Functions
    LUA_REG_GLOBAL(lua_kState, "kState");
    LUA_REG_GLOBAL(lua_nHandle, "nHandle");
    LUA_REG_GLOBAL(lua_rHandle, "rHandle");
    LUA_REG_GLOBAL(lua_handleEqual, "handleEqual");
    LUA_REG_GLOBAL(lua_print, "print");
}

// Clean up macros (prevent pollution)
#undef LUA_REG_METHOD
#undef LUA_REG_GLOBAL
#undef LUA_BEGIN_METATABLE
#undef LUA_END_METATABLE
