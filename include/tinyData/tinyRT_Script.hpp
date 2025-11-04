#pragma once

#include "tinyData/tinyScript.hpp"
#include "tinyExt/tinyHandle.hpp"
#include "tinyExt/tinyPool.hpp"

#include <unordered_map>
#include <string>
#include <variant>
#include <glm/glm.hpp>

namespace tinyRT {

struct Scene;

// Per-instance runtime script data container
struct Script {
    Script() noexcept = default;
    ~Script() = default;
    
    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    
    Script(Script&&) noexcept = default;
    Script& operator=(Script&&) noexcept = default;
    
    // Initialize with script pool (mimics tinyRT_SKEL3D pattern)
    void init(const tinyPool<tinyScript>* scriptPool);
    
    // Assign a script and initialize runtime variables
    void assign(tinyHandle scriptHandle);
    
    // Check validity (script assigned and version matches)
    bool valid() const;
    
    // Update/execute script
    void update(Scene* scene, tinyHandle nodeHandle, float dTime);
    
    // Getters (mimics tinyRT_SKEL3D pattern)
    tinyHandle scriptHandle() const { return scriptHandle_; }
    const tinyScript* rScript() const {
        return scriptPool_ ? scriptPool_->get(scriptHandle_) : nullptr;
    }
    bool hasScript() const { return rScript() != nullptr; }
    
    // ========== Runtime Variable Access ==========
    // Type-safe variable access - returns nullptr if not found or type mismatch
    
    template<typename T>
    T* var(const std::string& key) {
        auto it = vars_.find(key);
        if (it != vars_.end() && std::holds_alternative<T>(it->second)) {
            return &std::get<T>(it->second);
        }
        return nullptr;
    }
    
    template<typename T>
    const T* var(const std::string& key) const {
        return const_cast<Script*>(this)->var<T>(key);
    }

    template<typename T>
    T* set(const std::string& key, const T& value) {
        vars_[key] = value;
        return &std::get<T>(vars_[key]);
    }

private:
    tinyHandle scriptHandle_;
    const tinyPool<tinyScript>* scriptPool_ = nullptr;
    uint32_t cachedVersion_ = 0;  // Cache script version for validity check

    std::unordered_map<std::string, tinyVar> vars_;

    void checkAndReload();
};

} // namespace tinyRT

using tinyRT_SCRIPT = tinyRT::Script;
