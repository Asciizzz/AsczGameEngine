#include "tinyScript/tinyScript.hpp"
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

    // Create new Lua state
    L_ = luaL_newstate();
    if (!L_) {
        std::cerr << "[tinyScript] Failed to create Lua state for: " << name << std::endl;
        return false;
    }
    
    // Load standard libraries
    luaL_openlibs(L_);
    
    // Compile the code
    if (luaL_loadstring(L_, code.c_str()) != LUA_OK) {
        std::cerr << "[tinyScript] Compilation error in " << name << ": " 
                  << lua_tostring(L_, -1) << std::endl;
        lua_pop(L_, 1);
        lua_close(L_);
        L_ = nullptr;
        return false;
    }
    
    // Execute the chunk to define functions
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
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

void tinyScript::initRtVars(std::unordered_map<std::string, tinyVar>& vars) const {
    if (!valid()) return;
    
    // Check if script has an "initVars" function
    lua_getglobal(L_, "initVars");
    
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        // No initVars function - script doesn't define initial vars
        return;
    }
    
    // Call initVars() which should return a table of variable definitions
    if (lua_pcall(L_, 0, 1, 0) != LUA_OK) {
        std::cerr   << "[tinyScript] Error calling initVars in " << name << ": " 
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

void tinyScript::test() {
    if (name.empty()) name = "TestSpinScript";

    // Create a simple spinning script that rotates a node around the Y axis
    code = R"(
-- Test Script: Spin Around Y Axis
-- This script demonstrates basic node rotation

-- Initialize variables with default values
function initVars()
    return {
        rotationSpeed = 2.0,  -- Radians per second (about 115 degrees/sec)
        currentAngle = 0.0    -- Current rotation angle
    }
end

function update()
    -- Update the rotation angle based on delta time
    vars.currentAngle = vars.currentAngle + (vars.rotationSpeed * dTime)
    
    -- Keep angle in [0, 2π] range to prevent overflow
    local TWO_PI = 6.28318530718
    if vars.currentAngle > TWO_PI then
        vars.currentAngle = vars.currentAngle - TWO_PI
    end
    
    -- Apply rotation using general-purpose transform API
    -- Set rotation around Y axis (pitch = 0, yaw = currentAngle, roll = 0)
    setRotation(__nodeHandle, {x = 0, y = vars.currentAngle, z = 0})
end
)";

    // Automatically compile the test script
    compile();
}
