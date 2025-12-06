#include "tinyScript.hpp"
#include "tinyScript_bind.hpp"
#include "tinyRT/rtScript.hpp"

#include <iostream>
#include <algorithm>
#include <typeinfo>

#include <fstream>
#include <sstream>


// ==================== Helper Functions ====================

namespace {
    // Helper to read a value from Lua stack to a C++ type
    template<typename T>
    bool readLuaValue(lua_State* L, int idx, T& out);

    // Specializations for basic types
    template<>
    bool readLuaValue<float>(lua_State* L, int idx, float& out) {
        if (lua_isnumber(L, idx)) {
            out = static_cast<float>(lua_tonumber(L, idx));
            return true;
        }
        return false;
    }

    template<>
    bool readLuaValue<int>(lua_State* L, int idx, int& out) {
        if (lua_isinteger(L, idx)) {
            out = static_cast<int>(lua_tointeger(L, idx));
            return true;
        }
        return false;
    }

    template<>
    bool readLuaValue<bool>(lua_State* L, int idx, bool& out) {
        if (lua_isboolean(L, idx)) {
            out = lua_toboolean(L, idx);
            return true;
        }
        return false;
    }

    template<>
    bool readLuaValue<std::string>(lua_State* L, int idx, std::string& out) {
        if (lua_isstring(L, idx)) {
            out = lua_tostring(L, idx);
            return true;
        }
        return false;
    }

    // Helper for vector types
    template<typename VecT>
    bool readVec(lua_State* L, int idx, const char* mtName, VecT& out) {
        if (lua_isuserdata(L, idx) && lua_getmetatable(L, idx)) {
            luaL_getmetatable(L, mtName);
            bool eq = lua_rawequal(L, -1, -2);
            lua_pop(L, 2);
            if (eq) {
                if (auto* vec = static_cast<VecT*>(lua_touserdata(L, idx))) {
                    out = *vec;
                    return true;
                }
            }
        }
        return false;
    }

    template<>
    bool readLuaValue<glm::vec2>(lua_State* L, int idx, glm::vec2& out) {
        return readVec(L, idx, "Vec2", out);
    }

    template<>
    bool readLuaValue<glm::vec3>(lua_State* L, int idx, glm::vec3& out) {
        return readVec(L, idx, "Vec3", out);
    }

    template<>
    bool readLuaValue<glm::vec4>(lua_State* L, int idx, glm::vec4& out) {
        return readVec(L, idx, "Vec4", out);
    }

    template<>
    bool readLuaValue<tinyHandle>(lua_State* L, int idx, tinyHandle& out) {
        if (lua_isuserdata(L, idx) && lua_getmetatable(L, idx)) {
            luaL_getmetatable(L, "Handle");
            bool eq = lua_rawequal(L, -1, -2);
            lua_pop(L, 2);
            if (eq) {
                if (auto* h = static_cast<tinyHandle*>(lua_touserdata(L, idx))) {
                    out = *h;
                    return true;
                }
            }
        }
        return false;
    }

    // Helper to get array type from metadata
    const char* getArrayType(lua_State* L, int idx) {
        if (!lua_istable(L, idx)) return nullptr;
        
        lua_getfield(L, idx, "__array_type");
        if (lua_isstring(L, -1)) {
            const char* type = lua_tostring(L, -1);
            lua_pop(L, 1);
            return type;
        }
        lua_pop(L, 1);
        return nullptr;
    }

    // Array reading helpers
    template<typename T>
    bool readLuaArray(lua_State* L, int idx, std::vector<T>& out, const char* expectedType) {
        if (!lua_istable(L, idx)) return false;
        
        // Check type metadata
        const char* arrayType = getArrayType(L, idx);
        if (arrayType && strcmp(arrayType, expectedType) != 0) {
            return false;  // Type mismatch
        }
        
        out.clear();
        size_t len = lua_rawlen(L, idx);
        out.reserve(len);
        
        for (size_t i = 1; i <= len; i++) {
            lua_rawgeti(L, idx, i);
            T value;
            if (readLuaValue(L, -1, value)) {
                out.push_back(value);
            }
            lua_pop(L, 1);
        }
        return true;
    }

    template<>
    bool readLuaValue<std::vector<float>>(lua_State* L, int idx, std::vector<float>& out) {
        return readLuaArray(L, idx, out, "float");
    }

