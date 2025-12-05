#pragma once

#include "tinyLuaDef.hpp"

namespace tinyRT {

// Lightweight container containing nothing but script handle and variable storage
struct Script {
    void set(tinyHandle scriptHandle) { scriptHandle_ = scriptHandle; }
    tinyHandle scriptHandle() const { return scriptHandle_; }

    tinyVarsMap& vars() { return vars_; }
    const tinyVarsMap& vars() const { return vars_; }

    tinyVarsMap& locals() { return locals_; }
    const tinyVarsMap& locals() const { return locals_; }

    tinyDebug& debug() { return debug_; }
    const tinyDebug& debug() const { return debug_; }

    void copy(const Script* other) {
        if (!other) return;

        scriptHandle_ = other->scriptHandle_;
        vars_ = other->vars_;
        locals_ = other->locals_;
        debug_ = other->debug_;
    }
private:
    tinyHandle scriptHandle_;

    tinyVarsMap vars_;
    tinyVarsMap locals_;
    tinyDebug   debug_;
};

} // namespace tinyRT

using rtSCRIPT = tinyRT::Script;