#include "tinyScript/tinyScript.hpp"

#include "tinyScript_bind.hpp"
#include <iostream>
#include <algorithm>
#include <typeinfo>

// ==================== tinyDebug Implementation ====================

void tinyDebug::log(const std::string& message, float r, float g, float b) {
    Entry entry;
    entry.str = message;
    entry.color[0] = r;
    entry.color[1] = g;
    entry.color[2] = b;

    // FIFO: if we're at max capacity, remove oldest entry
    if (logs_.size() >= maxLogs_) {
        logs_.erase(logs_.begin());
    }
    
    logs_.push_back(entry);
}

void tinyDebug::clear() {
    logs_.clear();
}

// ==================== tinyScript Implementation ====================

tinyScript::~tinyScript() {
    closeLua();
}

void tinyScript::closeLua() {
    if (!L_) return;

    lua_close(L_);
    L_ = nullptr;
    compiled_ = false;
    defaultVars_.clear();
    varsOrder_.clear();
}

tinyScript::tinyScript(tinyScript&& other) noexcept
    : code(std::move(other.code))
    , version_(other.version_)
    , L_(other.L_)
    , compiled_(other.compiled_)
    , defaultVars_(std::move(other.defaultVars_))
    , varsOrder_(std::move(other.varsOrder_))
    , debug_(std::move(other.debug_)) {
    other.L_ = nullptr;
    other.compiled_ = false;
}

tinyScript& tinyScript::operator=(tinyScript&& other) noexcept {
    if (this != &other) {
        if (L_) lua_close(L_);
        code = std::move(other.code);
        version_ = other.version_;
        L_ = other.L_;
        compiled_ = other.compiled_;
        defaultVars_ = std::move(other.defaultVars_);
        varsOrder_ = std::move(other.varsOrder_);
        debug_ = std::move(other.debug_);

        other.L_ = nullptr;
        other.compiled_ = false;
    }
    return *this;
}

bool tinyScript::compile() {
    // Close existing state
    closeLua();

    compiled_ = false;
    // Don't clear debug logs - keep history across compilations

    // Create new Lua state
    L_ = luaL_newstate();
    if (!L_) {
        debug_.log("Failed to create Lua state", 1.0f, 0.0f, 0.0f);
        return false;
    }
    
    // Load standard libraries
    luaL_openlibs(L_);
    
    // Register metatables and API bindings (only once during compilation)
    registerNodeBindings(L_);
    
    // Compile the code
    if (luaL_loadstring(L_, code.c_str()) != LUA_OK) {
        std::string error = std::string("Compilation error: ") + lua_tostring(L_, -1);
        debug_.log(error, 1.0f, 0.0f, 0.0f);
        lua_pop(L_, 1);
        closeLua();
        return false;
    }
    
    // Execute the chunk to define functions
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        std::string error = std::string("Execution error: ") + lua_tostring(L_, -1);
        debug_.log(error, 1.0f, 0.0f, 0.0f);
        lua_pop(L_, 1);
        closeLua();
        return false;
    }

    version_++;
    compiled_ = true;
    debug_.log("Compilation successful", 0.0f, 1.0f, 0.0f);

    cacheDefaultVars();
    cacheDefaultLocals();

    return true;
}

bool tinyScript::call(const char* functionName, lua_State* runtimeL, tinyDebug* runtimeDebug) const {
    if (!valid()) return false;
    
    lua_State* targetL = runtimeL ? runtimeL : L_;

    lua_getglobal(targetL, functionName);
    
    if (!lua_isfunction(targetL, -1)) {
        lua_pop(targetL, 1);
        if (runtimeDebug) {
            std::string error = std::string("Function '") + functionName + "' not found";
            runtimeDebug->log(error, 1.0f, 0.5f, 0.0f); // Orange for warning
        }
        return false;
    }

    // Call the function (0 args, 0 returns for now)
    if (lua_pcall(targetL, 0, 0, 0) != LUA_OK) {
        if (runtimeDebug) {
            std::string error = std::string("Runtime error in '") + functionName + "': " + lua_tostring(targetL, -1);
            runtimeDebug->log(error, 1.0f, 0.0f, 0.0f); // Red for error
        }
        lua_pop(targetL, 1);
        return false;
    }
    
    return true;
}

