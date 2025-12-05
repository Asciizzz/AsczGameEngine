#pragma once

#include "rtScene.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <SDL2/SDL.h>

#include <algorithm>

extern "C" {
    #include "lua.h"
}

// ========================================
// HELPER FUNCTIONS
// ========================================

static inline rtScene* getSceneFromLua(lua_State* L) {
    lua_getglobal(L, "__scene");
    void* ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<rtScene*>(ptr);
}

static inline void decomposeMatrix(const glm::mat4& mat, glm::vec3& pos, glm::quat& rot, glm::vec3& scale) {
    glm::vec3 skew;
    glm::vec4 persp;
    glm::decompose(mat, scale, rot, pos, skew, persp);
}

static inline glm::mat4 composeMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale) {
    return glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.0f), scale);
}

static inline tinyHandle* getNodeHandleFromUserdata(lua_State* L, int index) {
    void* ud = luaL_testudata(L, index, "Node");
    return ud ? static_cast<tinyHandle*>(ud) : nullptr;
}

static inline rtScene** getSceneFromUserdata(lua_State* L, int index) {
    return static_cast<rtScene**>(luaL_checkudata(L, index, "Scene"));
}

static inline void pushNode(lua_State* L, tinyHandle handle) {
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = handle;
    luaL_getmetatable(L, "Node");
    lua_setmetatable(L, -2);
}

static inline void pushScene(lua_State* L, rtScene* scene) {
    rtScene** ud = static_cast<rtScene**>(lua_newuserdata(L, sizeof(rtScene*)));
    *ud = scene;
    luaL_getmetatable(L, "Scene");
    lua_setmetatable(L, -2);
}

// ========================================
// VECTOR TYPES
// ========================================

static inline void pushVec2(lua_State* L, const glm::vec2& vec) {
    glm::vec2* ud = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *ud = vec;
    luaL_getmetatable(L, "Vec2");
    lua_setmetatable(L, -2);
}

static inline void pushVec3(lua_State* L, const glm::vec3& vec) {
    glm::vec3* ud = static_cast<glm::vec3*>(lua_newuserdata(L, sizeof(glm::vec3)));
    *ud = vec;
    luaL_getmetatable(L, "Vec3");
    lua_setmetatable(L, -2);
}

static inline void pushVec4(lua_State* L, const glm::vec4& vec) {
    glm::vec4* ud = static_cast<glm::vec4*>(lua_newuserdata(L, sizeof(glm::vec4)));
    *ud = vec;
    luaL_getmetatable(L, "Vec4");
    lua_setmetatable(L, -2);
}

static inline glm::vec2* getVec2(lua_State* L, int index) {
    return static_cast<glm::vec2*>(luaL_checkudata(L, index, "Vec2"));
}

static inline glm::vec3* getVec3(lua_State* L, int index) {
    return static_cast<glm::vec3*>(luaL_checkudata(L, index, "Vec3"));
}

static inline glm::vec4* getVec4(lua_State* L, int index) {
    return static_cast<glm::vec4*>(luaL_checkudata(L, index, "Vec4"));
}

static inline int lua_Vec2(lua_State* L) {
    float x = luaL_optnumber(L, 1, 0.0f);
    float y = luaL_optnumber(L, 2, 0.0f);
    pushVec2(L, glm::vec2(x, y));
    return 1;
}

static inline int lua_Vec3(lua_State* L) {
    float x = luaL_optnumber(L, 1, 0.0f);
    float y = luaL_optnumber(L, 2, 0.0f);
    float z = luaL_optnumber(L, 3, 0.0f);
    pushVec3(L, glm::vec3(x, y, z));
    return 1;
}

static inline int lua_Vec4(lua_State* L) {
    float x = luaL_optnumber(L, 1, 0.0f);
    float y = luaL_optnumber(L, 2, 0.0f);
    float z = luaL_optnumber(L, 3, 0.0f);
    float w = luaL_optnumber(L, 4, 0.0f);
    pushVec4(L, glm::vec4(x, y, z, w));
    return 1;
}

