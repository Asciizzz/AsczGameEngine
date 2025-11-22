#pragma once

#include "tinyType.hpp"
#include <string>

namespace tinyRT {

struct Node {
    std::string name = "Node";
    tinyHandle parent;
    std::vector<tinyHandle> children;

    tinyHandle TRANF3D;
    tinyHandle MESHR3D;
    tinyHandle BONE3D;
    tinyHandle SKELE3D;
    tinyHandle ANIME3D;

    tinyHandle TRANF2D;
    tinyHandle MESHR2D;

    tinyHandle SCRIPT;
};

}

using rtNode = tinyRT::Node;