    template<>
    bool readLuaValue<std::vector<int>>(lua_State* L, int idx, std::vector<int>& out) {
        return readLuaArray(L, idx, out, "int");
    }

    template<>
    bool readLuaValue<std::vector<bool>>(lua_State* L, int idx, std::vector<bool>& out) {
        return readLuaArray(L, idx, out, "bool");
    }

    template<>
    bool readLuaValue<std::vector<glm::vec2>>(lua_State* L, int idx, std::vector<glm::vec2>& out) {
        return readLuaArray(L, idx, out, "vec2");
    }

    template<>
    bool readLuaValue<std::vector<glm::vec3>>(lua_State* L, int idx, std::vector<glm::vec3>& out) {
        return readLuaArray(L, idx, out, "vec3");
    }

    template<>
    bool readLuaValue<std::vector<glm::vec4>>(lua_State* L, int idx, std::vector<glm::vec4>& out) {
        return readLuaArray(L, idx, out, "vec4");
    }

    template<>
    bool readLuaValue<std::vector<std::string>>(lua_State* L, int idx, std::vector<std::string>& out) {
        return readLuaArray(L, idx, out, "string");
    }

    template<>
    bool readLuaValue<std::vector<tinyHandle>>(lua_State* L, int idx, std::vector<tinyHandle>& out) {
        return readLuaArray(L, idx, out, "handle");
    }

    // Helper to pull a map from Lua global table
    void pullMapFromLua(lua_State* L, const char* globalName, tinyVarsMap& vars) {
        lua_getglobal(L, globalName);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return;
        }

        for (auto& [key, value] : vars) {
            lua_getfield(L, -1, key.c_str());
            std::visit([&](auto& val) {
                readLuaValue(L, -1, val);
            }, value);
            lua_pop(L, 1);  // Pop field value
        }
        lua_pop(L, 1);  // Pop table
    }
}


// ==================== tinyScript Implementation ====================

tinyScript::~tinyScript() = default;

tinyScript::tinyScript(tinyScript&& other) noexcept
    : code(std::move(other.code))
    , version_(other.version_)
    , luaInstance_(std::move(other.luaInstance_))
    , onInit_(std::move(other.onInit_))
    , onCompile_(std::move(other.onCompile_))
    , compiled_(other.compiled_)
    , defaultVars_(std::move(other.defaultVars_))
    , defaultLocals_(std::move(other.defaultLocals_))
    , globals_(std::move(other.globals_))
    , varsOrder_(std::move(other.varsOrder_))
    , debug_(std::move(other.debug_)) {
    other.compiled_ = false;
}

tinyScript& tinyScript::operator=(tinyScript&& other) noexcept {
    if (this != &other) {
        code = std::move(other.code);
        version_ = other.version_;
        luaInstance_ = std::move(other.luaInstance_);
        onInit_ = std::move(other.onInit_);
        onCompile_ = std::move(other.onCompile_);
        compiled_ = other.compiled_;
        
        defaultVars_ = std::move(other.defaultVars_);
        defaultLocals_ = std::move(other.defaultLocals_);
        globals_ = std::move(other.globals_);

        varsOrder_ = std::move(other.varsOrder_);
        debug_ = std::move(other.debug_);

        other.compiled_ = false;
    }
    return *this;
}

bool tinyScript::compile() {
    compiled_ = false;

    // Set up scene-specific bindings
    luaInstance_.setBindings([](lua_State* L) {
        registerNodeBindings(L);
    });
    
    // Set up compilation callback to log errors
    onCompile_ = [this](bool success, const std::string& errorMsg) {
        if (!success) debug_.log(errorMsg, 1.0f, 0.0f, 0.0f);
        else debug_.log("Compilation successful", 0.0f, 1.0f, 0.0f);
    };
    
    // Initialize Lua instance
    if (!luaInstance_.init(onInit_)) {
        debug_.log("Failed to create Lua state", 1.0f, 0.0f, 0.0f);
        return false;
    }

    // Compile code
    if (!luaInstance_.compile(code, onCompile_)) {
        return false;
    }

    version_++;
    compiled_ = true;

    cacheDefaultVars();
    cacheDefaultLocals();
    cacheGlobals();

    return true;
}

