#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <fstream>
#include <sstream>

struct TinyFNode {
    std::string road; // path segment
    TinyHandle parent;
    std::vector<TinyHandle> children; // child node handles

    TypeHandle metaHandle; // Similar to godot.imported or unity.meta
    bool isFolder() const { return metaHandle.valid(); }
};

class TinyFS {
    TinyPool<TinyFNode> fnodes;
    TinyRegistry registry;

    TinyHandle asczHandle; // Folder to store engine data
    TinyHandle registryHandle; // Folder to store registry data

public:
    TinyHandle getHandle(uint32_t index) const { return fnodes.getHandle(index); }
    TinyHandle rootHandle() const { return getHandle(0); }

    void init(const std::string& root) {
        // create root node
        TinyFNode rootNode;
        rootNode.road = root;
        fnodes.insert(std::move(rootNode)); // index 0

        // create root/.ascz folder
        asczHandle = addChild(rootHandle(), ".ascz");

        // create root/.ascz/registry folder
        registryHandle = addChild(asczHandle, "registry");

        // more folders in the future
    }

    // Add child to parent node
    TinyHandle addChild(TinyHandle parentHandle, const std::string& road) {
        if (!fnodes.isValid(parentHandle)) return TinyHandle(); // invalid handle
        TinyFNode child;
        child.road = road;
        child.parent = parentHandle;

        TinyHandle handle = fnodes.insert(std::move(child));
        fnodes.get(parentHandle)->children.push_back(handle);
        return handle;
    }

    // Get full path by traversing parents
    std::string getFullPath(TinyHandle handle) const {
        if (!fnodes.isValid(handle)) return "";

        std::string path;
        TinyHandle current = handle;
        while (fnodes.isValid(current)) {
            const auto* node = fnodes.get(current);
            path = "/" + node->road + path;
            current = node->parent;
            if (current.value == 0) break; // stop at root
        }
        // prepend root road
        if (fnodes.isValid(rootHandle())) {
            path = fnodes.get(rootHandle())->road + path;
        }
        return path;
    }

    // Move node to new parent
    void moveNode(TinyHandle nodeHandle, TinyHandle newParent) {
        if (!fnodes.isValid(nodeHandle) || !fnodes.isValid(newParent)) return;

        TinyFNode* node = fnodes.get(nodeHandle);
        TinyFNode* oldParent = fnodes.get(node->parent);
        if (oldParent) {
            auto& siblings = oldParent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), nodeHandle), siblings.end());
        }

        node->parent = newParent;
        fnodes.get(newParent)->children.push_back(nodeHandle);
    }

    // Optional: remove node (recursive)
    void removeNode(TinyHandle handle) {
        if (!fnodes.isValid(handle)) return;
        TinyFNode* node = fnodes.get(handle);

        // Remove children recursively
        for (TinyHandle child : node->children) {
            removeNode(child);
        }

        // Remove from parent
        TinyFNode* parent = fnodes.get(node->parent);
        if (parent) {
            auto& siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), handle), siblings.end());
        }

        fnodes.remove(handle);
    }
};