static inline int vec2_index(lua_State* L) {
    glm::vec2* v = getVec2(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    
    if (strcmp(key, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
    if (strcmp(key, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
    
    return 0;
}

static inline int vec2_newindex(lua_State* L) {
    glm::vec2* v = getVec2(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key || !lua_isnumber(L, 3)) return 0;
    
    float value = static_cast<float>(lua_tonumber(L, 3));
    
    if (strcmp(key, "x") == 0) { v->x = value; return 0; }
    if (strcmp(key, "y") == 0) { v->y = value; return 0; }
    
    return 0;
}
static inline int vec2_tostring(lua_State* L) {
    glm::vec2* v = getVec2(L, 1);
    if (!v) return 0;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Vec2(%.3f, %.3f)", v->x, v->y);
    lua_pushstring(L, buffer);
    return 1;
}

static inline int vec3_index(lua_State* L) {
    glm::vec3* v = getVec3(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    
    if (strcmp(key, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
    if (strcmp(key, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
    if (strcmp(key, "z") == 0) { lua_pushnumber(L, v->z); return 1; }
    
    return 0;
}

static inline int vec3_newindex(lua_State* L) {
    glm::vec3* v = getVec3(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key || !lua_isnumber(L, 3)) return 0;
    
    float value = static_cast<float>(lua_tonumber(L, 3));
    
    if (strcmp(key, "x") == 0) { v->x = value; return 0; }
    if (strcmp(key, "y") == 0) { v->y = value; return 0; }
    if (strcmp(key, "z") == 0) { v->z = value; return 0; }
    
    return 0;
}

static inline int vec3_tostring(lua_State* L) {
    glm::vec3* v = getVec3(L, 1);
    if (!v) return 0;
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "Vec3(%.3f, %.3f, %.3f)", v->x, v->y, v->z);
    lua_pushstring(L, buffer);
    return 1;
}

static inline int vec4_index(lua_State* L) {
    glm::vec4* v = getVec4(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    
    if (strcmp(key, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
    if (strcmp(key, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
    if (strcmp(key, "z") == 0) { lua_pushnumber(L, v->z); return 1; }
    if (strcmp(key, "w") == 0) { lua_pushnumber(L, v->w); return 1; }
    
    return 0;
}

static inline int vec4_newindex(lua_State* L) {
    glm::vec4* v = getVec4(L, 1);
    if (!v) return 0;
    
    const char* key = lua_tostring(L, 2);
    if (!key || !lua_isnumber(L, 3)) return 0;
    
    float value = static_cast<float>(lua_tonumber(L, 3));
    
    if (strcmp(key, "x") == 0) { v->x = value; return 0; }
    if (strcmp(key, "y") == 0) { v->y = value; return 0; }
    if (strcmp(key, "z") == 0) { v->z = value; return 0; }
    if (strcmp(key, "w") == 0) { v->w = value; return 0; }
    
    return 0;
}

static inline int vec4_tostring(lua_State* L) {
    glm::vec4* v = getVec4(L, 1);
    if (!v) return 0;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Vec4(%.3f, %.3f, %.3f, %.3f)", v->x, v->y, v->z, v->w);
    lua_pushstring(L, buffer);
    return 1;
}

// ========================================
// HANDLE SYSTEM
// ========================================

// Push tinyHandle as full userdata with metatable
static inline void pushHandle(lua_State* L, const tinyHandle& handle) {
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    new (ud) tinyHandle(handle);  // Placement new to copy construct
    luaL_getmetatable(L, "Handle");
    lua_setmetatable(L, -2);
}

// Get tinyHandle from userdata
static inline tinyHandle* getHandleFromUserdata(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Handle"));
}

// ========================================
// HANDLE CONSTRUCTORS & UTILITIES
// ========================================

// Handle() - Create an empty handle
static inline int lua_Handle(lua_State* L) {
    tinyHandle handle;  // Default empty handle
    pushHandle(L, handle);
    return 1;
}

static inline int lua_handleEqual(lua_State* L) {
    if (lua_isnil(L, 1) || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    tinyHandle* h1 = getHandleFromUserdata(L, 1);
    tinyHandle* h2 = getHandleFromUserdata(L, 2);

    lua_pushboolean(L, h1 && h2 && *h1 == *h2);
    return 1;
}

static inline int lua_handleToString(lua_State* L) {
    tinyHandle* h = getHandleFromUserdata(L, 1);
    if (!h || !h->valid()) {
        lua_pushstring(L, "Handle()");
        return 1;
    }
    lua_pushfstring(L, "Handle(%d:%d:%d)", h->tID(), h->idx(), h->ver());
    return 1;
}

static inline int lua_handleGC(lua_State* L) {
    tinyHandle* h = getHandleFromUserdata(L, 1);
    if (h) h->~tinyHandle();
    return 0;
}

// ========================================
// SCENE METHODS
// ========================================

// Scene:node(handle) - Get node from handle
static inline int scene_node(lua_State* L) {
    rtScene** scenePtr = getSceneFromUserdata(L, 1);
    if (!scenePtr || !*scenePtr)
        return luaL_error(L, "Invalid scene");
    
    if (lua_isnil(L, 2)) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyHandle* handle = getHandleFromUserdata(L, 2);
    if (!handle || !handle->valid()) {
        lua_pushnil(L);
        return 1;
    }

    // Verify the node exists in the scene
    if (!(*scenePtr)->node(*handle)) {
        lua_pushnil(L);
        return 1;
    }

    pushNode(L, *handle);
    return 1;
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

static inline int lua_KState(lua_State* L) {
    if (!lua_isstring(L, 1))
        return luaL_error(L, "KState expects string");
    
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

    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        pushVec3(L, pos);
        return 1;
    }
    return 0;
}

static inline int transform3d_setPos(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    glm::vec3* newPos = getVec3(L, 2);
    if (!newPos) return luaL_error(L, "setPos expects Vec3");
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        trfm3D->local = composeMatrix(*newPos, rot, scale);
    }
    return 0;
}

static inline int transform3d_rotX(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        
        glm::quat rotX = glm::angleAxis(glm::radians(degrees), glm::vec3(1.0f, 0.0f, 0.0f));
        trfm3D->local = composeMatrix(pos, rotX * rot, scale);
    }
    return 0;
}

static inline int transform3d_rotY(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        
        glm::quat rotY = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
        trfm3D->local = composeMatrix(pos, rotY * rot, scale);
    }
    return 0;
}

static inline int transform3d_rotZ(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        
        glm::quat rotZ = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 0.0f, 1.0f));
        trfm3D->local = composeMatrix(pos, rotZ * rot, scale);
    }
    return 0;
}

static inline int transform3d_getQuat(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        pushVec4(L, glm::vec4(rot.x, rot.y, rot.z, rot.w));
        return 1;
    }
    return 0;
}

static inline int transform3d_setQuat(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    glm::vec4* quatVec = getVec4(L, 2);
    if (!quatVec) return luaL_error(L, "setQuat expects Vec4");
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        glm::quat quat(quatVec->w, quatVec->x, quatVec->y, quatVec->z);
        trfm3D->local = composeMatrix(pos, quat, scale);
    }
    return 0;
}

static inline int transform3d_getScl(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        pushVec3(L, scale);
        return 1;
    }
    return 0;
}

static inline int transform3d_setScl(lua_State* L) {
    tinyHandle* handle = getTransform3DHandle(L, 1);
    if (!handle) return 0;
    
    glm::vec3* newScale = getVec3(L, 2);
    if (!newScale) return luaL_error(L, "setScl expects Vec3");
    
    auto trfm3D = getSceneFromLua(L)->nGetComp<rtTransform3D>(*handle);
    if (trfm3D) {
        glm::vec3 pos, scale;
        glm::quat rot;
        decomposeMatrix(trfm3D->local, pos, rot, scale);
        trfm3D->local = composeMatrix(pos, rot, *newScale);
    }
    return 0;
}

static inline int node_transform3D(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    rtScene* scene = getSceneFromLua(L);
    if (!scene) { lua_pushnil(L); return 1; }
    
    auto trfm3D = scene->nGetComp<rtTransform3D>(*handle);
    if (!trfm3D) { lua_pushnil(L); return 1; }
    
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = *handle;
    luaL_getmetatable(L, "Transform3D");
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// COMPONENT: SKELETON3D & BONE
// ========================================

// Bone userdata structure - stores node handle + bone index
struct LuaBone {
    tinyHandle nodeHandle;
    uint32_t boneIndex;
};

static inline LuaBone* getBoneFromUserdata(lua_State* L, int index) {
    return static_cast<LuaBone*>(luaL_checkudata(L, index, "Bone"));
}

static inline void pushBone(lua_State* L, tinyHandle nodeHandle, uint32_t boneIndex) {
    LuaBone* ud = static_cast<LuaBone*>(lua_newuserdata(L, sizeof(LuaBone)));
    ud->nodeHandle = nodeHandle;
    ud->boneIndex = boneIndex;
    luaL_getmetatable(L, "Bone");
    lua_setmetatable(L, -2);
}

// ===== Bone Local Pose Methods =====

static inline int bone_getLocalPos(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    pushVec3(L, pos);
    return 1;
}

static inline int bone_setLocalPos(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    glm::vec3* newPos = getVec3(L, 2);
    if (!newPos) return luaL_error(L, "setLocalPos expects Vec3");
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    skel3D->localPose(bone->boneIndex) = composeMatrix(*newPos, rot, scale);
    return 0;
}

static inline int bone_rotX(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    glm::quat rotX = glm::angleAxis(glm::radians(degrees), glm::vec3(1.0f, 0.0f, 0.0f));
    skel3D->localPose(bone->boneIndex) = composeMatrix(pos, rotX * rot, scale);
    return 0;
}

static inline int bone_rotY(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    glm::quat rotY = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
    skel3D->localPose(bone->boneIndex) = composeMatrix(pos, rotY * rot, scale);
    return 0;
}

static inline int bone_rotZ(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    float degrees = luaL_checknumber(L, 2);
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    glm::quat rotZ = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 0.0f, 1.0f));
    skel3D->localPose(bone->boneIndex) = composeMatrix(pos, rotZ * rot, scale);
    return 0;
}

static inline int bone_getLocalQuat(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    pushVec4(L, glm::vec4(rot.x, rot.y, rot.z, rot.w));
    return 1;
}

static inline int bone_setLocalQuat(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    glm::vec4* quatVec = getVec4(L, 2);
    if (!quatVec) return luaL_error(L, "setLocalQuat expects Vec4");
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    glm::quat quat(quatVec->w, quatVec->x, quatVec->y, quatVec->z);
    skel3D->localPose(bone->boneIndex) = composeMatrix(pos, quat, scale);
    return 0;
}

static inline int bone_getLocalScl(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    pushVec3(L, scale);
    return 1;
}

static inline int bone_setLocalScl(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    glm::vec3* newScale = getVec3(L, 2);
    if (!newScale) return luaL_error(L, "setLocalScl expects Vec3");
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    skel3D->localPose(bone->boneIndex) = composeMatrix(pos, rot, *newScale);
    return 0;
}

// Get full local pose matrix
static inline int bone_localPose(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skel3D->localPose(bone->boneIndex), pos, rot, scale);
    
    lua_newtable(L);
    
    // Position
    pushVec3(L, pos);
    lua_setfield(L, -2, "pos");
    
    // Rotation (as quaternion Vec4)
    pushVec4(L, glm::vec4(rot.x, rot.y, rot.z, rot.w));
    lua_setfield(L, -2, "rot");
    
    // Scale
    pushVec3(L, scale);
    lua_setfield(L, -2, "scl");
    
    return 1;
}

// ===== Bone Bind Pose Methods (Read-only) =====

static inline int bone_getBindPos(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skeleton->bones[bone->boneIndex].bindPose, pos, rot, scale);
    
    pushVec3(L, pos);
    return 1;
}

static inline int bone_getBindQuat(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skeleton->bones[bone->boneIndex].bindPose, pos, rot, scale);
    
    pushVec4(L, glm::vec4(rot.x, rot.y, rot.z, rot.w));
    return 1;
}

static inline int bone_getBindScl(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skeleton->bones[bone->boneIndex].bindPose, pos, rot, scale);
    
    pushVec3(L, scale);
    return 1;
}

// Get full bind pose
static inline int bone_bindPose(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) return 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) return 0;
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) return 0;
    
    glm::vec3 pos, scale;
    glm::quat rot;
    decomposeMatrix(skeleton->bones[bone->boneIndex].bindPose, pos, rot, scale);
    
    lua_newtable(L);
    
    // Position
    pushVec3(L, pos);
    lua_setfield(L, -2, "pos");
    
    // Rotation (as quaternion Vec4)
    pushVec4(L, glm::vec4(rot.x, rot.y, rot.z, rot.w));
    lua_setfield(L, -2, "rot");
    
    // Scale
    pushVec3(L, scale);
    lua_setfield(L, -2, "scl");
    
    return 1;
}

