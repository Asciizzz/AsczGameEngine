#pragma once

#include "tinyData/tinyRT_Scene.hpp"
#include "tinyData/tinyRT_Node.hpp"

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
// UNIFIED HANDLE SYSTEM
// ========================================

// LuaHandle - Unified handle structure with type information
struct LuaHandle {
    std::string type = "void";
    tinyHandle handle;

    LuaHandle() = default;
    LuaHandle(const std::string& t, tinyHandle h = tinyHandle()) : type(t), handle(h) {}
    LuaHandle(const std::string& t, uint32_t index, uint32_t version) 
        : type(t), handle(index, version) {}

    bool valid() const { return !type.empty() && handle.valid(); }

    // Convert LuaHandle to typeHandle based on type string
    typeHandle toTypeHandle() const {
        if (type == "node") return typeHandle::make<tinyNodeRT>(handle); else
        if (type == "script") return typeHandle::make<tinyScript>(handle); else
        if (type == "scene") return typeHandle::make<tinySceneRT>(handle); else
        return typeHandle::make<void>(handle);
    }

    // Create LuaHandle from typeHandle
    static LuaHandle fromTypeHandle(const typeHandle& th) {
        if (th.isType<tinyNodeRT>()) return LuaHandle("node", th.handle); else
        if (th.isType<tinyScript>()) return LuaHandle("script", th.handle); else
        if (th.isType<tinySceneRT>()) return LuaHandle("scene", th.handle); else
        return LuaHandle("resource", th.handle);
    }
};

// Push LuaHandle as full userdata with metatable
static inline void pushLuaHandle(lua_State* L, const LuaHandle& luaHandle) {
    LuaHandle* ud = static_cast<LuaHandle*>(lua_newuserdata(L, sizeof(LuaHandle)));
    new (ud) LuaHandle(luaHandle);  // Placement new to copy construct
    luaL_getmetatable(L, "Handle");
    lua_setmetatable(L, -2);
}

// Get LuaHandle from userdata
static inline LuaHandle* getLuaHandleFromUserdata(lua_State* L, int index) {
    return static_cast<LuaHandle*>(luaL_checkudata(L, index, "Handle"));
}

// ========================================
// HANDLE CONSTRUCTORS & UTILITIES
// ========================================

// Handle("type") or Handle("type", index, version)
static inline int lua_Handle(lua_State* L) {
    int numArgs = lua_gettop(L);
    
    if (numArgs < 1) {
        LuaHandle handle;
        handle.type = "node";
        pushLuaHandle(L, handle);
        return 1;
    }

    const char* typeStr = luaL_checkstring(L, 1);
    
    LuaHandle luaHandle;
    luaHandle.type = typeStr;
    
    if (numArgs >= 2) {
        luaHandle.handle.index = luaL_checkinteger(L, 2);
        if (numArgs >= 3) {
            luaHandle.handle.version = luaL_checkinteger(L, 3);
        }
    }
    
    pushLuaHandle(L, luaHandle);
    return 1;
}

static inline int lua_handleEqual(lua_State* L) {
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    LuaHandle* h1 = getLuaHandleFromUserdata(L, 1);
    LuaHandle* h2 = getLuaHandleFromUserdata(L, 2);
    
    if (!h1 || !h2) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    // Handles are equal if both type and handle match
    lua_pushboolean(L, h1->type == h2->type && h1->handle == h2->handle);
    return 1;
}

// Handle:type() - Get the type string of a handle
static inline int lua_handleGetType(lua_State* L) {
    LuaHandle* h = getLuaHandleFromUserdata(L, 1);
    if (!h) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, h->type.c_str());
    return 1;
}

// Handle:index() - Get the index of a handle
static inline int lua_handleGetIndex(lua_State* L) {
    LuaHandle* h = getLuaHandleFromUserdata(L, 1);
    if (!h) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, h->handle.index);
    return 1;
}

// Handle:version() - Get the version of a handle
static inline int lua_handleGetVersion(lua_State* L) {
    LuaHandle* h = getLuaHandleFromUserdata(L, 1);
    if (!h) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, h->handle.version);
    return 1;
}

