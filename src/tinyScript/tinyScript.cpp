#include "tinyScript/tinyScript.hpp"

#include "tinyScript_bind.hpp"
#include <iostream>

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
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
        compiled_ = false;
        defaultVars_.clear();
    }
}

tinyScript::tinyScript(tinyScript&& other) noexcept
    : name(std::move(other.name))
    , code(std::move(other.code))
    , version_(other.version_)
    , L_(other.L_)
    , compiled_(other.compiled_)
    , defaultVars_(std::move(other.defaultVars_))
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
        lua_close(L_);
        L_ = nullptr;
        return false;
    }
    
    // Execute the chunk to define functions
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        std::string error = std::string("Execution error: ") + lua_tostring(L_, -1);
        debug_.log(error, 1.0f, 0.0f, 0.0f);
        lua_pop(L_, 1);
        lua_close(L_);
        L_ = nullptr;
        return false;
    }
    
    // Increment version and mark as compiled
    version_++;
    compiled_ = true;

    // Log success
    debug_.log("Compilation successful", 0.0f, 1.0f, 0.0f);

    // Cache default variables from vars() function
    cacheDefaultVars();

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
        lua_pop(targetL, 1);
        return false;
    }
    
    return true;
}

void tinyScript::cacheDefaultVars() {
    defaultVars_.clear(); // Clear previous cache
    
    if (!valid()) return;

    // Check if script has a "vars" function
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
            } else if (lua_islightuserdata(L_, -1)) {
                // Handle as lightuserdata with isNode flag in high bit
                uint64_t packed = reinterpret_cast<uint64_t>(lua_touserdata(L_, -1));
                bool isNodeHandle = (packed & (1ULL << 63)) != 0;
                tinyHandle h;
                h.index = static_cast<uint32_t>((packed & 0x7FFFFFFFFFFFF000ULL) >> 32);
                h.version = static_cast<uint32_t>(packed & 0xFFFFFFFF);
                
                // Create typeHandle: type int for nodes, type void for files
                typeHandle th = isNodeHandle ? typeHandle::make<int>(h) : typeHandle::make<void>(h);
                defaultVars_[key] = th;
            }
        }
        
        lua_pop(L_, 1);  // Pop value, keep key for next iteration
    }

    lua_pop(L_, 1);  // Pop the table
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
                // Push handle as lightuserdata with isNode flag in high bit
                // type int = node handle, type void = file handle
                bool isNodeHandle = val.isType<int>();
                uint64_t packed = (isNodeHandle ? (1ULL << 63) : 0ULL) |
                                 (static_cast<uint64_t>(val.handle.index) << 32) | 
                                 val.handle.version;
                lua_pushlightuserdata(L_, reinterpret_cast<void*>(packed));
                lua_setfield(L_, -2, key.c_str());
            }
        }, value);
    }
    
    lua_setglobal(L_, "vars");  // Set as global "vars" table

    // ========== Push context information ==========
    // Push dTime
    lua_pushnumber(L_, dTime);
    lua_setglobal(L_, "dTime");
    
    // Push scene as a Scene userdata object
    pushScene(L_, static_cast<tinyRT::Scene*>(scene));
    lua_setglobal(L_, "scene");
    
    // Keep legacy __scene for compatibility
    lua_pushlightuserdata(L_, scene);
    lua_setglobal(L_, "__scene");
    
    // Push node as a Node userdata object
    pushNode(L_, nodeHandle);
    lua_setglobal(L_, "node");

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

void tinyScript::init() {
    if (name.empty()) name = "Script";

code = R"(
-- Character Controller (Multi-Node Edition)
-- This script controls TWO separate nodes:
--   rootNode: Movement and rotation
--   animeNode: Animation playback
-- Drag node handles from scene hierarchy into the fields below

function vars()
    return {
        -- Node references (drag nodes from scene hierarchy)
        rootNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),   -- Root node for movement
        animeNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),  -- Animation node
        otherNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),  -- Other node for interaction

        -- Stats
        isPlayer = true,
        vel = 2.0,
        hp = 100.0,
        maxHp = 100.0,
        isDead = false,

        -- Animation names (configure for your model)
        idleAnim = "Idle_Loop",
        walkAnim = "Walk_Loop",
        runAnim = "Sprint_Loop",
        deathAnim = "Death01"  -- Death animation (non-looping)
    }
end

