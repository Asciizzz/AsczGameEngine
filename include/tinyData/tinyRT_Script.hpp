#pragma once

#include "tinyScript/tinyScript.hpp"
#include "tinyExt/tinyPool.hpp"

namespace tinyRT {

struct Scene;

// Per-instance runtime script data container
struct Script {
    Script() noexcept = default;
    void init(const tinyPool<tinyScript>* scriptPool);

    // Script are allowed to be copyable
    Script(const Script&) = default;
    Script& operator=(const Script&) = default;
    
    Script(Script&&) noexcept = default;
    Script& operator=(Script&&) noexcept = default;

    bool valid() const;

    void assign(tinyHandle scriptHandle);

    void update(Scene* scene, tinyHandle nodeHandle, float deltaTime);

    tinyHandle scriptHandle() const { return scriptHandle_; }
    const tinyScript* rScript() const {
        return scriptPool_ ? scriptPool_->get(scriptHandle_) : nullptr;
    }
    bool hasScript() const { return rScript() != nullptr; }
    
    // ========== Runtime Variable Access ==========
    // Type-safe variable access - returns nullptr if not found or type mismatch

    template<typename T>
    T* vGet(const std::string& key) {
        auto it = vars_.find(key);
        if (it != vars_.end() && std::holds_alternative<T>(it->second)) {
            return &std::get<T>(it->second);
        }
        return nullptr;
    }
    
    template<typename T>
    const T* vGet(const std::string& key) const {
        return const_cast<Script*>(this)->var<T>(key);
    }

    tinyVarsMap& vMap() { return vars_; }
    const tinyVarsMap& vMap() const { return vars_; }
    
    // Local variables (private to this script, not accessible via getVar/setVar)
    tinyVarsMap& lMap() { return locals_; }
    const tinyVarsMap& lMap() const { return locals_; }
    
    // Get the sorted order of variable names (sorted by type, then alphabetically)
    const std::vector<std::string>& vOrder() const {
        const tinyScript* script = rScript();
        static const std::vector<std::string> empty;
        return script ? script->varsOrder() : empty;
    }

    template<typename T> // Builder pattern
    Script& vSet(const std::string& key, const T& value) {
        vars_[key] = value;
        return *this;
    }

    bool vHas(const std::string& key) const {
        return vars_.find(key) != vars_.end();
    }

    // Runtime debug logs (FIFO, for real-time broken values)
    tinyDebug& debug() { return debug_; }
    const tinyDebug& debug() const { return debug_; }

    // UI state for debug window toggle
    bool showDebugWindow = false;

private:
    tinyHandle scriptHandle_;
    const tinyPool<tinyScript>* scriptPool_ = nullptr;
    uint32_t cachedVersion_ = 0;

    tinyVarsMap vars_;    // Public variables (accessible via getVar/setVar)
    tinyVarsMap locals_;  // Private variables (only accessible within the script)
    tinyDebug debug_{128};  // Runtime debug logs (128 lines max for real-time values)
};

} // namespace tinyRT

using tinyRT_SCRIPT = tinyRT::Script;