void tinyScript::initTable(tinyVarsMap& outTable, const tinyVarsMap& defaultTable) const {
    if (defaultTable.empty()) {
        outTable.clear();
        return;
    }

    printf("Initializing table with %zu default entries\n", defaultTable.size());

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

    lua_getglobal(L_, tableName);

    // Check if table exists
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }
    
    // Iterate over the table and extract variable definitions
    lua_pushnil(L_);  // First key
    while (lua_next(L_, -2) != 0) {
        // key is at index -2, value is at index -1
        if (lua_isstring(L_, -2)) {
            std::string key = lua_tostring(L_, -2);
            
            // Determine type and default value
            if (lua_isinteger(L_, -1)) {
                outMap[key] = static_cast<int>(lua_tointeger(L_, -1));
            } else if (lua_isnumber(L_, -1)) {
                outMap[key] = static_cast<float>(lua_tonumber(L_, -1));
            } else if (lua_isboolean(L_, -1)) {
                outMap[key] = static_cast<bool>(lua_toboolean(L_, -1));
            } else if (lua_isstring(L_, -1)) {
                outMap[key] = std::string(lua_tostring(L_, -1));
            } else if (lua_isuserdata(L_, -1)) {
                // Check metatable to determine userdata type
                if (lua_getmetatable(L_, -1)) {
                    // Check for Vec2
                    luaL_getmetatable(L_, "Vec2");
                    if (lua_rawequal(L_, -1, -2)) {
                        lua_pop(L_, 2);
                        glm::vec2* vec = static_cast<glm::vec2*>(lua_touserdata(L_, -1));
                        if (vec) outMap[key] = *vec;
                    } else {
                        lua_pop(L_, 1);
                        
                        // Check for Vec3
                        luaL_getmetatable(L_, "Vec3");
                        if (lua_rawequal(L_, -1, -2)) {
                            lua_pop(L_, 2);
                            glm::vec3* vec = static_cast<glm::vec3*>(lua_touserdata(L_, -1));
                            if (vec) outMap[key] = *vec;
                        } else {
                            lua_pop(L_, 1);
                            
                            // Check for Vec4
                            luaL_getmetatable(L_, "Vec4");
                            if (lua_rawequal(L_, -1, -2)) {
                                lua_pop(L_, 2);
                                glm::vec4* vec = static_cast<glm::vec4*>(lua_touserdata(L_, -1));
                                if (vec) outMap[key] = *vec;
                            } else {
                                lua_pop(L_, 1);
                                
                                // Check for Handle
                                luaL_getmetatable(L_, "Handle");
                                if (lua_rawequal(L_, -1, -2)) {
                                    lua_pop(L_, 2);
                                    LuaHandle* luaHandle = static_cast<LuaHandle*>(lua_touserdata(L_, -1));
                                    if (luaHandle) {
                                        outMap[key] = luaHandle->toTypeHandle();
                                    }
                                } else {
                                    lua_pop(L_, 2);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        lua_pop(L_, 1);  // Pop value, keep key for next iteration
    }

    lua_pop(L_, 1);  // Pop the table
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

// ==================== Lua Sync Helpers ====================

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
    bool readLuaValue<typeHandle>(lua_State* L, int idx, typeHandle& out) {
        if (lua_isuserdata(L, idx) && lua_getmetatable(L, idx)) {
            luaL_getmetatable(L, "Handle");
            bool eq = lua_rawequal(L, -1, -2);
            lua_pop(L, 2);
            if (eq) {
                if (auto* h = static_cast<LuaHandle*>(lua_touserdata(L, idx)); h && h->valid()) {
                    out = h->toTypeHandle();
                    return true;
                }
            }
        }
        return false;
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

void tinyScript::update(void* rtScript, void* scene, tinyHandle nodeHandle, float deltaTime, tinyDebug* runtimeDebug) const {
    if (!valid()) return;
    
    // Store rtScript pointer in Lua globals for print() function
    lua_pushlightuserdata(L_, rtScript);
    lua_setglobal(L_, "__rtScript");
    
    // Get vars and locals from rtScript
    auto* rt = static_cast<tinyRT::Script*>(rtScript);
    auto& vars = rt->vMap();
    auto& locals = rt->lMap();

    // Helper lambda to push variables into a Lua table
    auto pushToTable = [&](const tinyVarsMap& tableMap) {
        for (const auto& [key, value] : tableMap) {
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
                else if constexpr (std::is_same_v<T, glm::vec2>) {
                    pushVec2(L_, val);
                    lua_setfield(L_, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, glm::vec3>) {
                    pushVec3(L_, val);
                    lua_setfield(L_, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, glm::vec4>) {
                    pushVec4(L_, val);
                    lua_setfield(L_, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    lua_pushstring(L_, val.c_str());
                    lua_setfield(L_, -2, key.c_str());
                }
                else if constexpr (std::is_same_v<T, typeHandle>) {
                    pushLuaHandle(L_, LuaHandle::fromTypeHandle(val));
                    lua_setfield(L_, -2, key.c_str());
                }
            }, value);
        }
    };

    // Push VARS and LOCALS tables to Lua
    lua_newtable(L_); pushToTable(vars);   lua_setglobal(L_, "VARS");
    lua_newtable(L_); pushToTable(locals); lua_setglobal(L_, "LOCALS");

    // Push other globals
    lua_pushnumber(L_, deltaTime); lua_setglobal(L_, "DELTATIME");
    lua_pushlightuserdata(L_, scene); lua_setglobal(L_, "__scene");

    pushScene(L_, static_cast<tinyRT::Scene*>(scene)); lua_setglobal(L_, "SCENE");
    pushNode(L_, nodeHandle); lua_setglobal(L_, "NODE");
    pushFS(L_); lua_setglobal(L_, "FS");

    // Call update function
    call("update", nullptr, runtimeDebug);

    // Pull values back from Lua using helper
    pullMapFromLua(L_, "VARS", vars);
    pullMapFromLua(L_, "LOCALS", locals);
}