function update()
    -- Get node references
    local root = scene:getNode(vars.rootNode)
    local anime = scene:getNode(vars.animeNode)
    local other = scene:getNode(vars.otherNode)

    -- ========== HEALTH MANAGEMENT ==========
    -- Clamp HP
    if vars.hp < 0 then
        vars.hp = 0
    end
    if vars.hp > vars.maxHp then
        vars.hp = vars.maxHp
    end

    -- Check for death
    if vars.hp <= 0 and not vars.isDead then
        vars.isDead = true
        print("Player died!")
    end

    -- If dead, play death animation (non-looping) and return early
    if vars.isDead and anime then
        local deathHandle = anime:getAnimHandle(vars.deathAnim)
        local curHandle = anime:getCurAnimHandle()
        
        -- Only play death animation once
        if not handleEqual(curHandle, deathHandle) then
            anime:setAnimLoop(false)  -- Death animation doesn't loop
            anime:playAnim(deathHandle, true)
        else
            -- Check if death animation has finished
            local animTime = anime:getAnimTime()
            local animDuration = anime:getAnimDuration()
            
            if animTime >= animDuration - 0.01 then
                -- Death animation finished, pause at last frame
                anime:pauseAnim()
            end
        end
        
        return  -- Don't process movement or other logic while dead
    end

    -- ========== DAMAGE FROM PROXIMITY ==========
    if root and other and vars.isPlayer and not vars.isDead then
        local rootPos = root:getPos()
        local otherPos = other:getPos()
        local dx = rootPos.x - otherPos.x
        local dy = rootPos.y - otherPos.y
        local dz = rootPos.z - otherPos.z
        local distSq = dx * dx + dy * dy + dz * dz

        if distSq <= 1.0 then -- Tick damage
            vars.hp = vars.hp - 10.0 * dTime
        end
    end

    -- ========== INPUT DETECTION ==========
    local k_up = kState("up")
    local k_down = kState("down")
    local k_left = kState("left")
    local k_right = kState("right")
    local running = kState("shift")
    
    -- Calculate movement direction
    local vz = (k_up and 1 or 0) - (k_down and 1 or 0)
    local vx = (k_right and -1 or 0) - (k_left and -1 or 0) -- I really need to fix the coord system lol
    local isMoving = (vx ~= 0) or (vz ~= 0)
    local moveSpeed = ((running and isMoving) and 4.0 or 1.0) * vars.vel
    
    -- ========== MOVEMENT (Root Node) ==========
    if root and isMoving then
        local pos = root:getPos()
        
        -- Calculate normalized movement direction
        local moveDir = {x = vx, y = 0, z = vz}
        local length = math.sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z)
        if length > 0 then
            moveDir.x = moveDir.x / length
            moveDir.z = moveDir.z / length
        end
        
        -- Apply movement
        pos.x = pos.x + moveDir.x * moveSpeed * dTime
        pos.z = pos.z + moveDir.z * moveSpeed * dTime
        root:setPos(pos)
        
        -- Apply rotation (face movement direction)
        local targetYaw = math.atan(moveDir.x, moveDir.z)
        root:setRot({x = 0, y = targetYaw, z = 0})
    end
    
    -- ========== ANIMATION (Anime Node) ==========
    if anime then
        -- Get animation handles
        local idleHandle = anime:getAnimHandle(vars.idleAnim)
        local walkHandle = anime:getAnimHandle(vars.walkAnim)
        local runHandle = anime:getAnimHandle(vars.runAnim)
        local curHandle = anime:getCurAnimHandle()
        
        -- Ensure looping is enabled for normal animations
        if not anime:getAnimLoop() then
            anime:setAnimLoop(true)
        end
        
        -- Set animation speed
        anime:setAnimSpeed(isMoving and moveSpeed or 1.0)
        
        if isMoving then
            -- Choose run or walk
            local playHandle = running and runHandle or walkHandle
            
            -- Only restart when switching from idle to walk/run
            local shouldRestart = not (handleEqual(curHandle, runHandle) or 
                                       handleEqual(curHandle, walkHandle))
            anime:playAnim(playHandle, shouldRestart)
        else
            -- Play idle
            local shouldRestart = not handleEqual(curHandle, idleHandle)
            anime:playAnim(idleHandle, shouldRestart)
        end
    end
end
)";

    // Automatically compile the test script
    compile();
}
