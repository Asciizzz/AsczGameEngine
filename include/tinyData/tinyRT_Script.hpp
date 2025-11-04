#pragma once

#include "tinyScript/tinyScript.hpp"
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
    void init(const tinyPool<tinyScript>* scriptPool);

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    
    Script(Script&&) noexcept = default;
    Script& operator=(Script&&) noexcept = default;

    bool valid() const;

    void assign(tinyHandle scriptHandle);

    void update(Scene* scene, tinyHandle nodeHandle, float dTime);

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

    template<typename T> // Builder pattern
    Script& vSet(const std::string& key, const T& value) {
        vars_[key] = value;
        return *this;
    }

    bool vHas(const std::string& key) const {
        return vars_.find(key) != vars_.end();
    }

private:
    tinyHandle scriptHandle_;
    const tinyPool<tinyScript>* scriptPool_ = nullptr;
    uint32_t cachedVersion_ = 0;

    tinyVarsMap vars_;

    void checkAndReload();
};

} // namespace tinyRT

using tinyRT_SCRIPT = tinyRT::Script;
