#pragma once

extern "C" {
    #include "luacpp/lua.h"
    #include "luacpp/lualib.h"
    #include "luacpp/lauxlib.h"
}

#include <functional>
#include <string>

namespace tinyLua {

// -----------------------------
// Lua Instance
// -----------------------------
using BindFunc = std::function<void(lua_State*)>;
using OnInitFunc = std::function<void(bool success)>;
using OnCompileFunc = std::function<void(bool success, const std::string& errorMsg)>;
using OnCallFunc = std::function<void(bool success, const std::string& errorMsg)>;

class Instance {
public:
    Instance() = default;
    ~Instance() { close(); }
    
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    
    Instance(Instance&& other) noexcept 
        : L_(other.L_)
        , bindFunc_(std::move(other.bindFunc_))
        , onInit_(std::move(other.onInit_))
        , onCompile_(std::move(other.onCompile_))
        , onCall_(std::move(other.onCall_)) {
        other.L_ = nullptr;
    }
    
    Instance& operator=(Instance&& other) noexcept {
        if (this != &other) {
            close();
            L_ = other.L_;
            bindFunc_ = std::move(other.bindFunc_);
            onInit_ = std::move(other.onInit_);
            onCompile_ = std::move(other.onCompile_);
            onCall_ = std::move(other.onCall_);
            other.L_ = nullptr;
        }
        return *this;
    }
    
    // Set custom bindings (call before init)
    void setBindings(BindFunc func) {
        bindFunc_ = func;
    }
    
    // Set callbacks
    void onInit(OnInitFunc func) { onInit_ = func; }
    void onCompile(OnCompileFunc func) { onCompile_ = func; }
    void onCall(OnCallFunc func) { onCall_ = func; }
    
    // Initialize state
    bool init() {
        close();
        
        L_ = luaL_newstate();
        if (!L_) {
            if (onInit_) onInit_(false);
            return false;
        }
        
        luaL_openlibs(L_);
        
        if (bindFunc_) {
            bindFunc_(L_);
        }
        
        if (onInit_) onInit_(true);
        return true;
    }
    
    // Compile code
    bool compile(const std::string& code) {
        if (!L_) {
            if (onCompile_) onCompile_(false, "Lua state not initialized");
            return false;
        }
        
        if (luaL_loadstring(L_, code.c_str()) != LUA_OK) {
            std::string error = std::string("Compilation error: ") + lua_tostring(L_, -1);
            lua_pop(L_, 1);
            if (onCompile_) onCompile_(false, error);
            return false;
        }
        
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            std::string error = std::string("Execution error: ") + lua_tostring(L_, -1);
            lua_pop(L_, 1);
            if (onCompile_) onCompile_(false, error);
            return false;
        }
        
        if (onCompile_) onCompile_(true, "Compilation successful");
        return true;
    }
    
    // Call a function
    bool call(const char* functionName) const {
        if (!L_) {
            if (onCall_) onCall_(false, "Lua state not initialized");
            return false;
        }
        
        lua_getglobal(L_, functionName);
        
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            std::string error = std::string("Function '") + functionName + "' not found";
            if (onCall_) onCall_(false, error);
            return false;
        }
        
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            std::string error = std::string("Runtime error in '") + functionName + "': " + lua_tostring(L_, -1);
            lua_pop(L_, 1);
            if (onCall_) onCall_(false, error);
            return false;
        }
        
        if (onCall_) onCall_(true, "");
        return true;
    }
    
    // Close state
    void close() {
        if (!L_) return;

        lua_close(L_);
        L_ = nullptr;
    }
    
    // Access raw Lua state
    lua_State* state() const { return L_; }
    bool valid() const { return L_ != nullptr; }

private:
    lua_State* L_ = nullptr;
    BindFunc bindFunc_;

    OnInitFunc onInit_;
    OnCompileFunc onCompile_;
    OnCallFunc onCall_;
};

} // namespace tinyLua
