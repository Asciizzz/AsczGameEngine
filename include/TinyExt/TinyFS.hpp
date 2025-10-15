#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <fstream>
#include <sstream>

#include <iostream>

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

    TinyFNode* getNode(TinyHandle handle) {
        return fnodes.get(handle);
    }

public:
    TinyFS() {
        // create root node
        TinyFNode rootNode;
        rootNode.name = ".";
        fnodes.insert(std::move(rootNode)); // index 0
    }

    TinyHandle getHandle(uint32_t index) const { return fnodes.getHandle(index); }
    TinyHandle rootHandle() const { return getHandle(0); }

    const TinyRegistry& getRegistry() const { return registry; }

    void setRoot(const std::string& root) {
        if (fnodes.isValid(rootHandle())) {
            fnodes.get(rootHandle())->name = root;
        }
    }

    void setRegistryHandle(TinyHandle handle) {
        if (fnodes.isValid(handle)) registryHandle = handle;
    }

    const TinyFNode* getFNode(TinyHandle handle) const {
        return fnodes.get(handle);
    }

    TypeHandle fnToRHandle(TinyHandle fileHandle) const {
        if (!fnodes.isValid(fileHandle)) return TypeHandle();

        const TinyFNode* node = fnodes.get(fileHandle);
        return node->metaHandle;
    }

    template<typename T>
    TinyHandle addToReg(T& data) {
        return registry.add(data).handle;
    }


    template<typename T>
    T* fileData(TinyHandle fileHandle) {
        if (!fnodes.isValid(fileHandle)) return nullptr;

        TinyFNode* node = fnodes.get(fileHandle);
        if (!node || !node->isFile()) return nullptr;

        // Retrieve the registry data
        return registry.get<T>(node->metaHandle.handle);
    }

    template<typename T>
    T* registryData(const TinyHandle& handle) {
        return registry.get<T>(handle);
    }

    template<typename T>
    TinyHandle addFNode(TinyHandle parentHandle, const std::string& name, T* data) {
        if (!fnodes.isValid(parentHandle)) return TinyHandle(); // invalid handle
        TinyFNode child;
        child.name = name;
        child.parent = parentHandle;

        if (data) child.metaHandle = registry.add(data);

        TinyHandle handle = fnodes.insert(std::move(child));
        fnodes.get(parentHandle)->children.push_back(handle);
        return handle;
    }

    template<typename T>
    TinyHandle addFNode(const std::string& name, T* data) {
        return addFNode(rootHandle(), name, data);
    }

    // Non-templated version for folders
    TinyHandle addFNode(TinyHandle parentHandle, const std::string& name) {
        return addFNode<void>(parentHandle, name, nullptr);
    }

    TinyHandle addFNode(const std::string& name) {
        return addFNode<void>(rootHandle(), name, nullptr);
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

    void debugPrint() const {
        debugPrintRecursive(rootHandle(), 0);
    }

private:
    void debugPrintRecursive(TinyHandle handle, int indent) const {
        if (!fnodes.isValid(handle)) return;

        const TinyFNode* node = fnodes.get(handle);
        if (!node) return;

        // Indentation
        for (int i = 0; i < indent; ++i) std::cout << "  ";

        // Print node: red if file
        if (node->isFile()) {
            // Print additional meta info
            std::cout << "\033[31m" << node->name <<
                " [File] (Index: " << node->metaHandle.handle.index
                << ", Version: " << node->metaHandle.handle.version
                << ", TypeHash: " << node->metaHandle.typeHash << ")\033[0m\n";
        } else {
            std::cout << node->name << "\n";
        }

        // Recurse into children
        for (TinyHandle child : node->children) {
            debugPrintRecursive(child, indent + 1);
        }
    }
};