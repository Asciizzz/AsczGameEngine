#include "tinyData/tinyRT_Script.hpp"
#include "tinyData/tinyRT_Scene.hpp"
#include <iostream>

using namespace tinyRT;

// ==================== tinyRT_SCRIPT Implementation ====================

void Script::init(const tinyPool<tinyScript>* scriptPool) {
    scriptPool_ = scriptPool;
}

void Script::assign(tinyHandle scriptHandle) {
    vars_.clear();
    cachedVersion_ = 0;
    scriptHandle_ = scriptHandle;

    const tinyScript* script = rScript();
    if (!script) return;

    cachedVersion_ = script->version();
    script->initVars(vars_);
}

bool Script::valid() const {
    const tinyScript* script = rScript();
    if (!script || !script->valid()) return false;

    return script->version() == cachedVersion_;
}

void Script::update(Scene* scene, tinyHandle nodeHandle, float dTime) {
    const tinyScript* script = rScript();
    if (!script) return;

    // Check for version change (hot reload detected)
    if (script->version() != cachedVersion_) {
        cachedVersion_ = script->version();
        script->initVars(vars_); // Smart init
    } else {
        script->update(this, scene, nodeHandle, dTime, &debug_);
    }
}