// Handle:valid() - Check if handle is valid
static inline int lua_handleValid(lua_State* L) {
    LuaHandle* h = getLuaHandleFromUserdata(L, 1);
    if (!h) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, h->valid());
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
    
    LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 2);
    if (!luaHandle) {
        lua_pushnil(L);
        return 1;
    }
    
    // Verify it's a node handle - return nil if wrong type instead of erroring
    if (luaHandle->type != "node") {
        lua_pushnil(L);
        return 1;
    }
    
    if (!luaHandle->handle.valid() || !(*scenePtr)->node(luaHandle->handle)) {
        lua_pushnil(L);
        return 1;
    }
    
    pushNode(L, luaHandle->handle);
    return 1;
}

static inline int scene_addScene(lua_State* L) {
    tinyRT::Scene** scenePtr = getSceneFromUserdata(L, 1);
    if (!scenePtr || !*scenePtr)
        return luaL_error(L, "Invalid scene");
    
    // Argument 2: scene handle (registry handle to Scene resource)
    LuaHandle* sceneHandle = getLuaHandleFromUserdata(L, 2);
    if (!sceneHandle || sceneHandle->type != "scene") {
        // Silently fail - don't halt the script
        return 0;
    }
    
    // Argument 3: node handle for parent - optional
    tinyHandle parentNHandle;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        LuaHandle* parentHandle = getLuaHandleFromUserdata(L, 3);
        if (!parentHandle || parentHandle->type != "node") {
            // Silently fail - don't halt the script
            return 0;
        }
        
        parentNHandle = parentHandle->handle;
    }
    // If no parent provided, it will default to root in the C++ function
    
    // Call the existing C++ addScene function
    // The existing function uses sharedRes_.fsGet<Scene>(sceneHandle) internally
    (*scenePtr)->addScene(sceneHandle->handle, parentNHandle);
    
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

static inline int transform3d_getQuat(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        
        // Return quaternion as table {x, y, z, w}
        lua_newtable(L);
        lua_pushnumber(L, rot.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, rot.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, rot.z); lua_setfield(L, -2, "z");
        lua_pushnumber(L, rot.w); lua_setfield(L, -2, "w");
        return 1;
    }
    return 0;
}

