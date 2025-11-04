#include "tinyScript_bind.hpp"
#include "tinyData/tinyRT_Scene.hpp"
#include "tinyExt/tinyHandle.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

// ========== Helper Functions ==========

static tinyRT::Scene* getSceneFromLua(lua_State* L) {
    lua_getglobal(L, "__scene");
    void* scenePtr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<tinyRT::Scene*>(scenePtr);
}

static tinyHandle getNodeHandleFromTable(lua_State* L, int tableIndex) {
    lua_getfield(L, tableIndex, "index");
    lua_getfield(L, tableIndex, "version");
    
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
    return handle;
}

// ========== Transform API Functions ==========

// Get node's local position - returns {x, y, z}
int lua_getPosition(lua_State* L) {
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
int lua_setPosition(lua_State* L) {
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
int lua_getRotation(lua_State* L) {
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
int lua_setRotation(lua_State* L) {
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
int lua_rotate(lua_State* L) {
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

// ========== Registration Function ==========

void registerNodeBindings(lua_State* L) {
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
}
