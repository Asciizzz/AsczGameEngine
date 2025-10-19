#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <type_traits>
#include <sstream>
#include <cctype>

class TinyFS {
public:
    struct Node {
        std::string name;                     // segment name (relative)
        TinyHandle parent;                    // parent node handle
        std::vector<TinyHandle> children;     // child node handles
        TypeHandle tHandle;                   // metadata / registry handle if file

        enum class Type { Folder, File, Other } type = Type::Folder;

        struct CFG {
            bool hidden = false;
            bool deletable = true;
        } cfg;

        bool hidden() const { return cfg.hidden; }
        bool deletable() const { return cfg.deletable; }

        bool isFile() const { return type == Type::File; }
        bool isFolder() const { return type == Type::Folder; }

        bool hasData() const { return tHandle.valid(); }

        bool hasChild(TinyHandle childHandle) const {
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }

        bool addChild(TinyHandle childHandle) {
            if (hasChild(childHandle)) return false;
            children.push_back(childHandle);
            return true;
        }
    };

    TinyFS(bool caseSensitive = false) : caseSensitive_(caseSensitive) {
        Node rootNode;
        rootNode.name = ".root";
        rootNode.parent = TinyHandle();
        rootNode.type = Node::Type::Folder;
        rootNode.cfg.deletable = false; // root is not deletable

        // Insert root and store explicitly the returned handle
        rootHandle_ = fnodes.add(std::move(rootNode));
    }

    // ---------- Basic access ----------
    TinyHandle rootHandle() const { return rootHandle_; }
    TinyHandle regHandle() const { return regHandle_; }

    // Case sensitivity control
    bool isCaseSensitive() const { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) { caseSensitive_ = caseSensitive; }

    // Optional: expose registry for read-only access
    const TinyRegistry& registryRef() const { return registry; }
    TinyRegistry& registryRef() { return registry; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) {
        Node* root = fnodes.get(rootHandle_);
        if (root) root->name = rootPath;
    }

    // Explicitly set registry folder handle (if you create it manually)
    void setRegistryHandle(TinyHandle h) {
        if (!fnodes.isValid(h)) return;

        Node* node = fnodes.get(h);
        node->cfg.deletable = false; // make non-deletable
        node->cfg.hidden = true;     // hide the registry folder

        regHandle_ = h;
    }

    // ---------- Creation ----------

    // Folder creation (non-template overload)
    TinyHandle addFolder(TinyHandle parentHandle, const std::string& name, Node::CFG cfg = {}) {
        return addFNodeImpl<void>(parentHandle, name, nullptr, cfg);
    }
    TinyHandle addFolder(const std::string& name, Node::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    TinyHandle addFile(TinyHandle parentHandle, const std::string& name, T* data, Node::CFG cfg = {}) {
        return addFNodeImpl<T>(parentHandle, name, data, cfg);
    }
    template<typename T>
    TinyHandle addFile(const std::string& name, T* data, Node::CFG cfg = {}) {
        return addFile(rootHandle_, name, data, cfg);
    }

    template<typename T>
    TypeHandle addToRegistry(T&& val) {
        // Remove const and reference qualifiers (f you too)
        using CleanT = std::remove_cv_t<std::remove_reference_t<T>>;
        return registry.add<CleanT>(std::forward<T>(val));
    }

    // ---------- Move with cycle prevention ----------
    bool moveFNode(TinyHandle nodeHandle, TinyHandle newParent) {
        if (nodeHandle == newParent) return false; // Move to self

        // prevent moving under descendant (no cycles)
        Node* parent = fnodes.get(newParent);
        if (parent && parent->hasChild(nodeHandle)) return false;

        Node* node = fnodes.get(nodeHandle);
        if (!node || hasRepeatName(newParent, node->name)) return false;

        // remove from old parent children vector
        if (fnodes.isValid(node->parent)) {
            Node* oldParent = fnodes.get(node->parent);
            if (oldParent) {
                auto& s = oldParent->children;
                s.erase(std::remove(s.begin(), s.end(), nodeHandle), s.end());
            }
        }

        // attach to new parent
        node->parent = newParent;
        Node* newP = fnodes.get(newParent);
        if (newP) newP->children.push_back(nodeHandle);

        return true;
    }

    // ---------- Safe recursive remove ----------
    bool removeFNode(TinyHandle handle, bool recursive = true) {
        Node* node = fnodes.get(handle);
        if (!node || !node->deletable()) return false;

        // Public interface - use the node's parent as the rescue parent
        TinyHandle rescueParent = node->parent;
        if (!fnodes.isValid(rescueParent)) {
            rescueParent = rootHandle_;
        }

        return removeFNodeRecursive(handle, rescueParent, recursive);
    }

    bool flattenFNode(TinyHandle handle) {
        return removeFNode(handle, false);
    }

    // ---------- Path resolution ----------
    std::string getFullPath(TinyHandle handle) const {
        if (!fnodes.isValid(handle)) return std::string();

        std::vector<std::string> parts;
        TinyHandle cur = handle;
        while (fnodes.isValid(cur)) {
            const Node* n = fnodes.get(cur);
            if (!n) break;
            parts.push_back(n->name);
            if (cur == rootHandle_) break;
            cur = n->parent;
        }
        std::reverse(parts.begin(), parts.end());
        std::ostringstream oss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) oss << '/';
            oss << parts[i];
        }
        return oss.str();
    }

    // ---------- Data retrieval ----------
    template<typename T>
    T* getFileData(TinyHandle fileHandle) {
        Node* node = fnodes.get(fileHandle);
        if (!node || !node->hasData()) return nullptr;

        return registry.get<T>(node->tHandle);
    }

    TypeHandle getTHandle(TinyHandle handle) const {
        const Node* node = fnodes.get(handle);
        return node ? node->tHandle : TypeHandle();
    }

    // Access to file system nodes (no non-const to prevent mutation)
    const TinyPool<Node>& getFNodes() const { return fnodes; }

    template<typename T>
    void setExt(const std::string& ext) {
        typeHashToExtension[typeid(T).hash_code()] = ext;
    }

    template<typename T>
    std::string getExt() const {
        auto it = typeHashToExtension.find(typeid(T).hash_code());
        return (it != typeHashToExtension.end()) ? it->second : "afile";
    }

    std::string getFileExt(TinyHandle fileHandle) const {
        const Node* node = fnodes.get(fileHandle);
        if (!node || !node->isFile()) return std::string();

        return typeHashToExtension.count(node->tHandle.typeHash) ?
            typeHashToExtension.at(node->tHandle.typeHash) : std::string();
    }

