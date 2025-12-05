#pragma once

#include "tinyVariable.hpp"

namespace tinyRT {

// Lightweight container containing nothing but script handle and variable storage
struct Script {
    tinyHandle scriptHandle;
    uint16_t cacheVersion = 0;

    tinyVarsMap vars;
    tinyVarsMap locals;
    tinyDebug   debug;
};

} // namespace tinyRT

using rtScript = tinyRT::Script;
using rtSCRIPT = tinyRT::Script;