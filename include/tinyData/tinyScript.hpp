#pragma once

#include <string>

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

struct tinyScript {
    std::string code;
};