private:
    TinyPool<Node> fnodes;
    TinyRegistry registry;
    TinyHandle rootHandle_{};
    TinyHandle regHandle_{};
    bool caseSensitive_{false}; // Global case sensitivity setting

    // Type to .extension map
    UnorderedMap<size_t, std::string> typeHashToExtension;

    bool namesEqual(const std::string& a, const std::string& b) const {
        if (caseSensitive_) {
            return a == b;
        } else {
            // Case-insensitive comparison
            if (a.size() != b.size()) return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](char c1, char c2) {
                return std::tolower(c1) == std::tolower(c2);
            });
        }
    }

    bool hasRepeatName(TinyHandle parentHandle, const std::string& name) const {
        if (!fnodes.isValid(parentHandle)) return false;

        const Node* parent = fnodes.get(parentHandle);
        if (!parent) return false;

        for (const TinyHandle& childHandle : parent->children) {
            const Node* child = fnodes.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }

    std::string resolveRepeatName(TinyHandle parentHandle, const std::string& baseName) const {
        if (!fnodes.isValid(parentHandle)) return baseName;

        // Check if parent has children with the same name
        const Node* parent = fnodes.get(parentHandle);
        if (!parent) return baseName;

        std::string resolvedName = baseName;
        size_t maxNumberedIndex = 0;

        for (const TinyHandle& childHandle : parent->children) {
            const Node* child = fnodes.get(childHandle);
            if (!child) continue;

            // Check if name matches baseName exactly
            if (namesEqual(child->name, resolvedName)) {
                maxNumberedIndex += 1;
                resolvedName = baseName + " (" + std::to_string(maxNumberedIndex) + ")";
            }
        }

        return resolvedName;
    }

    // Implementation function: templated but uses if constexpr to allow T=void
    template<typename T>
    TinyHandle addFNodeImpl(TinyHandle parentHandle, const std::string& name, T* data, Node::CFG cfg) {
        if (!fnodes.isValid(parentHandle)) return TinyHandle(); // invalid parent

        Node child;
        child.name = resolveRepeatName(parentHandle, name);
        child.parent = parentHandle;
        child.cfg = cfg;

        if constexpr (!std::is_same_v<T, void>) {
            if (data) {
                child.tHandle = registry.add(*data);
                child.type = Node::Type::File;
            }
        }

        TinyHandle h = fnodes.add(std::move(child));
        // parent might have been invalidated in a multithreaded scenario; guard
        if (fnodes.isValid(parentHandle)) {
            Node* parent = fnodes.get(parentHandle);
            if (parent) parent->addChild(h);
        }
        return h;
    }

    // Internal recursive function that tracks the original parent for non-deletable rescues
    bool removeFNodeRecursive(TinyHandle handle, TinyHandle rescueParent, bool recursive) {
        Node* node = fnodes.get(handle);
        if (!node) return false;

        // Note: We can assume node is deletable since non-deletable nodes are moved, not recursed

        // copy children to avoid mutation during recursion
        std::vector<TinyHandle> childCopy = node->children;
        for (TinyHandle ch : childCopy) {
            Node* child = fnodes.get(ch);
            if (!child) continue;
            
            if (child->deletable() && recursive) {
                // Child is deletable and we're in normal delete mode - remove it recursively
                removeFNodeRecursive(ch, rescueParent, recursive);
            } else {
                // Child is non-deletable OR we're in flatten mode - move it to the rescue parent
                moveFNode(ch, rescueParent);
            }
        }

        // if file / has data, remove registry entry
        if (node->hasData()) {
            registry.remove(node->tHandle);
            node->tHandle = TypeHandle(); // invalidate
        }

        // remove from parent children list
        if (fnodes.isValid(node->parent)) {
            Node* parent = fnodes.get(node->parent);
            if (parent) {
                auto& s = parent->children;
                s.erase(std::remove(s.begin(), s.end(), handle), s.end());
            }
        }

        // finally remove node from pool
        fnodes.remove(handle);
        return true;
    }

};
