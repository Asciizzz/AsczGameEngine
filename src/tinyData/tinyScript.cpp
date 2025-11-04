#include "tinyData/tinyScript.hpp"
#include <iostream>

#include "tinyData/tinyRT_Scene.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

// ========== Lua C Functions ==========

// Get node's local position - returns {x, y, z}
static int lua_getPosition(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "getPosition expects (nodeHandle)");
    }

    lua_getglobal(L, "__scene");
    tinyRT::Scene* scene = static_cast<tinyRT::Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "index");
    lua_getfield(L, 1, "version");
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
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
static int lua_setPosition(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "setPosition expects (nodeHandle, {x, y, z})");
    }
    
    lua_getglobal(L, "__scene");
    tinyRT::Scene* scene = static_cast<tinyRT::Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "index");
    lua_getfield(L, 1, "version");
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
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
static int lua_getRotation(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "getRotation expects (nodeHandle)");
    }
    
    lua_getglobal(L, "__scene");
    tinyRT::Scene* scene = static_cast<tinyRT::Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "index");
    lua_getfield(L, 1, "version");
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
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
static int lua_setRotation(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
        return luaL_error(L, "setRotation expects (nodeHandle, {x, y, z})");
    }
    
    lua_getglobal(L, "__scene");
    tinyRT::Scene* scene = static_cast<tinyRT::Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "index");
    lua_getfield(L, 1, "version");
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
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
static int lua_rotate(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isnumber(L, 3)) {
        return luaL_error(L, "rotate expects (nodeHandle, {x, y, z} axis, angle)");
    }
    
    lua_getglobal(L, "__scene");
    tinyRT::Scene* scene = static_cast<tinyRT::Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "index");
    lua_getfield(L, 1, "version");
    tinyHandle handle;
    handle.index = static_cast<uint32_t>(lua_tointeger(L, -2));
    handle.version = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 2);
    
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

// ========== tinyScript Implementation ==========

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
    lua_pushcfunction(L_, lua_getPosition);
    lua_setglobal(L_, "getPosition");
    
    lua_pushcfunction(L_, lua_setPosition);
    lua_setglobal(L_, "setPosition");
    
    lua_pushcfunction(L_, lua_getRotation);
    lua_setglobal(L_, "getRotation");
    
    lua_pushcfunction(L_, lua_setRotation);
    lua_setglobal(L_, "setRotation");
    
    lua_pushcfunction(L_, lua_rotate);
    lua_setglobal(L_, "rotate");

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
