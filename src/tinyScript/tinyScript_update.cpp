#include "tinyScript/tinyScript.hpp"
#include "tinyScript_bind.hpp"
#include <iostream>

void tinyScript::update(std::unordered_map<std::string, tinyVar>& vars, void* scene, tinyHandle nodeHandle, float dTime) const {
    if (!valid()) return;

    // ========== Push runtime variables into Lua global table "vars" ==========
    lua_newtable(L_);  // Create vars table
    
    for (const auto& [key, value] : vars) {
        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            
            if constexpr (std::is_same_v<T, float>) {
                lua_pushnumber(L_, val);
                lua_setfield(L_, -2, key.c_str());
            }
            else if constexpr (std::is_same_v<T, int>) {
                lua_pushinteger(L_, val);
                lua_setfield(L_, -2, key.c_str());
            }
            else if constexpr (std::is_same_v<T, bool>) {
                lua_pushboolean(L_, val);
                lua_setfield(L_, -2, key.c_str());
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                lua_newtable(L_);
                lua_pushnumber(L_, val.x);
                lua_setfield(L_, -2, "x");
                lua_pushnumber(L_, val.y);
                lua_setfield(L_, -2, "y");
                lua_pushnumber(L_, val.z);
                lua_setfield(L_, -2, "z");
                lua_setfield(L_, -2, key.c_str());
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                lua_pushstring(L_, val.c_str());
                lua_setfield(L_, -2, key.c_str());
            }
        }, value);
    }
    
    lua_setglobal(L_, "vars");  // Set as global "vars" table

    // ========== Push context information ==========
    // Push dTime
    lua_pushnumber(L_, dTime);
    lua_setglobal(L_, "dTime");
    
    // Push scene pointer (as light userdata)
    lua_pushlightuserdata(L_, scene);
    lua_setglobal(L_, "__scene");
    
    // Push node handle
    lua_newtable(L_);
    lua_pushinteger(L_, nodeHandle.index);
    lua_setfield(L_, -2, "index");
    lua_pushinteger(L_, nodeHandle.version);
    lua_setfield(L_, -2, "version");
    lua_setglobal(L_, "__nodeHandle");

    // ========== Expose native functions to Lua ==========
    registerNodeBindings(L_);

    // ========== Call the update function ==========
    call("update");

    // ========== Pull variables back from Lua ==========
    lua_getglobal(L_, "vars");
    if (lua_istable(L_, -1)) {
        for (auto& [key, value] : vars) {
            lua_getfield(L_, -1, key.c_str());
            
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, float>) {
                    if (lua_isnumber(L_, -1)) {
                        val = static_cast<float>(lua_tonumber(L_, -1));
                    }
                }
                else if constexpr (std::is_same_v<T, int>) {
                    if (lua_isinteger(L_, -1)) {
                        val = static_cast<int>(lua_tointeger(L_, -1));
                    }
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    if (lua_isboolean(L_, -1)) {
                        val = lua_toboolean(L_, -1);
                    }
                }
                else if constexpr (std::is_same_v<T, glm::vec3>) {
                    if (lua_istable(L_, -1)) {
                        lua_getfield(L_, -1, "x");
                        if (lua_isnumber(L_, -1)) val.x = static_cast<float>(lua_tonumber(L_, -1));
                        lua_pop(L_, 1);
                        
                        lua_getfield(L_, -1, "y");
                        if (lua_isnumber(L_, -1)) val.y = static_cast<float>(lua_tonumber(L_, -1));
                        lua_pop(L_, 1);
                        
                        lua_getfield(L_, -1, "z");
                        if (lua_isnumber(L_, -1)) val.z = static_cast<float>(lua_tonumber(L_, -1));
                        lua_pop(L_, 1);
                    }
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    if (lua_isstring(L_, -1)) {
                        val = lua_tostring(L_, -1);
                    }
                }
            }, value);
            
            lua_pop(L_, 1);  // Pop the field value
        }
    }
    lua_pop(L_, 1);  // Pop vars table
}