static inline int transform3d_setQuat(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle || !lua_istable(L, 2)) return 0;
    
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z"); lua_getfield(L, 2, "w");
    glm::quat quat(
        static_cast<float>(lua_tonumber(L, -1)), // w
        static_cast<float>(lua_tonumber(L, -4)), // x
        static_cast<float>(lua_tonumber(L, -3)), // y
        static_cast<float>(lua_tonumber(L, -2))  // z
    );
    lua_pop(L, 4);
    
    auto comps = getSceneFromLua(L)->nComp(*handle);
    if (comps.trfm3D) {
        glm::vec3 pos, scale, skew;
        glm::quat rot;
        glm::vec4 persp;
        glm::decompose(comps.trfm3D->local, scale, rot, pos, skew, persp);
        comps.trfm3D->local = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(quat) * glm::scale(glm::mat4(1.0f), scale);
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
            pushLuaHandle(L, LuaHandle("animation", animHandle));
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
            pushLuaHandle(L, LuaHandle("animation", animHandle));
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
    } else {
        LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 2);
        if (!luaHandle) return 0;
        // Accept animation handles
        if (luaHandle->type != "animation") {
            return luaL_error(L, "anim3d:play() expects animation handle, got '%s'", luaHandle->type.c_str());
        }
        animHandle = luaHandle->handle;
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
    
    // Check if second argument is provided
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 2);
        if (luaHandle && luaHandle->type == "animation") {
            animHandle = luaHandle->handle;
        } else {
            animHandle = comps.anim3D->curHandle();
        }
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
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                lua_newtable(L);
                lua_pushnumber(L, val.x); lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y); lua_setfield(L, -2, "y");
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                lua_newtable(L);
                lua_pushnumber(L, val.x); lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y); lua_setfield(L, -2, "y");
                lua_pushnumber(L, val.z); lua_setfield(L, -2, "z");
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                lua_newtable(L);
                lua_pushnumber(L, val.x); lua_setfield(L, -2, "x");
                lua_pushnumber(L, val.y); lua_setfield(L, -2, "y");
                lua_pushnumber(L, val.z); lua_setfield(L, -2, "z");
                lua_pushnumber(L, val.w); lua_setfield(L, -2, "w");
            }
            else if constexpr (std::is_same_v<T, std::string>) lua_pushstring(L, val.c_str());
            else if constexpr (std::is_same_v<T, typeHandle>) {
                // Convert typeHandle back to LuaHandle
                pushLuaHandle(L, LuaHandle::fromTypeHandle(val));
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
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                if (lua_istable(L, 3)) {
                    lua_getfield(L, 3, "x"); lua_getfield(L, 3, "y");
                    val.x = static_cast<float>(lua_tonumber(L, -2));
                    val.y = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 2);
                }
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                if (lua_istable(L, 3)) {
                    lua_getfield(L, 3, "x"); lua_getfield(L, 3, "y"); lua_getfield(L, 3, "z");
                    val.x = static_cast<float>(lua_tonumber(L, -3));
                    val.y = static_cast<float>(lua_tonumber(L, -2));
                    val.z = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 3);
                }
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                if (lua_istable(L, 3)) {
                    lua_getfield(L, 3, "x"); lua_getfield(L, 3, "y"); lua_getfield(L, 3, "z"); lua_getfield(L, 3, "w");
                    val.x = static_cast<float>(lua_tonumber(L, -4));
                    val.y = static_cast<float>(lua_tonumber(L, -3));
                    val.z = static_cast<float>(lua_tonumber(L, -2));
                    val.w = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 4);
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) val = std::string(lua_tostring(L, 3));
            else if constexpr (std::is_same_v<T, typeHandle>) {
                LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 3);
                if (luaHandle) {
                    val = luaHandle->toTypeHandle();
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
        pushLuaHandle(L, LuaHandle("node", parentHandle));
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
        pushLuaHandle(L, LuaHandle("node", children[i]));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ========================================
// FS (FILESYSTEM REGISTRY) OBJECT
// ========================================

// Helper to get tinyRegistry pointer from __scene
static inline tinyRegistry* getFSRegistryFromLua(lua_State* L) {
    tinyRT::Scene* scene = getSceneFromLua(L);
    if (!scene) return nullptr;
    return scene->sharedRes().fsRegistry;
}

// Helper to get FS userdata (just a marker, we access registry through __scene)
static inline void** getFSUserdata(lua_State* L, int index) {
    return static_cast<void**>(luaL_checkudata(L, index, "FS"));
}

// Push FS object (just a marker userdata)
static inline void pushFS(lua_State* L) {
    void** ud = static_cast<void**>(lua_newuserdata(L, sizeof(void*)));
    *ud = nullptr; // FS is just a marker, we access registry through scene
    luaL_getmetatable(L, "FS");
    lua_setmetatable(L, -2);
}

// fs:get(handle) - Get resource from filesystem registry
static inline int fs_get(lua_State* L) {
    // Argument 1: self (FS object)
    // Argument 2: handle to resource
    
    if (lua_gettop(L) < 2) {
        lua_pushnil(L);
        return 1;
    }
    
    LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 2);
    if (!luaHandle || !luaHandle->valid()) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyRegistry* registry = getFSRegistryFromLua(L);
    if (!registry) {
        lua_pushnil(L);
        return 1;
    }
    
    // Handle different resource types based on handle type
    // "resource" is a generic type used when we can't determine the exact type
    if (luaHandle->type == "script" || luaHandle->type == "resource") {
        // Try to get as script first (most common for generic "resource" type)
        const tinyScript* script = registry->get<tinyScript>(luaHandle->handle);
        
        if (!script || !script->valid()) {
            lua_pushnil(L);
            return 1;
        }
        
        // Push the script handle as a StaticScript userdata
        tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
        *ud = luaHandle->handle;
        luaL_getmetatable(L, "StaticScript");
        lua_setmetatable(L, -2);
        return 1;
    }
    // Add more resource types here as needed (scene, mesh, texture, etc.)
    else {
        return luaL_error(L, "FS:get() does not support resource type '%s' yet", luaHandle->type.c_str());
    }
}

// ========================================
// STATICSCRIPT (SCRIPT RESOURCE) OBJECT
// ========================================

// Helper to get script handle from StaticScript userdata
static inline tinyHandle* getStaticScriptHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "StaticScript"));
}

