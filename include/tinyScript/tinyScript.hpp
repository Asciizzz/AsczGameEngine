#pragma once

#include "tinyLua.hpp"
#include "tinyVariable.hpp"

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

    void update(void* rtScript, void* scene, tinyHandle nodeHandle, float deltaTime) const;

    // Get internal Lua state for advanced operations (use with caution!)
    lua_State* luaState() const { return luaInstance_.state(); }

    bool valid() const { return compiled_ && luaInstance_.valid(); }
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
    tinyLua::Instance luaInstance_;
    uint16_t version_ = 0;
    bool compiled_ = false;

    // On init and on compile callbacks
    tinyLua::OnInitFunc onInit_ = nullptr;
    tinyLua::OnCompileFunc onCompile_ = nullptr;

    tinyVarsMap defaultVars_;    // Default VARS from script
    tinyVarsMap defaultLocals_;  // Default LOCALS from script

    tinyVarsMap globals_;        // Global variables (tied to the script itself, not instances)

    std::vector<std::string> varsOrder_;  // Ordered list of var names (sorted by type)
    tinyDebug debug_{16};  // Compilation/static debug logs (16 lines max)

    void cacheDefaultVars();
    void cacheDefaultLocals();

    void cacheDefaultTable(const char* tableName, tinyVarsMap& outMap);  // Shared implementation

    void initTable(tinyVarsMap& outTable, const tinyVarsMap& defaultTable) const;
};
