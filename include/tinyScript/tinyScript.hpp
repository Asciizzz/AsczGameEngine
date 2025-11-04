#pragma once

#include <string>
#include <cstdint>
#include <map>
#include <variant>
#include <glm/glm.hpp>
#include "tinyExt/tinyHandle.hpp"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

// Script handle - wraps tinyHandle with a type tag
// - isNodeHandle = true: Handle to scene node (needs remapping on scene load)
// - isNodeHandle = false: Handle to file/resource (global, never remapped)
struct scriptHandle {
    tinyHandle handle;
    bool isNodeHandle = false;
    
    scriptHandle() = default;
    scriptHandle(tinyHandle h, bool isNode) : handle(h), isNodeHandle(isNode) {}
    
    bool valid() const { return handle.valid(); }
};

using tinyVar = std::variant<float, int, bool, glm::vec3, std::string, scriptHandle>;
using tinyVarsMap = std::map<std::string, tinyVar>; // Using ordered map for consistent display order

// Static script definition - shared across all instances
struct tinyScript {
    std::string name;
    std::string code;

    tinyScript() { init(); }
    tinyScript(const std::string& scriptName) : name(scriptName) { init(); }
    ~tinyScript();
    
    tinyScript(const tinyScript&) = delete;
    tinyScript& operator=(const tinyScript&) = delete;

    tinyScript(tinyScript&& other) noexcept;
    tinyScript& operator=(tinyScript&& other) noexcept;

    bool compile();

    void update(tinyVarsMap& vars, void* scene, tinyHandle nodeHandle, float dTime) const;

    bool call(const char* functionName, lua_State* runtimeL = nullptr) const;

    bool valid() const { return compiled_ && L_ != nullptr; }
    uint32_t version() const { return version_; }
    const std::string& error() const { return error_; }

    // For making copies
    const tinyVarsMap& defaultVars() const { return defaultVars_; }

private:
    void init();

    std::string error_; // Cached error messages

    uint32_t version_ = 0;
    lua_State* L_ = nullptr;
    bool compiled_ = false;

    tinyVarsMap defaultVars_;
    void cacheDefaultVars();

    void closeLua();
};