// staticScript:call(funcName, ...) - Call a function on the static script
static inline int staticscript_call(lua_State* L) {
    // Safely get script handle without throwing
    if (!lua_isuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyHandle* scriptHandle = static_cast<tinyHandle*>(lua_touserdata(L, 1));
    if (!scriptHandle || !lua_isstring(L, 2)) {
        lua_pushnil(L);
        return 1;
    }
    
    const char* funcName = lua_tostring(L, 2);
    tinyRegistry* registry = getFSRegistryFromLua(L);
    
    if (!registry) {
        lua_pushnil(L);
        return 1;
    }
    
    const tinyScript* script = registry->get<tinyScript>(*scriptHandle);
    
    if (!script || !script->valid()) {
        lua_pushnil(L);
        return 1;
    }
    
    // Get the script's Lua state
    lua_State* scriptL = script->luaState();
    if (!scriptL) {
        lua_pushnil(L);
        return 1;
    }
    
    // Ensure the target script has access to the same scene context
    // Transfer the __scene global from calling state to target state
    lua_getglobal(L, "__scene");
    if (lua_isuserdata(L, -1)) {
        void* scenePtr = lua_touserdata(L, -1);
        lua_pushlightuserdata(scriptL, scenePtr);
        lua_setglobal(scriptL, "__scene");
    }
    lua_pop(L, 1);
    
    // Also transfer __rtScript if it exists (needed for print function)
    lua_getglobal(L, "__rtScript");
    if (lua_isuserdata(L, -1)) {
        void* rtScriptPtr = lua_touserdata(L, -1);
        lua_pushlightuserdata(scriptL, rtScriptPtr);
        lua_setglobal(scriptL, "__rtScript");
    }
    lua_pop(L, 1);
    
    // Get the function from the script's Lua state
    lua_getglobal(scriptL, funcName);
    if (!lua_isfunction(scriptL, -1)) {
        lua_pop(scriptL, 1);
        lua_pushnil(L);
        return 1;
    }
    
    // Transfer arguments from calling state to script state
    int numArgs = lua_gettop(L) - 2;  // Subtract self and funcName
    for (int i = 3; i <= lua_gettop(L); i++) {
        if (lua_isnumber(L, i)) {
            lua_pushnumber(scriptL, lua_tonumber(L, i));
        } else if (lua_isboolean(L, i)) {
            lua_pushboolean(scriptL, lua_toboolean(L, i));
        } else if (lua_isstring(L, i)) {
            lua_pushstring(scriptL, lua_tostring(L, i));
        } else if (lua_istable(L, i)) {
            // Transfer table generically (supports vec3 and other tables)
            lua_newtable(scriptL);
            lua_pushnil(L);  // First key for iteration
            while (lua_next(L, i) != 0) {
                // Key is at -2, value is at -1
                bool validKeyValue = false;
                
                // Copy key to scriptL
                if (lua_isstring(L, -2)) {
                    lua_pushstring(scriptL, lua_tostring(L, -2));
                    validKeyValue = true;
                } else if (lua_isnumber(L, -2)) {
                    lua_pushnumber(scriptL, lua_tonumber(L, -2));
                    validKeyValue = true;
                }
                
                if (!validKeyValue) {
                    lua_pop(L, 1); // Pop value, keep key for next iteration
                    continue;
                }
                
                // Copy value to scriptL
                if (lua_isnumber(L, -1)) {
                    lua_pushnumber(scriptL, lua_tonumber(L, -1));
                } else if (lua_isboolean(L, -1)) {
                    lua_pushboolean(scriptL, lua_toboolean(L, -1));
                } else if (lua_isstring(L, -1)) {
                    lua_pushstring(scriptL, lua_tostring(L, -1));
                } else {
                    // Invalid value type - pop the key we already pushed to scriptL
                    lua_pop(scriptL, 1); // Pop key from scriptL
                    lua_pop(L, 1); // Pop value from L, keep key for next iteration
                    continue;
                }
                
                // Both key and value are valid, set in table
                lua_settable(scriptL, -3);
                lua_pop(L, 1);  // Pop value from L, keep key for next iteration
            }
        } else if (lua_isuserdata(L, i)) {
            // Check if it's a Handle userdata by checking metatable
            if (lua_getmetatable(L, i)) {
                luaL_getmetatable(L, "Handle");
                bool isHandle = lua_rawequal(L, -1, -2);
                lua_pop(L, 2); // Pop both metatables
                
                if (isHandle) {
                    // Transfer handles - CORRECTLY copy the full LuaHandle struct
                    LuaHandle* sourceHandle = static_cast<LuaHandle*>(lua_touserdata(L, i));
                    if (sourceHandle) {
                        // Create new LuaHandle userdata in target state with correct size
                        pushLuaHandle(scriptL, *sourceHandle);
                    } else {
                        lua_pushnil(scriptL);
                    }
                } else {
                    lua_pushnil(scriptL);
                }
            } else {
                lua_pushnil(scriptL);
            }
        } else {
            lua_pushnil(scriptL);
        }
    }
    
    // Call the function
    if (lua_pcall(scriptL, numArgs, LUA_MULTRET, 0) != LUA_OK) {
        const char* error = lua_tostring(scriptL, -1);
        
        // Try to log the error if print is available
        lua_getglobal(L, "print");
        if (lua_isfunction(L, -1)) {
            std::string errorMsg = std::string("StaticScript:call() error: ") + (error ? error : "unknown error");
            lua_pushstring(L, errorMsg.c_str());
            lua_pcall(L, 1, 0, 0);
        } else {
            lua_pop(L, 1);
        }
        
        lua_pop(scriptL, 1);
        lua_settop(scriptL, 0);
        lua_pushnil(L);
        return 1;
    }
    
    // Transfer return values back to calling state
    int numReturns = lua_gettop(scriptL);
    
    // We need to be careful with stack indices during table iteration
    // Store the initial stack top to calculate correct indices
    int scriptLBase = lua_gettop(scriptL);
    
    for (int i = 1; i <= numReturns; i++) {
        // Use absolute index from the base
        int absIndex = i;
        
        if (lua_isnumber(scriptL, absIndex)) {
            lua_pushnumber(L, lua_tonumber(scriptL, absIndex));
        } else if (lua_isboolean(scriptL, absIndex)) {
            lua_pushboolean(L, lua_toboolean(scriptL, absIndex));
        } else if (lua_isstring(scriptL, absIndex)) {
            lua_pushstring(L, lua_tostring(scriptL, absIndex));
        } else if (lua_istable(scriptL, absIndex)) {
            // Transfer table generically back to calling state
            lua_newtable(L);
            lua_pushnil(scriptL);  // First key for iteration
            
            // Adjust the table index because we pushed nil, making the stack deeper
            int tableIndex = absIndex;
            if (absIndex > 0 && absIndex <= scriptLBase) {
                // Use absolute index adjustment for positive indices
                // The table is still at the same absolute position
                tableIndex = absIndex;
            }
            
            while (lua_next(scriptL, tableIndex) != 0) {
                // Key is at -2, value is at -1 (in scriptL)
                bool validKeyValue = false;
                
                // Copy key to calling state
                if (lua_isstring(scriptL, -2)) {
                    lua_pushstring(L, lua_tostring(scriptL, -2));
                    validKeyValue = true;
                } else if (lua_isnumber(scriptL, -2)) {
                    lua_pushnumber(L, lua_tonumber(scriptL, -2));
                    validKeyValue = true;
                }
                
                if (!validKeyValue) {
                    lua_pop(scriptL, 1); // Pop value, keep key for next iteration
                    continue;
                }
                
                // Copy value to calling state
                if (lua_isnumber(scriptL, -1)) {
                    lua_pushnumber(L, lua_tonumber(scriptL, -1));
                } else if (lua_isboolean(scriptL, -1)) {
                    lua_pushboolean(L, lua_toboolean(scriptL, -1));
                } else if (lua_isstring(scriptL, -1)) {
                    lua_pushstring(L, lua_tostring(scriptL, -1));
                } else {
                    // Invalid value type - pop the key we already pushed to L
                    lua_pop(L, 1); // Pop key from calling state
                    lua_pop(scriptL, 1); // Pop value from scriptL, keep key for next iteration
                    continue;
                }
                
                // Both key and value are valid, set in table
                lua_settable(L, -3);
                lua_pop(scriptL, 1);  // Pop value from scriptL, keep key for next iteration
            }
        } else if (lua_isuserdata(scriptL, absIndex)) {
            // Check if it's a Handle userdata by checking metatable
            if (lua_getmetatable(scriptL, absIndex)) {
                luaL_getmetatable(scriptL, "Handle");
                bool isHandle = lua_rawequal(scriptL, -1, -2);
                lua_pop(scriptL, 2); // Pop both metatables
                
                if (isHandle) {
                    // Transfer handles back - CORRECTLY copy the full LuaHandle struct
                    LuaHandle* sourceHandle = static_cast<LuaHandle*>(lua_touserdata(scriptL, absIndex));
                    if (sourceHandle) {
                        // Create new LuaHandle userdata in calling state with correct size
                        pushLuaHandle(L, *sourceHandle);
                    } else {
                        lua_pushnil(L);
                    }
                } else {
                    lua_pushnil(L);
                }
            } else {
                lua_pushnil(L);
            }
        } else {
            lua_pushnil(L);
        }
    }
    
    // Clean up script's stack
    lua_settop(scriptL, 0);
    
    return numReturns;
}

// ========================================
// UTILITY FUNCTIONS
// ========================================
// ========== Quaternion Utility Functions ==========

// quat_slerp(q1, q2, t) - Spherical linear interpolation between two quaternions
static inline int lua_quat_slerp(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isnumber(L, 3)) {
        return luaL_error(L, "quat_slerp requires (quat1, quat2, t)");
    }
    
    // Read first quaternion
    lua_getfield(L, 1, "x"); lua_getfield(L, 1, "y"); lua_getfield(L, 1, "z"); lua_getfield(L, 1, "w");
    glm::quat q1(
        static_cast<float>(lua_tonumber(L, -1)), // w
        static_cast<float>(lua_tonumber(L, -4)), // x
        static_cast<float>(lua_tonumber(L, -3)), // y
        static_cast<float>(lua_tonumber(L, -2))  // z
    );
    lua_pop(L, 4);
    
    // Read second quaternion
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z"); lua_getfield(L, 2, "w");
    glm::quat q2(
        static_cast<float>(lua_tonumber(L, -1)), // w
        static_cast<float>(lua_tonumber(L, -4)), // x
        static_cast<float>(lua_tonumber(L, -3)), // y
        static_cast<float>(lua_tonumber(L, -2))  // z
    );
    lua_pop(L, 4);
    
    float t = static_cast<float>(lua_tonumber(L, 3));
    
    // Perform slerp
    glm::quat result = glm::slerp(q1, q2, t);
    
    // Return result
    lua_newtable(L);
    lua_pushnumber(L, result.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, result.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, result.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, result.w); lua_setfield(L, -2, "w");
    return 1;
}

