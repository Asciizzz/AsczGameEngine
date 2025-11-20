#pragma once

#include "tinyRegistry.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class tinyFS {
public:
    struct Node {
        std::string             name;       // File/folder name
        tinyHandle              parent;     // Parent node handle
        std::vector<tinyHandle> children;   // Ordered children
        tinyHandle              data;       // Handle to actual data in registry (invalid = folder)

        [[nodiscard]] bool isFile() const noexcept   { return data.valid(); }
        [[nodiscard]] bool isFolder() const noexcept { return !data.valid(); }

        [[nodiscard]] tinyHandle findChild(const std::string& childName) const noexcept {
            for (size_t i = 0; i < children.size(); ++i) {
                const Node* child = fs_->fnodes_.get(children[i]);
                if (child && child->name == childName) {
                    return children[i];
                }
            }
            return {};
        }

    private:
        friend class tinyFS;
        tinyFS* fs_ = nullptr;  // Back-ref for child lookup (set on insert)
    };

    struct TypeInfo {
        std::string ext;           // e.g. "png", "obj", "wav"
        uint8_t     color[3] = {255, 255, 255};  // RGB for editor icons

        [[nodiscard]] const char* c_str() const noexcept { return ext.c_str(); }
    };

    tinyFS() noexcept {
        Node root;
        root.name = "root";
        root.parent = {};
        root.data = {};
        root.fs_ = this;
        rootHandle_ = fnodes_.emplace(std::move(root));
    }

    [[nodiscard]] tinyHandle root() const noexcept { return rootHandle_; }

    // Create folder
    tinyHandle createFolder(tinyHandle parent, std::string name) {
        if (!parent) parent = rootHandle_;
        Node* p = fnodes_.get(parent);
        if (!p || p->isFile()) return {};

        name = resolveUniqueName(parent, std::move(name));

        Node folder;
        folder.name = std::move(name);
        folder.parent = parent;
        folder.data = {};
        folder.fs_ = this;

        tinyHandle h = fnodes_.emplace(std::move(folder));
        p->children.push_back(h);
        return h;
    }

    // Overload: Create folder at root
    tinyHandle createFolder(std::string name) {
        return createFolder(rootHandle_, std::move(name));
    }

    // Create file with data
    template<typename T>
    tinyHandle createFile(tinyHandle parent, std::string name, T&& data) {
        if (!parent) parent = rootHandle_;
        Node* p = fnodes_.get(parent);
        if (!p || p->isFile()) return {};

        name = resolveUniqueName(parent, std::move(name));

        // Store data in registry
        tinyHandle dataHandle = registry_.emplace<T>(std::forward<T>(data));

        Node file;
        file.name = std::move(name);
        file.parent = parent;
        file.data = dataHandle;
        file.fs_ = this;

        tinyHandle h = fnodes_.emplace(std::move(file));
        p->children.push_back(h);

        // Update path cache
        updatePathCache(h);

        return h;
    }
    // Overload: Create file at root
    template<typename T>
    tinyHandle createFile(std::string name, T&& data) {
        return createFile(rootHandle_, std::move(name), std::forward<T>(data));
    }

    // Remove node + all children + data
    void remove(tinyHandle nodeHandle) {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        // Recursively remove children
        for (tinyHandle child : node->children) {
            remove(child);
        }

        // Remove data if file
        if (node->isFile()) {
            registry_.remove(node->data);
            pathCache_.erase(nodeHandle);
        }

        // Remove from parent
        if (node->parent) {
            Node* parent = fnodes_.get(node->parent);
            if (parent) {
                parent->children.erase(
                    std::remove(parent->children.begin(), parent->children.end(), nodeHandle),
                    parent->children.end()
                );
            }
        }

        // Finally remove node
        fnodes_.remove(nodeHandle);
    }

    // Move (with cycle protection)
    bool move(tinyHandle nodeHandle, tinyHandle newParentHandle) {
        Node* node = fnodes_.get(nodeHandle);
        Node* newParent = fnodes_.get(newParentHandle);
        if (!node || !newParent || newParent->isFile()) return false;
        if (nodeHandle == newParentHandle) return false;
        if (isDescendant(nodeHandle, newParentHandle)) return false;

        // Remove from old parent
        if (node->parent) {
            Node* oldParent = fnodes_.get(node->parent);
            if (oldParent) {
                oldParent->children.erase(
                    std::remove(oldParent->children.begin(), oldParent->children.end(), nodeHandle),
                    oldParent->children.end()
                );
            }
        }

        // Add to new parent
        node->parent = newParentHandle;
        newParent->children.push_back(nodeHandle);

        // Update path cache
        updatePathCacheRecursive(nodeHandle);

        return true;
    }

    // Rename
    void rename(tinyHandle nodeHandle, std::string newName) {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        newName = resolveUniqueName(node->parent, std::move(newName), nodeHandle);
        node->name = std::move(newName);

        updatePathCacheRecursive(nodeHandle);
    }

    template<typename T>
    [[nodiscard]] T* fRData(tinyHandle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? registry_.get<T>(node->data) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* fRData(tinyHandle fileHandle) const noexcept {
        return const_cast<tinyFS*>(this)->data<T>(fileHandle);
    }

    [[nodiscard]] tinyHandle fRHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? node->data : tinyHandle();
    }

    [[nodiscard]] tinyType::ID fRTypeID(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node || !node->isFile()) return 0;

        tinyHandle dataHandle = node->data;
        return dataHandle.tID();
    }

    [[nodiscard]] const char* path(tinyHandle handle, const char* rootAlias = nullptr) const noexcept {
        auto it = pathCache_.find(handle);
        if (it == pathCache_.end()) return nullptr;

        thread_local std::string result;
        result.clear();

        for (size_t i = 0; i < it->second.size(); ++i) {
            const Node* n = fnodes_.get(it->second[i]);
            if (!n) continue;
            if (i == 0 && rootAlias) result += rootAlias;
            else result += n->name;
            if (i + 1 < it->second.size()) result += '/';
        }

        return result.c_str();
    }

    TypeInfo* typeInfo(tinyType::ID typeID) noexcept {
        // Lazily create if not exists
        auto [it, inserted] = typeInfo_.try_emplace(typeID);
        return &it->second;
    }

    template<typename T>
    TypeInfo* typeInfo() noexcept {
        return typeInfo(tinyType::TypeID<T>());
    }

    const TypeInfo* typeInfo(tinyType::ID typeID) const noexcept {
        auto it = typeInfo_.find(typeID);
        return it != typeInfo_.end() ? &it->second : nullptr;
    }

    template<typename T>
    const TypeInfo* typeInfo() const noexcept {
        return typeInfo(tinyType::TypeID<T>());
    }

    [[nodiscard]] const Node* fNode(tinyHandle h) const noexcept { return fnodes_.get(h); }
    [[nodiscard]] Node* fNode(tinyHandle h) noexcept { return fnodes_.get(h); }

    [[nodiscard]] const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

    [[nodiscard]] tinyRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const tinyRegistry& registry() const noexcept { return registry_; }

    // Some cool static functions

    // Name (no extension, no path)
    [[nodiscard]] static const char* pName(const char* path) noexcept {
        const char* slash = strrchr(path, '/');
        const char* start = slash ? slash + 1 : path;
        const char* dot = strrchr(start, '.');
        return dot ? std::string(start, dot - start).c_str() : start;
    }

    // Extension (no dot)
    [[nodiscard]] static const char* pExt(const char* path) noexcept {
        const char* dot = strrchr(path, '.');
        return dot ? dot + 1 : nullptr;
    }

    [[nodiscard]] static std::string pDir(const char* path) noexcept {
        const char* slash = strrchr(path, '/');
        if (!slash) return ".";
        return std::string(path, slash - path);
    }

