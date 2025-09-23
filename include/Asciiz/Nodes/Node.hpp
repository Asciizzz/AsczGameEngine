#pragma once

#include "Helpers/Templates.hpp"
#include <string>

namespace Asciiz {

class Node {
protected:
    std::string name;
    Node* parent = nullptr;
    UniquePtrVec<Node> children;

public:
    Node(const std::string& n = "Default") : name(n) {}

    // Scene tree management
    void add_child(std::unique_ptr<Node> child) {
        child->parent = this;
        children.push_back(std::move(child));
    }

    const UniquePtrVec<Node>& get_children() const { return children; }
    Node* get_parent() const { return parent; }

    // Lifecycle
    virtual void ready() {}
    virtual void process(float delta) {}
    virtual void physics_process(float delta) {}
    
    virtual ~Node() = default;
};

}