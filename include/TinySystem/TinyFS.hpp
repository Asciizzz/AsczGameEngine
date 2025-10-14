#pragma once

#include ".ext/Templates.hpp"

#include <fstream>
#include <sstream>

struct TinyFNode {
    std::string name; // No need for path, path will be resolved by tree structure

    TinyFNode* parent = nullptr;
    UniquePtrVec<TinyFNode> children;
};



class TinyFS {

};