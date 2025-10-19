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

        template<typename T>
        bool isType() const { return tHandle.isType<T>(); }
        bool isFile() const { return type == Type::File; }
        bool isFolder() const { return type == Type::Folder; }

        bool hasData() const { return tHandle.valid(); }

        bool hasChild(TinyHandle childHandle) const {
            // Check if childHandle exists in children vector
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }

        int childIndex(TinyHandle childHandle) const {
            // Return the index of the child if found, -1 otherwise
            auto it = std::find(children.begin(), children.end(), childHandle);
            return (it != children.end()) ? static_cast<int>(std::distance(children.begin(), it)) : -1;
        }

        int addChild(TinyHandle childHandle) {
            if (hasChild(childHandle)) return -1;
            children.push_back(childHandle);
            return static_cast<int>(children.size()) - 1;
        }

        bool removeChild(TinyHandle childHandle) {
            int index = childIndex(childHandle);
            if (index != -1) {
                children.erase(children.begin() + index);
                return true;
            }
            return false;
        }
    };

    struct TypeExt {
        std::string ext;
        uint8_t priority;
        float color[3];

        // Assume folder, max priority
        TypeExt(const std::string& ext = "", uint8_t priority = UINT8_MAX, float r = 1.0f, float g = 1.0f, float b = 1.0f)
            : ext(ext), priority(priority) {
            color[0] = r;
            color[1] = g;
            color[2] = b;
        }

        // Helpful comparison operator for sorting
        // Order: priority desc, then ext asc
        bool operator<(const TypeExt& other) const {
            if (priority != other.priority) {
                return priority > other.priority; // Higher priority first
            }
            return ext < other.ext; // Lexicographical order
        }

        bool operator>(const TypeExt& other) const {
            return other < *this;
        }

        // Compare for equality
        bool operator==(const TypeExt& other) const {
            return 
                ext == other.ext &&
                priority == other.priority &&
                color[0] == other.color[0] &&
                color[1] == other.color[1] &&
                color[2] == other.color[2];
        }
    };


    TinyFS() {
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

    // Case sensitivity control
    bool caseSensitive() const { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) { caseSensitive_ = caseSensitive; }

    // Expose registry for access
    const TinyRegistry& registryRef() const { return registry; }
    TinyRegistry& registryRef() { return registry; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) {
        Node* root = fnodes.get(rootHandle_);
        if (root) root->name = rootPath;
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
        if (!node) return false;

        // resolve name conflicts
        node->name = resolveRepeatName(newParent, node->name);

        // remove from old parent children vector
        if (fnodes.isValid(node->parent)) {
            Node* oldParent = fnodes.get(node->parent);
            if (oldParent) oldParent->removeChild(nodeHandle);
        }

        Node* newP = fnodes.get(newParent);
        if (newP) newP->addChild(nodeHandle);

        // attach to new parent
        node->parent = newParent;

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
    void setTypeExt(const std::string& ext, uint8_t priority = 0, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
        TypeExt typeExt;
        typeExt.ext = ext;
        typeExt.priority = priority;
        typeExt.color[0] = r;
        typeExt.color[1] = g;
        typeExt.color[2] = b;
        typeHashToExt[typeid(T).hash_code()] = typeExt;
    }

    // Get the full TypeExt info for a type
    template<typename T>
    TypeExt getTypeExt() const {
        auto it = typeHashToExt.find(typeid(T).hash_code());
        return (it != typeHashToExt.end()) ? it->second : TypeExt();
    }

    // Get the full TypeExt info for a file
    TypeExt getFileTypeExt(TinyHandle fileHandle) const {
        const Node* node = fnodes.get(fileHandle);

        // Invalid, minimum priority
        if (!node) return TypeExt("", 0);

        // Folder type, max priority
        if (node->isFolder()) return TypeExt("", UINT8_MAX);

        return typeHashToExt.count(node->tHandle.typeHash) ?
            typeHashToExt.at(node->tHandle.typeHash) : TypeExt();
    }

private:
    TinyPool<Node> fnodes;
    TinyRegistry registry;
    TinyHandle rootHandle_{};
    bool caseSensitive_{false}; // Global case sensitivity setting

    // Type to extension info map (using new TypeExt structure)
    UnorderedMap<size_t, TypeExt> typeHashToExt;

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