void tinyScript::initTable(tinyVarsMap& outTable, const tinyVarsMap& defaultTable) const {
    if (defaultTable.empty()) {
        outTable.clear();
        return;
    }

    tinyVarsMap newVars;

    for (const auto& [key, defaultValue] : defaultTable) {
        auto it = outTable.find(key);

        if (it != outTable.end()) {
            bool typesMatch = std::visit([&](auto&& existingVal, auto&& defaultVal) -> bool {
                using ExistingT = std::decay_t<decltype(existingVal)>;
                using DefaultT = std::decay_t<decltype(defaultVal)>;
                return std::is_same_v<ExistingT, DefaultT>;
            }, it->second, defaultValue);

            // Check for matchking key-type, if exists, keep it, else, make default
            newVars[key] = typesMatch ? it->second : defaultValue;
        } else {
            // Variable doesn't exist yet - add it with default value
            newVars[key] = defaultValue;
        }
    }

    outTable = std::move(newVars);
}

void tinyScript::initVars(tinyVarsMap& outVars) const {
    initTable(outVars, defaultVars_);
}
void tinyScript::initLocals(tinyVarsMap& outLocals) const {
    initTable(outLocals, defaultLocals_);
}

void tinyScript::cacheDefaultTable(const char* tableName, tinyVarsMap& outMap) {
    outMap.clear();
    
    if (!valid()) return;

    lua_State* L = luaInstance_.state();
    lua_getglobal(L, tableName);

    // Check if table exists
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    
    // Iterate over the table and extract variable definitions
    lua_pushnil(L);  // First key
    while (lua_next(L, -2) != 0) {
        // key is at index -2, value is at index -1
        if (lua_isstring(L, -2)) {
            std::string key = lua_tostring(L, -2);
            
            // Try reading each type using the helper functions
            int intVal;
            float floatVal;
            bool boolVal;
            std::string strVal;
            glm::vec2 vec2Val;
            glm::vec3 vec3Val;
            glm::vec4 vec4Val;
            tinyHandle handleVal;
            std::vector<int> intArrVal;
            std::vector<float> floatArrVal;
            std::vector<bool> boolArrVal;
            std::vector<glm::vec2> vec2ArrVal;
            std::vector<glm::vec3> vec3ArrVal;
            std::vector<glm::vec4> vec4ArrVal;
            std::vector<std::string> strArrVal;
            std::vector<tinyHandle> handleArrVal;
            
            if (readLuaValue(L, -1, intVal)) {
                outMap[key] = intVal;
            } else if (readLuaValue(L, -1, floatVal)) {
                outMap[key] = floatVal;
            } else if (readLuaValue(L, -1, boolVal)) {
                outMap[key] = boolVal;
            } else if (readLuaValue(L, -1, vec2Val)) {
                outMap[key] = vec2Val;
            } else if (readLuaValue(L, -1, vec3Val)) {
                outMap[key] = vec3Val;
            } else if (readLuaValue(L, -1, vec4Val)) {
                outMap[key] = vec4Val;
            } else if (readLuaValue(L, -1, handleVal)) {
                outMap[key] = handleVal;
            } else if (readLuaValue(L, -1, strVal)) {
                outMap[key] = strVal;
            } else if (readLuaValue(L, -1, intArrVal)) {
                outMap[key] = intArrVal;
            } else if (readLuaValue(L, -1, floatArrVal)) {
                outMap[key] = floatArrVal;
            } else if (readLuaValue(L, -1, boolArrVal)) {
                outMap[key] = boolArrVal;
            } else if (readLuaValue(L, -1, vec2ArrVal)) {
                outMap[key] = vec2ArrVal;
            } else if (readLuaValue(L, -1, vec3ArrVal)) {
                outMap[key] = vec3ArrVal;
            } else if (readLuaValue(L, -1, vec4ArrVal)) {
                outMap[key] = vec4ArrVal;
            } else if (readLuaValue(L, -1, strArrVal)) {
                outMap[key] = strArrVal;
            } else if (readLuaValue(L, -1, handleArrVal)) {
                outMap[key] = handleArrVal;
            }
        }
        
        lua_pop(L, 1);  // Pop value, keep key for next iteration
    }

    lua_pop(L, 1);  // Pop the table
}

