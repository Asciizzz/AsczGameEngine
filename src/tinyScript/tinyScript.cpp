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
    : name(std::move(other.name))
    , code(std::move(other.code))
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
        name = std::move(other.name);
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

    return true;
}

bool tinyScript::call(const char* functionName, lua_State* runtimeL) const {
    if (!valid()) return false;
    
    lua_State* targetL = runtimeL ? runtimeL : L_;

    lua_getglobal(targetL, functionName);
    
    if (!lua_isfunction(targetL, -1)) {
        lua_pop(targetL, 1);
        return false;
    }

    // Call the function (0 args, 0 returns for now)
    if (lua_pcall(targetL, 0, 0, 0) != LUA_OK) {
        lua_pop(targetL, 1);
        return false;
    }
    
    return true;
}

void tinyScript::cacheDefaultVars() {
    defaultVars_.clear();
    
    if (!valid()) return;

    lua_getglobal(L_, "vars");

    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }

    if (lua_pcall(L_, 0, 1, 0) != LUA_OK) {
        lua_pop(L_, 1);
        return;
    }
    
    // Check if return value is a table
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
                // Lua 5.3+ has explicit integer type
                defaultVars_[key] = static_cast<int>(lua_tointeger(L_, -1));
            } else if (lua_isnumber(L_, -1)) {
                // All other numbers are treated as floats
                defaultVars_[key] = static_cast<float>(lua_tonumber(L_, -1));
            } else if (lua_isboolean(L_, -1)) {
                defaultVars_[key] = static_cast<bool>(lua_toboolean(L_, -1));
            } else if (lua_isstring(L_, -1)) {
                defaultVars_[key] = std::string(lua_tostring(L_, -1));
            } else if (lua_istable(L_, -1)) {
                // Check if it's a vec3 (table with x, y, z)
                lua_getfield(L_, -1, "x");
                lua_getfield(L_, -2, "y");
                lua_getfield(L_, -3, "z");
                
                if (lua_isnumber(L_, -3) && lua_isnumber(L_, -2) && lua_isnumber(L_, -1)) {
                    glm::vec3 vec;
                    vec.z = static_cast<float>(lua_tonumber(L_, -1));
                    vec.y = static_cast<float>(lua_tonumber(L_, -2));
                    vec.x = static_cast<float>(lua_tonumber(L_, -3));
                    defaultVars_[key] = vec;
                }
                
                lua_pop(L_, 3);  // Pop x, y, z
            } else if (lua_isuserdata(L_, -1)) {
                // Check if it has the Handle metatable
                if (lua_getmetatable(L_, -1)) {
                    luaL_getmetatable(L_, "Handle");
                    if (lua_rawequal(L_, -1, -2)) {
                        // It's a valid Handle userdata
                        lua_pop(L_, 2); // Pop both metatables
                        
                        uint64_t* packedPtr = static_cast<uint64_t*>(lua_touserdata(L_, -1));
                        if (packedPtr) {
                            bool isNodeHandle = isNHandle(*packedPtr);
                            tinyHandle h = isNodeHandle ? unpackNHandle(*packedPtr) : unpackRHandle(*packedPtr);
                            
                            // Create typeHandle: type int for nodes, type void for files
                            typeHandle th = isNodeHandle ? typeHandle::make<int>(h) : typeHandle::make<void>(h);
                            defaultVars_[key] = th;
                        }
                    } else {
                        lua_pop(L_, 2); // Pop both metatables
                    }
                }
            }
        }
        
        lua_pop(L_, 1);  // Pop value, keep key for next iteration
    }

    lua_pop(L_, 1);  // Pop the table
    
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


void tinyScript::initVars(tinyVarsMap& outVars) const {
    if (defaultVars_.empty()) {
        outVars.clear();
        return;
    }

    tinyVarsMap newVars;

    for (const auto& [key, defaultValue] : defaultVars_) {
        auto it = outVars.find(key);
        
        if (it != outVars.end()) {
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

    outVars = std::move(newVars);
}


void tinyScript::update(void* rtScript, void* scene, tinyHandle nodeHandle, float dTime) const {
    if (!valid()) return;
    
    // Store rtScript pointer in Lua globals for print() function
    lua_pushlightuserdata(L_, rtScript);
    lua_setglobal(L_, "__rtScript");
    
    // Get vars from rtScript
    tinyRT::Script* rt = static_cast<tinyRT::Script*>(rtScript);
    tinyVarsMap& vars = rt->vMap();

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
            else if constexpr (std::is_same_v<T, typeHandle>) {
                // Push handle as full userdata with metatable
                // type int = node handle (nHandle), type void = file handle (rHandle)
                bool isNodeHandle = val.isType<int>();
                if (isNodeHandle) {
                    pushNHandle(L_, val.handle);
                } else {
                    pushRHandle(L_, val.handle);
                }
                lua_setfield(L_, -2, key.c_str());
            }
        }, value);
    }
    
    lua_setglobal(L_, "VARS");  // Set as global "VARS" table

    // ========== Push context information ==========
    // Push DTIME
    lua_pushnumber(L_, dTime);
    lua_setglobal(L_, "DTIME");
    
    // Push SCENE as a Scene userdata object
    pushScene(L_, static_cast<tinyRT::Scene*>(scene));
    lua_setglobal(L_, "SCENE");
    
    // Keep legacy __scene for compatibility
    lua_pushlightuserdata(L_, scene);
    lua_setglobal(L_, "__scene");
    
    // Push NODE as a Node userdata object (for calling methods like NODE:transform3D())
    pushNode(L_, nodeHandle);
    lua_setglobal(L_, "NODE");
    
    // Push NODEHANDLE as a raw nHandle lightuserdata (for passing to functions like SCENE:addScene())
    pushNHandle(L_, nodeHandle);
    lua_setglobal(L_, "NODEHANDLE");
    
    // Push FS (filesystem registry accessor) as a FS userdata object
    pushFS(L_);
    lua_setglobal(L_, "FS");

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
                else if constexpr (std::is_same_v<T, typeHandle>) {
                    // Pull back handle from full userdata
                    if (lua_isuserdata(L_, -1)) {
                        // Check if it has the Handle metatable
                        if (lua_getmetatable(L_, -1)) {
                            luaL_getmetatable(L_, "Handle");
                            if (lua_rawequal(L_, -1, -2)) {
                                // It's a valid Handle userdata
                                lua_pop(L_, 2); // Pop both metatables
                                
                                uint64_t* packedPtr = static_cast<uint64_t*>(lua_touserdata(L_, -1));
                                if (packedPtr) {
                                    bool isNodeHandle = isNHandle(*packedPtr);
                                    tinyHandle h = isNodeHandle ? unpackNHandle(*packedPtr) : unpackRHandle(*packedPtr);
                                    val = isNodeHandle ? typeHandle::make<int>(h) : typeHandle::make<void>(h);
                                }
                            } else {
                                lua_pop(L_, 2); // Pop both metatables
                            }
                        }
                    }
                }
            }, value);
            
            lua_pop(L_, 1);  // Pop the field value
        }
    }
    lua_pop(L_, 1);  // Pop vars table
}
