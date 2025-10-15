#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <fstream>
#include <sstream>

struct TinyFNode {
    std::string name; // path segment
    TinyHandle parent;
    std::vector<TinyHandle> children; // child node handles

    TypeHandle metaHandle; // Similar to godot.imported or unity.meta
    bool isFile() const { return metaHandle.valid(); }
};

class TinyFS {
    TinyPool<TinyFNode> fnodes;

    TinyRegistry registry;
    TinyHandle registryHandle; // Folder to store registry data

    TinyFS() {
        // create root node
        TinyFNode rootNode;
        rootNode.name = ".";
        fnodes.insert(std::move(rootNode)); // index 0
    }

    TinyFNode* getNode(TinyHandle handle) {
        return fnodes.get(handle);
    }

public:
    TinyHandle getHandle(uint32_t index) const { return fnodes.getHandle(index); }
    TinyHandle rootHandle() const { return getHandle(0); }

    void setRoot(const std::string& root) {
        if (!fnodes.isValid(rootHandle())) return;
        fnodes.get(rootHandle())->name = root;
    }

    void setRegistryHandle(TinyHandle handle) {
        if (fnodes.isValid(handle)) registryHandle = handle;
    }

    TinyHandle addFNode(TinyHandle parentHandle, const std::string& name) {
        if (!fnodes.isValid(parentHandle)) return TinyHandle(); // invalid handle
        TinyFNode child;
        child.name = name;
        child.parent = parentHandle;

        TinyHandle handle = fnodes.insert(std::move(child));
        fnodes.get(parentHandle)->children.push_back(handle);
        return handle;
    }

    template<typename T>
    TinyHandle addFile(TinyHandle parentHandle, const std::string& name, const T& data) {
        TinyHandle handle = addFNode(parentHandle, name);
        TypeHandle thandle = registry.add(data);
        if (!handle.valid()) return handle;

        TinyFNode* node = fnodes.get(handle);
        if (node) node->metaHandle = thandle;

        return handle;
    }

    void moveFNode(TinyHandle nodeHandle, TinyHandle newParent) {
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

    void removeFNode(TinyHandle handle) {
        if (!fnodes.isValid(handle)) return;

        TinyFNode* node = fnodes.get(handle);
        if (!node) return;

        // Remove registry data if file
        if (node->isFile()) {
            registry.remove(node->metaHandle);
            node->metaHandle = TypeHandle(); // invalidate
        }

        // Remove children recursively
        for (TinyHandle child : node->children) {
            removeFNode(child);
        }

        // Remove from parent
        TinyFNode* parent = fnodes.get(node->parent);
        if (parent) {
            auto& siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), handle), siblings.end());
        }

        fnodes.remove(handle);
    }

    std::string getFullPath(TinyHandle handle) const {
        if (!fnodes.isValid(handle)) return "";

        std::string path;
        TinyHandle current = handle;
        while (fnodes.isValid(current)) {
            const auto* node = fnodes.get(current);
            path = "/" + node->name + path;
            current = node->parent;
            if (current.value == 0) break; // stop at root
        }
        // prepend root name
        if (fnodes.isValid(rootHandle())) {
            path = fnodes.get(rootHandle())->name + path;
        }
        return path;
    }
};