// ===== Bone Hierarchy Methods =====

static inline int bone_parent(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_pushnil(L); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) { lua_pushnil(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) {
        lua_pushnil(L);
        return 1;
    }
    
    int parentIndex = skeleton->bones[bone->boneIndex].parent;
    if (parentIndex < 0) {
        lua_pushnil(L); // Root bone has no parent
        return 1;
    }
    
    pushBone(L, bone->nodeHandle, static_cast<uint32_t>(parentIndex));
    return 1;
}

static inline int bone_parentIndex(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_pushnil(L); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) { lua_pushnil(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) {
        lua_pushnil(L);
        return 1;
    }
    
    int parentIndex = skeleton->bones[bone->boneIndex].parent;
    if (parentIndex < 0) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_pushinteger(L, parentIndex);
    return 1;
}

static inline int bone_children(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_newtable(L); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) { lua_newtable(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) {
        lua_newtable(L);
        return 1;
    }
    
    const std::vector<int>& children = skeleton->bones[bone->boneIndex].children;
    
    lua_newtable(L);
    for (size_t i = 0; i < children.size(); ++i) {
        pushBone(L, bone->nodeHandle, static_cast<uint32_t>(children[i]));
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    
    return 1;
}

static inline int bone_childrenIndices(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_newtable(L); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) { lua_newtable(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) {
        lua_newtable(L);
        return 1;
    }
    
    const std::vector<int>& children = skeleton->bones[bone->boneIndex].children;
    
    lua_newtable(L);
    for (size_t i = 0; i < children.size(); ++i) {
        lua_pushinteger(L, children[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    
    return 1;
}

// Get bone index
static inline int bone_index(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_pushnil(L); return 1; }
    
    lua_pushinteger(L, bone->boneIndex);
    return 1;
}

// Get bone name
static inline int bone_name(lua_State* L) {
    LuaBone* bone = getBoneFromUserdata(L, 1);
    if (!bone) { lua_pushnil(L); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(bone->nodeHandle);
    if (!skel3D) { lua_pushnil(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || bone->boneIndex >= skeleton->bones.size()) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_pushstring(L, skeleton->bones[bone->boneIndex].name.c_str());
    return 1;
}

// ===== Skeleton3D Component Methods =====

// Skeleton3D userdata structure - stores node handle
static inline tinyHandle* getSkeleton3DHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Skeleton3D"));
}

// skeleton:bone(index) - Get bone by index
static inline int skeleton3d_bone(lua_State* L) {
    tinyHandle* handle = getSkeleton3DHandle(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    uint32_t boneIndex = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(*handle);
    if (!skel3D) { lua_pushnil(L); return 1; }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton || boneIndex >= skeleton->bones.size()) {
        lua_pushnil(L);
        return 1;
    }
    
    pushBone(L, *handle, boneIndex);
    return 1;
}

// skeleton:boneCount() - Get total bone count
static inline int skeleton3d_boneCount(lua_State* L) {
    tinyHandle* handle = getSkeleton3DHandle(L, 1);
    if (!handle) { lua_pushinteger(L, 0); return 1; }
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(*handle);
    if (!skel3D) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    lua_pushinteger(L, static_cast<lua_Integer>(skeleton->bones.size()));
    return 1;
}

// skeleton:refresh(boneIndex, recursive) - Reset bone(s) to bind pose
static inline int skeleton3d_refresh(lua_State* L) {
    tinyHandle* handle = getSkeleton3DHandle(L, 1);
    if (!handle) return 0;
    
    uint32_t boneIndex = lua_isnumber(L, 2) ? static_cast<uint32_t>(lua_tointeger(L, 2)) : 0;
    bool recursive = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(*handle);
    if (skel3D) {
        skel3D->refresh(boneIndex, recursive);
    }
    return 0;
}

// skeleton:update(boneIndex) - Update final pose and skin data
static inline int skeleton3d_update(lua_State* L) {
    tinyHandle* handle = getSkeleton3DHandle(L, 1);
    if (!handle) return 0;
    
    uint32_t boneIndex = lua_isnumber(L, 2) ? static_cast<uint32_t>(lua_tointeger(L, 2)) : 0;
    
    auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(*handle);
    if (skel3D) {
        skel3D->update(boneIndex);
    }
    return 0;
}

// NODE:skeleton3D() - Get skeleton component
static inline int node_skeleton3D(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    rtScene* scene = getSceneFromLua(L);
    if (!scene) { lua_pushnil(L); return 1; }
    
    auto skel3D = scene->nGetComp<rtSkeleton3D>(*handle);
    if (!skel3D) { lua_pushnil(L); return 1; }
    
    tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
    *ud = *handle;
    luaL_getmetatable(L, "Skeleton3D");
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// COMPONENT: ANIMATION3D
// ========================================
/*
static inline tinyHandle* getAnim3DHandle(lua_State* L, int index) {
    return static_cast<tinyHandle*>(luaL_checkudata(L, index, "Anim3D"));
}

static inline int anim3d_get(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isstring(L, 2)) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->getHandle(lua_tostring(L, 2));
        if (animHandle) {
            pushHandle(L, animHandle);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static inline int anim3d_current(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) {
        tinyHandle animHandle = comps.anim3D->curHandle();
        if (animHandle) {
            pushHandle(L, animHandle);
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
        auto comps = getSceneFromLua(L)->Wrap(*handle);
        if (!comps.anim3D) return 0;
        animHandle = comps.anim3D->getHandle(lua_tostring(L, 2));
    } else {
        tinyHandle* handle = getHandleFromUserdata(L, 2);
        if (!handle) return 0;
        animHandle = *handle;
    }
    
    bool restart = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : true;
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->play(animHandle, restart);
    return 0;
}

static inline int anim3d_setSpeed(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isnumber(L, 2)) return 0;
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->setSpeed(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

static inline int anim3d_isPlaying(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushboolean(L, false); return 1; }
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    lua_pushboolean(L, comps.anim3D && comps.anim3D->isPlaying());
    return 1;
}

static inline int anim3d_getTime(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnumber(L, 0.0f); return 1; }
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    lua_pushnumber(L, comps.anim3D ? comps.anim3D->getTime() : 0.0f);
    return 1;
}

static inline int anim3d_setTime(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle || !lua_isnumber(L, 2)) return 0;
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->setTime(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

static inline int anim3d_getDuration(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushnumber(L, 0.0f); return 1; }
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (!comps.anim3D) { lua_pushnumber(L, 0.0f); return 1; }
    
    tinyHandle animHandle;
    
    // Check if second argument is provided
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        LuaHandle* luaHandle = getLuaHandleFromUserdata(L, 2);
        if (luaHandle && luaHandle->type == "animation") {
            animHandle = luaHandle->toTinyHandle();
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
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->setLoop(lua_toboolean(L, 2));
    return 0;
}

static inline int anim3d_isLoop(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) { lua_pushboolean(L, true); return 1; }
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    lua_pushboolean(L, comps.anim3D ? comps.anim3D->getLoop() : true);
    return 1;
}

static inline int anim3d_pause(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) return 0;
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->pause();
    return 0;
}

static inline int anim3d_resume(lua_State* L) {
    tinyHandle* handle = getAnim3DHandle(L, 1);
    if (!handle) return 0;
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    if (comps.anim3D) comps.anim3D->resume();
    return 0;
}

static inline int node_anime3D(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
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
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    const char* varName = lua_tostring(L, 2);
    
    if (comps.script && comps.script->vHas(varName)) {
        const tinyVar& var = comps.script->vMap().at(varName);
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) lua_pushnumber(L, val);
            else if constexpr (std::is_same_v<T, int>) lua_pushinteger(L, val);
            else if constexpr (std::is_same_v<T, bool>) lua_pushboolean(L, val);
            else if constexpr (std::is_same_v<T, glm::vec2>) pushVec2(L, val);
            else if constexpr (std::is_same_v<T, glm::vec3>) pushVec3(L, val);
            else if constexpr (std::is_same_v<T, glm::vec4>) pushVec4(L, val);
            else if constexpr (std::is_same_v<T, std::string>) lua_pushstring(L, val.c_str());
            else if constexpr (std::is_same_v<T, tinyHandle>) {
                // Convert tinyHandle back to LuaHandle
                pushHandle(L, LuaHandle::fromTinyHandle(val));
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
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
    const char* varName = lua_tostring(L, 2);
    
    if (comps.script && comps.script->vHas(varName)) {
        tinyVar& var = comps.script->vMap().at(varName);
        std::visit([L](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) val = static_cast<float>(lua_tonumber(L, 3));
            else if constexpr (std::is_same_v<T, int>) val = static_cast<int>(lua_tointeger(L, 3));
            else if constexpr (std::is_same_v<T, bool>) val = static_cast<bool>(lua_toboolean(L, 3));
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                if (lua_isuserdata(L, 3)) {
                    glm::vec2* vec = getVec2(L, 3);
                    if (vec) val = *vec;
                }
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                if (lua_isuserdata(L, 3)) {
                    glm::vec3* vec = getVec3(L, 3);
                    if (vec) val = *vec;
                }
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                if (lua_isuserdata(L, 3)) {
                    glm::vec4* vec = getVec4(L, 3);
                    if (vec) val = *vec;
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) val = std::string(lua_tostring(L, 3));
            else if constexpr (std::is_same_v<T, tinyHandle>) {
                // The handle userdata comes from the calling Lua state (L)
                // We need to safely extract it without throwing errors
                if (lua_isuserdata(L, 3)) {
                    // Try to get it as a LuaHandle by checking metatable
                    if (lua_getmetatable(L, 3)) {
                        luaL_getmetatable(L, "Handle");
                        if (lua_rawequal(L, -1, -2)) {
                            // Valid Handle userdata, safe to cast
                            LuaHandle* luaHandle = static_cast<LuaHandle*>(lua_touserdata(L, 3));
                            if (luaHandle) {
                                val = luaHandle->toTinyHandle();
                            }
                        }
                        lua_pop(L, 2);  // Pop both metatables
                    }
                }
            }
        }, var);
    }
    return 0;
}

static inline int node_script(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) { lua_pushnil(L); return 1; }
    
    auto comps = getSceneFromLua(L)->Wrap(*handle);
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
    if (parentHandle) {
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
    if (parentHandle) {
        pushHandle(L, LuaHandle("node", parentHandle.idx(), parentHandle.ver()));
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
        pushHandle(L, LuaHandle("node", children[i].idx(), children[i].ver()));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// node:delete([recursive]) - Delete this node
static inline int node_delete(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    // Argument 2: recursive flag (optional, default true)
    bool recursive = true;
    if (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) {
        recursive = lua_toboolean(L, 2);
    }
    
    // Get scene and call removeNode
    rtScene* scene = getSceneFromLua(L);
    if (!scene) {
        lua_pushboolean(L, false);
        return 1;
    }
    
    bool success = scene->removeNode(*handle, recursive);
    lua_pushboolean(L, success);
    return 1;
}

// node:handle() - Get the handle of this node as a LuaHandle
static inline int node_handle(lua_State* L) {
    tinyHandle* handle = getNodeHandleFromUserdata(L, 1);
    if (!handle) {
        lua_pushnil(L);
        return 1;
    }
    
    // Return the node's handle as a LuaHandle
    pushHandle(L, LuaHandle("node", handle->idx(), handle->ver()));
    return 1;
}

*/

// ========================================
// FS (FILESYSTEM REGISTRY) OBJECT
// ========================================

// Helper to get tinyRegistry pointer from __scene
static inline tinyRegistry* getFSRegistryFromLua(lua_State* L) {
    rtScene* scene = getSceneFromLua(L);
    if (!scene) return nullptr;
    return scene->res().fsr;
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
    
    tinyHandle* handle = getHandleFromUserdata(L, 2);
    if (!handle || !handle->valid()) {
        lua_pushnil(L);
        return 1;
    }
    
    tinyRegistry* registry = getFSRegistryFromLua(L);
    if (!registry) {
        lua_pushnil(L);
        return 1;
    }
    
    // Get resource from registry - type checking handled by tinyHandle
    {
        tinyHandle th = *handle;

        // Try to get as script first (most common for generic "resource" type)
        const tinyScript* script = registry->get<tinyScript>(th);
        
        if (!script || !script->valid()) {
            lua_pushnil(L);
            return 1;
        }
        
        // Push the script handle as a StaticScript userdata
        tinyHandle* ud = static_cast<tinyHandle*>(lua_newuserdata(L, sizeof(tinyHandle)));
        *ud = th;
        luaL_getmetatable(L, "StaticScript");
        lua_setmetatable(L, -2);
        return 1;
    }
    // Note: Type checking is handled by tinyHandle internally
    // If get<T>() fails, it returns nullptr
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
            // Transfer table generically (supports generic tables/arrays)
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
            // Check userdata type by metatable
            bool handled = false;
            if (lua_getmetatable(L, i)) {
                // Try Vec2
                luaL_getmetatable(L, "Vec2");
                if (lua_rawequal(L, -1, -2)) {
                    lua_pop(L, 2);
                    glm::vec2* vec = getVec2(L, i);
                    if (vec) pushVec2(scriptL, *vec);
                    else lua_pushnil(scriptL);
                    handled = true;
                } else {
                    lua_pop(L, 1);
                    
                    // Try Vec3
                    luaL_getmetatable(L, "Vec3");
                    if (lua_rawequal(L, -1, -2)) {
                        lua_pop(L, 2);
                        glm::vec3* vec = getVec3(L, i);
                        if (vec) pushVec3(scriptL, *vec);
                        else lua_pushnil(scriptL);
                        handled = true;
                    } else {
                        lua_pop(L, 1);
                        
                        // Try Vec4
                        luaL_getmetatable(L, "Vec4");
                        if (lua_rawequal(L, -1, -2)) {
                            lua_pop(L, 2);
                            glm::vec4* vec = getVec4(L, i);
                            if (vec) pushVec4(scriptL, *vec);
                            else lua_pushnil(scriptL);
                            handled = true;
                        } else {
                            lua_pop(L, 1);
                            
                            // Try Handle
                            luaL_getmetatable(L, "Handle");
                            if (lua_rawequal(L, -1, -2)) {
                                lua_pop(L, 2);
                                tinyHandle* sourceHandle = static_cast<tinyHandle*>(lua_touserdata(L, i));
                                if (sourceHandle) pushHandle(scriptL, *sourceHandle);
                                else lua_pushnil(scriptL);
                                handled = true;
                            } else {
                                lua_pop(L, 2); // Pop both metatables
                            }
                        }
                    }
                }
            }
            
            if (!handled) {
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
            // Check userdata type by metatable
            bool handled = false;
            if (lua_getmetatable(scriptL, absIndex)) {
                // Try Vec2
                luaL_getmetatable(scriptL, "Vec2");
                if (lua_rawequal(scriptL, -1, -2)) {
                    lua_pop(scriptL, 2);
                    glm::vec2* vec = static_cast<glm::vec2*>(lua_touserdata(scriptL, absIndex));
                    if (vec) pushVec2(L, *vec);
                    else lua_pushnil(L);
                    handled = true;
                } else {
                    lua_pop(scriptL, 1);
                    
                    // Try Vec3
                    luaL_getmetatable(scriptL, "Vec3");
                    if (lua_rawequal(scriptL, -1, -2)) {
                        lua_pop(scriptL, 2);
                        glm::vec3* vec = static_cast<glm::vec3*>(lua_touserdata(scriptL, absIndex));
                        if (vec) pushVec3(L, *vec);
                        else lua_pushnil(L);
                        handled = true;
                    } else {
                        lua_pop(scriptL, 1);
                        
                        // Try Vec4
                        luaL_getmetatable(scriptL, "Vec4");
                        if (lua_rawequal(scriptL, -1, -2)) {
                            lua_pop(scriptL, 2);
                            glm::vec4* vec = static_cast<glm::vec4*>(lua_touserdata(scriptL, absIndex));
                            if (vec) pushVec4(L, *vec);
                            else lua_pushnil(L);
                            handled = true;
                        } else {
                            lua_pop(scriptL, 1);
                            
                            // Try Quat
                            luaL_getmetatable(scriptL, "Quat");
                            if (lua_rawequal(scriptL, -1, -2)) {
                                lua_pop(scriptL, 2);
                                glm::vec4* vec = static_cast<glm::vec4*>(lua_touserdata(scriptL, absIndex));
                                if (vec) pushVec4(L, *vec);
                                else lua_pushnil(L);
                                handled = true;
                            } else {
                                lua_pop(scriptL, 1);
                                
                                // Try Handle
                                luaL_getmetatable(scriptL, "Handle");
                                if (lua_rawequal(scriptL, -1, -2)) {
                                    lua_pop(scriptL, 2);
                                    tinyHandle* sourceHandle = static_cast<tinyHandle*>(lua_touserdata(scriptL, absIndex));
                                    if (sourceHandle) pushHandle(L, *sourceHandle);
                                    else lua_pushnil(L);
                                    handled = true;
                                } else {
                                    lua_pop(scriptL, 2); // Pop both metatables
                                }
                            }
                        }
                    }
                }
            }
            
            if (!handled) {
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
    glm::vec4* q1 = getVec4(L, 1);
    glm::vec4* q2 = getVec4(L, 2);
    if (!q1 || !q2 || !lua_isnumber(L, 3)) {
        return luaL_error(L, "quat_slerp requires (Vec4, Vec4, number)");
    }
    
    float t = static_cast<float>(lua_tonumber(L, 3));
    
    // Convert Vec4 to quat (x,y,z,w -> w,x,y,z)
    glm::quat quat1(q1->w, q1->x, q1->y, q1->z);
    glm::quat quat2(q2->w, q2->x, q2->y, q2->z);
    
    // Perform slerp
    glm::quat result = glm::slerp(quat1, quat2, t);
    pushVec4(L, glm::vec4(result.x, result.y, result.z, result.w));
    return 1;
}

// quat_fromAxisAngle(axis, angle) - Create quaternion from axis-angle representation
static inline int lua_quat_fromAxisAngle(lua_State* L) {
    glm::vec3* axis = getVec3(L, 1);
    if (!axis || !lua_isnumber(L, 2)) {
        return luaL_error(L, "quat_fromAxisAngle requires (Vec3, number)");
    }
    
    float angle = static_cast<float>(lua_tonumber(L, 2));
    
    // Create quaternion from axis-angle
    glm::quat result = glm::angleAxis(angle, glm::normalize(*axis));
    pushVec4(L, glm::vec4(result.x, result.y, result.z, result.w));
    return 1;
}

// quat_fromEuler(euler) - Create quaternion from Euler angles (vec3)
static inline int lua_quat_fromEuler(lua_State* L) {
    glm::vec3* euler = getVec3(L, 1);
    if (!euler) {
        return luaL_error(L, "quat_fromEuler requires (Vec3)");
    }
    
    // Create quaternion from euler
    glm::quat result = glm::quat(*euler);
    pushVec4(L, glm::vec4(result.x, result.y, result.z, result.w));
    return 1;
}

// quat_toEuler(quat) - Convert quaternion to Euler angles (returns vec3)
static inline int lua_quat_toEuler(lua_State* L) {
    glm::vec4* q = getVec4(L, 1);
    if (!q) {
        return luaL_error(L, "quat_toEuler requires (Vec4)");
    }
    
    // Convert Vec4 to quat
    glm::quat quat(q->w, q->x, q->y, q->z);
    
    // Convert to euler
    glm::vec3 euler = glm::eulerAngles(quat);
    pushVec3(L, euler);
    return 1;
}

// quat_lookAt(forward, up) - Create a quaternion that looks in the forward direction
static inline int lua_quat_lookAt(lua_State* L) {
    glm::vec3* forward = getVec3(L, 1);
    glm::vec3* up = getVec3(L, 2);
    if (!forward || !up) {
        return luaL_error(L, "quat_lookAt requires (Vec3, Vec3)");
    }
    
    // Create look-at quaternion
    // Negate forward because glm::quatLookAt assumes +Z is forward, but most engines use -Z
    glm::quat result = glm::quatLookAt(-glm::normalize(*forward), glm::normalize(*up));
    pushVec4(L, glm::vec4(result.x, result.y, result.z, result.w));
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

    rtScript->debug.log(message, 1.0f, 1.0f, 1.0f);
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

    // Vec2 metatable
    luaL_newmetatable(L, "Vec2");
    lua_pushcfunction(L, vec2_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, vec2_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, vec2_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);
    
    // Vec3 metatable
    luaL_newmetatable(L, "Vec3");
    lua_pushcfunction(L, vec3_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, vec3_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, vec3_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);
    
    // Vec4 metatable
    luaL_newmetatable(L, "Vec4");
    lua_pushcfunction(L, vec4_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, vec4_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, vec4_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);
    
    // Register vector constructors as global functions
    LUA_REG_GLOBAL(lua_Vec2, "Vec2");
    LUA_REG_GLOBAL(lua_Vec3, "Vec3");
    LUA_REG_GLOBAL(lua_Vec4, "Vec4");
    
    // Transform3D metatable
    LUA_BEGIN_METATABLE("Transform3D");
    LUA_REG_METHOD(transform3d_getPos, "getPos");
    LUA_REG_METHOD(transform3d_setPos, "setPos");
    LUA_REG_METHOD(transform3d_getQuat, "getQuat");
    LUA_REG_METHOD(transform3d_setQuat, "setQuat");
    LUA_REG_METHOD(transform3d_rotX, "rotX");
    LUA_REG_METHOD(transform3d_rotY, "rotY");
    LUA_REG_METHOD(transform3d_rotZ, "rotZ");
    LUA_REG_METHOD(transform3d_getScl, "getScl");
    LUA_REG_METHOD(transform3d_setScl, "setScl");
    LUA_END_METATABLE("Transform3D");
    
    // Skeleton3D metatable
    LUA_BEGIN_METATABLE("Skeleton3D");
    LUA_REG_METHOD(skeleton3d_bone, "bone");
    LUA_REG_METHOD(skeleton3d_boneCount, "boneCount");
    LUA_REG_METHOD(skeleton3d_refresh, "refresh");
    LUA_REG_METHOD(skeleton3d_update, "update");
    LUA_END_METATABLE("Skeleton3D");

    // Bone metatable
    luaL_newmetatable(L, "Bone");
    lua_newtable(L); // __index table for methods
    // Local pose (modifiable)
    LUA_REG_METHOD(bone_getLocalPos, "getLocalPos");
    LUA_REG_METHOD(bone_setLocalPos, "setLocalPos");
    LUA_REG_METHOD(bone_getLocalQuat, "getLocalQuat");
    LUA_REG_METHOD(bone_setLocalQuat, "setLocalQuat");
    LUA_REG_METHOD(bone_rotX, "rotX");
    LUA_REG_METHOD(bone_rotY, "rotY");
    LUA_REG_METHOD(bone_rotZ, "rotZ");
    LUA_REG_METHOD(bone_getLocalScl, "getLocalScl");
    LUA_REG_METHOD(bone_setLocalScl, "setLocalScl");
    LUA_REG_METHOD(bone_localPose, "localPose");
    // Bind pose (read-only)
    LUA_REG_METHOD(bone_getBindPos, "getBindPos");
    LUA_REG_METHOD(bone_getBindQuat, "getBindQuat");
    LUA_REG_METHOD(bone_getBindScl, "getBindScl");
    LUA_REG_METHOD(bone_bindPose, "bindPose");
    // Hierarchy
    LUA_REG_METHOD(bone_parent, "parent");
    LUA_REG_METHOD(bone_parentIndex, "parentIndex");
    LUA_REG_METHOD(bone_children, "children");
    LUA_REG_METHOD(bone_childrenIndices, "childrenIndices");
    // Info
    LUA_REG_METHOD(bone_index, "index");
    LUA_REG_METHOD(bone_name, "name");
    lua_setfield(L, -2, "__index");
    // __tostring for debugging
    lua_pushcfunction(L, [](lua_State* L) -> int {
        LuaBone* b = getBoneFromUserdata(L, 1);
        if (!b) {
            lua_pushstring(L, "Bone(invalid)");
            return 1;
        }
        
        auto skel3D = getSceneFromLua(L)->nGetComp<rtSkeleton3D>(b->nodeHandle);
        if (!skel3D) {
            lua_pushfstring(L, "Bone(%d, invalid)", b->boneIndex);
            return 1;
        }
        
        const tinySkeleton* skeleton = skel3D->rSkeleton();
        if (skeleton && b->boneIndex < skeleton->bones.size()) {
            lua_pushfstring(L, "Bone(%d, \"%s\")", b->boneIndex, skeleton->bones[b->boneIndex].name.c_str());
        } else {
            lua_pushfstring(L, "Bone(%d)", b->boneIndex);
        }
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);
    
    /*
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

    */

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
    LUA_REG_METHOD(node_transform3D, "Transform3D");
    LUA_REG_METHOD(node_skeleton3D, "Skeleton3D");
    /*
    LUA_REG_METHOD(node_anime3D, "anime3D");
    LUA_REG_METHOD(node_script, "script");
    LUA_REG_METHOD(node_parent, "parent");
    LUA_REG_METHOD(node_children, "children");
    LUA_REG_METHOD(node_parentHandle, "parentHandle");
    LUA_REG_METHOD(node_childrenHandles, "childrenHandles");
    LUA_REG_METHOD(node_delete, "delete");
    LUA_REG_METHOD(node_handle, "handle");
    */
    LUA_END_METATABLE("Node");
    
    // Scene metatable
    LUA_BEGIN_METATABLE("Scene");
    LUA_REG_METHOD(scene_node, "node");
    LUA_END_METATABLE("Scene");
    
    // Handle metatable (minimal, type-checking handled by tinyHandle internally)
    luaL_newmetatable(L, "Handle");
    lua_pushcfunction(L, lua_handleEqual);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, lua_handleToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, lua_handleGC);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // Global Functions
    LUA_REG_GLOBAL(lua_KState, "KSTATE");
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