void tinyScript::cacheDefaultVars() {
    cacheDefaultTable("VARS", defaultVars_);

    // Build varsOrder_ - sort by type hash, then alphabetically within each type
    varsOrder_.clear();
    std::vector<std::pair<std::string, size_t>> orderedVars;
    
    for (const auto& [key, value] : defaultVars_) {
        // Get type hash for sorting
        size_t typeHash = std::visit([](auto&& val) -> size_t {
            using T = std::decay_t<decltype(val)>;
            return typeid(T).hash_code();
        }, value);
        
        orderedVars.push_back({key, typeHash});
    }
    
    // Sort by type hash, then by name
    std::sort(orderedVars.begin(), orderedVars.end(), 
        [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second;
            return a.first < b.first;  // Alphabetically within same type
        });
    
    // Extract just the names
    for (const auto& [key, _] : orderedVars) {
        varsOrder_.push_back(key);
    }
}

void tinyScript::cacheDefaultLocals() {
    cacheDefaultTable("LOCALS", defaultLocals_);
}

void tinyScript::cacheGlobals() {
    cacheDefaultTable("GLOBALS", globals_);
}

void tinyScript::update(void* rtScript, void* scene, tinyHandle nodeHandle, float deltaTime) {
    if (!valid()) return;

    auto* rt = static_cast<rtSCRIPT*>(rtScript);
    if (!rt) return;

    lua_State* L = luaInstance_.state();
    
    // Store rtScript pointer in Lua globals for print() function
    lua_pushlightuserdata(L, rtScript);
    lua_setglobal(L, "__rtScript");

    // Get vars and locals from rtScript
    auto& vars = rt->vars;
    auto& locals = rt->locals;

    // Helper lambda to push variables into a Lua table
    auto pushToTable = [&](const tinyVarsMap& tableMap) {
        for (const auto& [key, value] : tableMap) {
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                
                if constexpr (std::is_same_v<T, float>) {
                    lua_pushnumber(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, int>) {
                    lua_pushinteger(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    lua_pushboolean(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, glm::vec2>) {
                    pushVec2(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, glm::vec3>) {
                    pushVec3(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, glm::vec4>) {
                    pushVec4(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    lua_pushstring(L, val.c_str());
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, tinyHandle>) {
                    pushHandle(L, val);
                    lua_setfield(L, -2, key.c_str());
                }
                // Array types
                else if constexpr (std::is_same_v<T, std::vector<float>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "float");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        lua_pushnumber(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<int>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "int");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        lua_pushinteger(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "bool");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        lua_pushboolean(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<glm::vec2>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "vec2");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        pushVec2(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<glm::vec3>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "vec3");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        pushVec3(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<glm::vec4>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "vec4");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        pushVec4(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "string");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        lua_pushstring(L, val[i].c_str());
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::vector<tinyHandle>>) {
                    lua_newtable(L);
                    luaL_getmetatable(L, "Array");
                    lua_setmetatable(L, -2);
                    lua_pushstring(L, "handle");
                    lua_setfield(L, -2, "__array_type");
                    for (size_t i = 0; i < val.size(); i++) {
                        pushHandle(L, val[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                    lua_setfield(L, -2, key.c_str());
                }
            }, value);
        }
    };

    // Push VARS and LOCALS tables to Lua
    lua_newtable(L); pushToTable(vars);   lua_setglobal(L, "VARS");
    lua_newtable(L); pushToTable(locals); lua_setglobal(L, "LOCALS");
    
    // Push GLOBALS to Lua (mutable, shared across all instances)
    lua_newtable(L); pushToTable(globals_); lua_setglobal(L, "GLOBALS");

    // Push other globals
    lua_pushnumber(L, deltaTime); lua_setglobal(L, "DELTATIME");
    lua_pushlightuserdata(L, scene); lua_setglobal(L, "__scene");

    pushScene(L, static_cast<rtScene*>(scene)); lua_setglobal(L, "SCENE");
    pushNode(L, nodeHandle); lua_setglobal(L, "NODE");
    pushFS(L); lua_setglobal(L, "FS");

    // Call update function
    // call("update", nullptr, runtimeDebug);
    luaInstance_.call("update", [&](bool success, const std::string& errorMsg) {
        if (!success) { rt->debug.log(errorMsg, 1.0f, 0.0f, 0.0f); }
    });

    // Pull values back from Lua using helper
    pullMapFromLua(L, "VARS", vars);
    pullMapFromLua(L, "LOCALS", locals);
    pullMapFromLua(L, "GLOBALS", globals_);
}