// quat_fromAxisAngle(axis, angle) - Create quaternion from axis-angle representation
static inline int lua_quat_fromAxisAngle(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "quat_fromAxisAngle requires (axis_vec3, angle_radians)");
    }
    
    // Read axis
    lua_getfield(L, 1, "x"); lua_getfield(L, 1, "y"); lua_getfield(L, 1, "z");
    glm::vec3 axis(
        static_cast<float>(lua_tonumber(L, -3)),
        static_cast<float>(lua_tonumber(L, -2)),
        static_cast<float>(lua_tonumber(L, -1))
    );
    lua_pop(L, 3);
    
    float angle = static_cast<float>(lua_tonumber(L, 2));
    
    // Create quaternion from axis-angle
    glm::quat result = glm::angleAxis(angle, glm::normalize(axis));
    
    // Return result
    lua_newtable(L);
    lua_pushnumber(L, result.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, result.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, result.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, result.w); lua_setfield(L, -2, "w");
    return 1;
}

// quat_fromEuler(euler) - Create quaternion from Euler angles (vec3)
static inline int lua_quat_fromEuler(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "quat_fromEuler requires (euler_vec3)");
    }
    
    // Read euler angles
    lua_getfield(L, 1, "x"); lua_getfield(L, 1, "y"); lua_getfield(L, 1, "z");
    glm::vec3 euler(
        static_cast<float>(lua_tonumber(L, -3)),
        static_cast<float>(lua_tonumber(L, -2)),
        static_cast<float>(lua_tonumber(L, -1))
    );
    lua_pop(L, 3);
    
    // Create quaternion from euler
    glm::quat result = glm::quat(euler);
    
    // Return result
    lua_newtable(L);
    lua_pushnumber(L, result.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, result.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, result.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, result.w); lua_setfield(L, -2, "w");
    return 1;
}

