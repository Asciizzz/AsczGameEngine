#pragma once

#include "tinyLuaDef.hpp"

extern "C" {
    #include "luacpp/lua.h"
    #include "luacpp/lualib.h"
    #include "luacpp/lauxlib.h"
}

// Update need: rework tinyScript to be independent of this engine context
// allowing it to be used in other projects more easily.

// Static script definition - shared across all instances
struct tinyScript {
    std::string code;

    tinyScript() = default;
    ~tinyScript();
    
    tinyScript(const tinyScript&) = delete;
    tinyScript& operator=(const tinyScript&) = delete;

    tinyScript(tinyScript&& other) noexcept;
    tinyScript& operator=(tinyScript&& other) noexcept;

    bool compile();

    void update(void* rtScript, void* scene, tinyHandle nodeHandle, float deltaTime, tinyDebug* runtimeDebug = nullptr) const;
    bool call(const char* functionName, lua_State* runtimeL = nullptr, tinyDebug* runtimeDebug = nullptr) const;
    
    // Get internal Lua state for advanced operations (use with caution!)
    lua_State* luaState() const { return L_; }

    bool valid() const { return compiled_ && L_ != nullptr; }
    uint32_t version() const { return version_; }

    // Compilation debug logs (8-16 lines max, FIFO)
    tinyDebug& debug() { return debug_; }
    const tinyDebug& debug() const { return debug_; }

    // For making copies
    const tinyVarsMap& defaultVars() const { return defaultVars_; }
    const tinyVarsMap& defaultLocals() const { return defaultLocals_; }
    const std::vector<std::string>& varsOrder() const { return varsOrder_; }

    void initVars(tinyVarsMap& outVars) const;
    void initLocals(tinyVarsMap& outLocals) const;

private:
    uint32_t version_ = 0;
    lua_State* L_ = nullptr;
    bool compiled_ = false;

    tinyVarsMap defaultVars_;    // Default VARS from script
    tinyVarsMap defaultLocals_;  // Default LOCALS from script

    tinyVarsMap globals_;        // Global variables (tied to the script itself, not instances)

    std::vector<std::string> varsOrder_;  // Ordered list of var names (sorted by type)
    tinyDebug debug_{16};  // Compilation/static debug logs (16 lines max)
    void closeLua();

    void cacheDefaultVars();
    void cacheDefaultLocals();

    void cacheDefaultTable(const char* tableName, tinyVarsMap& outMap);  // Shared implementation

    void initTable(tinyVarsMap& outTable, const tinyVarsMap& defaultTable) const;
};