private:
    tinyPool<Node>                                 fnodes_;
    tinyRegistry                                   registry_;
    tinyHandle                                     rootHandle_;

    // Full path cache: file node → vector of ancestors (root first)
    std::unordered_map<tinyHandle, std::vector<tinyHandle>> pathCache_;

    // Type → extension + color
    std::unordered_map<tinyType::ID, TypeInfo>     typeInfo_;

    // Resolve name conflicts: "file", "file (1)", "file (2)", etc.
    std::string resolveUniqueName(tinyHandle parent, std::string name, tinyHandle exclude = {}) const {
        const Node* p = fnodes_.get(parent);
        if (!p) return name;

        if (!hasChildWithName(p, name, exclude)) return name;

        int index = 1;
        std::string base = name;
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);

        std::string candidate;
        do {
            candidate = base + " (" + std::to_string(++index) + ")";
            if (dot != std::string::npos) {
                candidate += name.substr(dot);
            }
        } while (hasChildWithName(p, candidate, exclude));

        return candidate;
    }

    bool hasChildWithName(const Node* parent, const std::string& name, tinyHandle exclude) const {
        for (tinyHandle h : parent->children) {
            if (h == exclude) continue;
            const Node* child = fnodes_.get(h);
            if (child && child->name == name) return true;
        }
        return false;
    }

    void updatePathCache(tinyHandle h) {
        std::vector<tinyHandle> path;
        tinyHandle cur = h;
        while (cur) {
            path.push_back(cur);
            const Node* n = fnodes_.get(cur);
            if (!n) break;
            cur = n->parent;
        }
        std::reverse(path.begin(), path.end());
        pathCache_[h] = std::move(path);
    }

    void updatePathCacheRecursive(tinyHandle h) {
        updatePathCache(h);
        Node* node = fnodes_.get(h);
        if (node) {
            for (tinyHandle child : node->children) {
                updatePathCacheRecursive(child);
            }
        }
    }

    bool isDescendant(tinyHandle ancestor, tinyHandle descendant) const {
        tinyHandle cur = fnodes_.get(descendant)->parent;
        while (cur) {
            if (cur == ancestor) return true;
            cur = fnodes_.get(cur)->parent;
        }
        return false;
    }
};

using tinyNodeFS = tinyFS::Node;