// quat_toEuler(quat) - Convert quaternion to Euler angles (returns vec3)
static inline int lua_quat_toEuler(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "quat_toEuler requires (quat)");
    }
    
    // Read quaternion
    lua_getfield(L, 1, "x"); lua_getfield(L, 1, "y"); lua_getfield(L, 1, "z"); lua_getfield(L, 1, "w");
    glm::quat q(
        static_cast<float>(lua_tonumber(L, -1)), // w
        static_cast<float>(lua_tonumber(L, -4)), // x
        static_cast<float>(lua_tonumber(L, -3)), // y
        static_cast<float>(lua_tonumber(L, -2))  // z
    );
    lua_pop(L, 4);
    
    // Convert to euler
    glm::vec3 euler = glm::eulerAngles(q);
    
    // Return result
    lua_newtable(L);
    lua_pushnumber(L, euler.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, euler.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, euler.z); lua_setfield(L, -2, "z");
    return 1;
}

// quat_lookAt(forward, up) - Create a quaternion that looks in the forward direction
static inline int lua_quat_lookAt(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "quat_lookAt requires (forward_vec3, up_vec3)");
    }
    
    // Read forward vector
    lua_getfield(L, 1, "x"); lua_getfield(L, 1, "y"); lua_getfield(L, 1, "z");
    glm::vec3 forward(
        static_cast<float>(lua_tonumber(L, -3)),
        static_cast<float>(lua_tonumber(L, -2)),
        static_cast<float>(lua_tonumber(L, -1))
    );
    lua_pop(L, 3);
    
    // Read up vector
    lua_getfield(L, 2, "x"); lua_getfield(L, 2, "y"); lua_getfield(L, 2, "z");
    glm::vec3 up(
        static_cast<float>(lua_tonumber(L, -3)),
        static_cast<float>(lua_tonumber(L, -2)),
        static_cast<float>(lua_tonumber(L, -1))
    );
    lua_pop(L, 3);
    
    // Create look-at quaternion
    glm::quat result = glm::quatLookAt(glm::normalize(forward), glm::normalize(up));
    
    // Return result
    lua_newtable(L);
    lua_pushnumber(L, result.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, result.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, result.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, result.w); lua_setfield(L, -2, "w");
    return 1;
}

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
    LUA_REG_METHOD(transform3d_getQuat, "getQuat");
    LUA_REG_METHOD(transform3d_setQuat, "setQuat");
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
    
    // FS metatable (filesystem registry accessor)
    LUA_BEGIN_METATABLE("FS");
    LUA_REG_METHOD(fs_get, "get");
    LUA_END_METATABLE("FS");
    
    // StaticScript metatable (script resource from registry)
    LUA_BEGIN_METATABLE("StaticScript");
    LUA_REG_METHOD(staticscript_call, "call");
    LUA_END_METATABLE("StaticScript");
    
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
    
    // Handle metatable (unified handle system)
    luaL_newmetatable(L, "Handle");
    lua_newtable(L); // __index table for methods
    LUA_REG_METHOD(lua_handleGetType, "type");
    LUA_REG_METHOD(lua_handleGetIndex, "index");
    LUA_REG_METHOD(lua_handleGetVersion, "version");
    LUA_REG_METHOD(lua_handleValid, "valid");
    lua_setfield(L, -2, "__index");
    // __eq operator for handle comparison
    lua_pushcfunction(L, lua_handleEqual);
    lua_setfield(L, -2, "__eq");
    // __tostring for debugging
    lua_pushcfunction(L, [](lua_State* L) -> int {
        LuaHandle* h = getLuaHandleFromUserdata(L, 1);
        if (!h) {
            lua_pushstring(L, "Handle(invalid)");
            return 1;
        }
        lua_pushfstring(L, "Handle(\"%s\", %d, %d)", h->type.c_str(), h->handle.index, h->handle.version);
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    // __gc for cleanup (string destructor)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        LuaHandle* h = getLuaHandleFromUserdata(L, 1);
        if (h) h->~LuaHandle(); // Call destructor explicitly
        return 0;
    });
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // Global Functions
    LUA_REG_GLOBAL(lua_kState, "kState");
    LUA_REG_GLOBAL(lua_Handle, "Handle");
    LUA_REG_GLOBAL(lua_print, "print");
    
    // Quaternion Utility Functions
    LUA_REG_GLOBAL(lua_quat_slerp, "quat_slerp");
    LUA_REG_GLOBAL(lua_quat_fromAxisAngle, "quat_fromAxisAngle");
    LUA_REG_GLOBAL(lua_quat_fromEuler, "quat_fromEuler");
    LUA_REG_GLOBAL(lua_quat_toEuler, "quat_toEuler");
    LUA_REG_GLOBAL(lua_quat_lookAt, "quat_lookAt");
}

// Clean up macros (prevent pollution)
#undef LUA_REG_METHOD
#undef LUA_REG_GLOBAL
#undef LUA_BEGIN_METATABLE
#undef LUA_END_METATABLE
