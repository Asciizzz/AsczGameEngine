#include "tinyScript/tinyScript.hpp"

#include "tinyScript_bind.hpp"
#include <iostream>

tinyScript::~tinyScript() {
    closeLua();
}

void tinyScript::closeLua() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
}

tinyScript::tinyScript(tinyScript&& other) noexcept
    : name(std::move(other.name))
    , code(std::move(other.code))
    , version_(other.version_)
    , L_(other.L_)
    , compiled_(other.compiled_) {
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

        other.L_ = nullptr;
        other.compiled_ = false;
    }
    return *this;
}

bool tinyScript::compile() {
    // Close existing state
    closeLua();

    compiled_ = false;
    error_.clear(); // Clear previous errors

    // Create new Lua state
    L_ = luaL_newstate();
    if (!L_) {
        error_ = "Failed to create Lua state";
        std::cerr << "[tinyScript] Failed to create Lua state for: " << name << std::endl;
        return false;
    }
    
    // Load standard libraries
    luaL_openlibs(L_);
    
    // Compile the code
    if (luaL_loadstring(L_, code.c_str()) != LUA_OK) {
        error_ = std::string("Compilation error: ") + lua_tostring(L_, -1);
        std::cerr << "[tinyScript] Compilation error in " << name << ": " 
                  << lua_tostring(L_, -1) << std::endl;
        lua_pop(L_, 1);
        lua_close(L_);
        L_ = nullptr;
        return false;
    }
    
    // Execute the chunk to define functions
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        error_ = std::string("Execution error: ") + lua_tostring(L_, -1);
        std::cerr << "[tinyScript] Execution error in " << name << ": " 
                  << lua_tostring(L_, -1) << std::endl;
        lua_pop(L_, 1);
        lua_close(L_);
        L_ = nullptr;
        return false;
    }
    
    // Increment version and mark as compiled
    version_++;
    compiled_ = true;

    return true;
}

bool tinyScript::call(const char* functionName, lua_State* runtimeL) const {
    if (!valid()) return false;
    
    lua_State* targetL = runtimeL ? runtimeL : L_;
    
    // Get the function from the compiled state
    lua_getglobal(targetL, functionName);
    
    if (!lua_isfunction(targetL, -1)) {
        lua_pop(targetL, 1);
        return false;
    }
    
    // Call the function (0 args, 0 returns for now)
    if (lua_pcall(targetL, 0, 0, 0) != LUA_OK) {
        std::cerr << "[tinyScript] Error calling " << functionName << " in " << name << ": " 
                  << lua_tostring(targetL, -1) << std::endl;
        lua_pop(targetL, 1);
        return false;
    }
    
    return true;
}

void tinyScript::initRtVars(tinyVarsMap& vars) const {
    vars.clear(); // Clear existing variables first
    
    if (!valid()) return;

    // Check if script has an "vars" function
    lua_getglobal(L_, "vars");
    
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }

    if (lua_pcall(L_, 0, 1, 0) != LUA_OK) {
        std::cerr   << "[tinyScript] Error calling vars in " << name << ": " 
                    << lua_tostring(L_, -1) << std::endl;
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
                vars[key] = static_cast<int>(lua_tointeger(L_, -1));
            } else if (lua_isnumber(L_, -1)) {
                // All other numbers are treated as floats
                vars[key] = static_cast<float>(lua_tonumber(L_, -1));
            } else if (lua_isboolean(L_, -1)) {
                vars[key] = static_cast<bool>(lua_toboolean(L_, -1));
            } else if (lua_isstring(L_, -1)) {
                vars[key] = std::string(lua_tostring(L_, -1));
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
                    vars[key] = vec;
                }
                
                lua_pop(L_, 3);  // Pop x, y, z
            }
        }
        
        lua_pop(L_, 1);  // Pop value, keep key for next iteration
    }
    
    lua_pop(L_, 1);  // Pop the table
}





void tinyScript::update(tinyVarsMap& vars, void* scene, tinyHandle nodeHandle, float dTime) const {
    if (!valid()) return;

    // ========== Save the original 'vars' function before overwriting ==========
    lua_getglobal(L_, "vars");  // Get the vars function
    int varsRef = luaL_ref(L_, LUA_REGISTRYINDEX);  // Save it in the registry

    // ========== Push runtime variables into Lua global table "vars" (runtime data, not the function) ==========
    lua_newtable(L_);  // Create vars runtime data table
    
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
    
    // ========== Restore the original 'vars' function ==========
    lua_rawgeti(L_, LUA_REGISTRYINDEX, varsRef);  // Retrieve the saved function
    lua_setglobal(L_, "vars");  // Restore it as global
    luaL_unref(L_, LUA_REGISTRYINDEX, varsRef);  // Free the reference
}

void tinyScript::test() {
    if (name.empty()) name = "Demo Script";

    code = R"(-- Demo Script: 

-- Initialize variables with values
function vars()
    return {
        a = 0.0,
        b = 0.0,
        str = ""
    }
end

-- Your own function
function foo(a, b, d)
    -- Simple arithmetic operation 
    b = b + (a * d)

    -- DANGER! Without <local> result becomes a global variable, polluting the script 
    -- result = b + (a * d)

    -- Return (Gotta be honest if you have no idea how return works please just gtfo my engine)
    return b
end

function update()

    -- Function call
    vars.b = foo(vars.a, vars.b, dTime)

    -- Local variables ([!] Do not forget "local" keyword)
    local TWO_PI = 6.28318530718

    -- If else condition
    if vars.b > TWO_PI then
        vars.b = vars.b - TWO_PI
    else
        -- Do nothing
    end

    -- Call functions

    setRotation(__nodeHandle, {x = 0, y = vars.b, z = 0})

    -- String concatenation
    vars.str = ""

    -- Inline or indentation both work

    if kState("w") then vars.str = vars.str .. "W " end

    if kState("a") then vars.str = vars.str .. "A " end

    if kState("s") then vars.str = vars.str .. "S " end

    if kState("d") then vars.str = vars.str .. "D " end

    if kState("shift") then vars.str = vars.str .. "SHIFT " end

    if kState("space") then vars.str = vars.str .. "SPACE " end

    if vars.str == "" then
        vars.str = "None"
    end
end
)";

    // Automatically compile the test script
    compile();
}
