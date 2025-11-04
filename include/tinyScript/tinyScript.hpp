#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <glm/glm.hpp>
#include "tinyExt/tinyHandle.hpp"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

using tinyVar = std::variant<float, int, bool, glm::vec3, std::string>;

// Static script definition - shared across all instances
struct tinyScript {
    std::string name;
    std::string code;

    tinyScript() = default;
    tinyScript(const std::string& scriptName) : name(scriptName) {}
    ~tinyScript();
    
    tinyScript(const tinyScript&) = delete;
    tinyScript& operator=(const tinyScript&) = delete;

    tinyScript(tinyScript&& other) noexcept;
    tinyScript& operator=(tinyScript&& other) noexcept;

    bool compile();

    void initRtVars(std::unordered_map<std::string, tinyVar>& vars) const;

    void update(std::unordered_map<std::string, tinyVar>& vars, void* scene, tinyHandle nodeHandle, float dTime) const;

    bool call(const char* functionName, lua_State* runtimeL = nullptr) const;

    bool valid() const { return compiled_ && L_ != nullptr; }
    uint32_t version() const { return version_; }

    void test(); // Generate a demo spinning script

private:
    uint32_t version_ = 0;
    lua_State* L_ = nullptr;
    bool compiled_ = false;

    void closeLua();
};
