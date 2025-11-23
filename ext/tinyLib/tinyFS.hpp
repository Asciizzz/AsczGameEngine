#pragma once

#include "tinyRegistry.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>

class tinyFS {
public:
    struct Node {
        std::string             name;
        tinyHandle              parent;
        std::vector<tinyHandle> children;
        tinyHandle              data;

        [[nodiscard]] bool isFile() const noexcept   { return data.valid(); }
        [[nodiscard]] bool isFolder() const noexcept { return !data.valid(); }

        [[nodiscard]] tinyType::ID typeID() const noexcept { return data.tID(); }

        [[nodiscard]] tinyHandle findChild(const std::string& childName) const noexcept {
            for (size_t i = 0; i < children.size(); ++i) {
                const Node* child = fs_->fnodes_.get(children[i]);
                if (child && child->name == childName) {
                    return children[i];
                }
            }
            return {};
        }

        int addChild(tinyHandle childHandle) {
            if (findChild(fs_->fnodes_.get(childHandle)->name)) {
                return -1;
            }
            children.push_back(childHandle);
            return static_cast<int>(children.size() - 1);
        }

        void eraseChild(tinyHandle childHandle) {
            children.erase(
                std::remove(children.begin(), children.end(), childHandle),
                children.end()
            );
        }

    private:
        friend class tinyFS;
        tinyFS* fs_ = nullptr;
    };

    struct TypeInfo {
        std::string ext;
        uint8_t     color[3] = {255, 255, 255};
        uint8_t     rmOrder = 0;    // Lower = removed first

        [[nodiscard]] const char* c_str() const noexcept { return ext.c_str(); }
    };

    tinyFS() noexcept {
        Node root;
        root.name = "root";
        root.parent = {};
        root.data = {};
        root.fs_ = this;
        rootHandle_ = fnodes_.emplace(std::move(root));

        // Ensure void type info exists
        typeInfo(tinyType::TypeID<void>());
    }

    ~tinyFS() {
        std::vector<tinyType::ID> typeOrder;
        for (const auto& [tID, tInfo] : typeInfo_) {
            typeOrder.push_back(tID);
        }
        std::sort(typeOrder.begin(), typeOrder.end(), [&](tinyType::ID a, tinyType::ID b) {
            return typeInfo_[a].rmOrder < typeInfo_[b].rmOrder;
        });

        for (tinyType::ID tID : typeOrder) {
            registry_.clear(tID);
        }
    }

    [[nodiscard]] tinyHandle root() const noexcept { return rootHandle_; }

// ------------------------------- Node creation -------------------------------

    tinyHandle createFolder(std::string name, tinyHandle parent = {}) {
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

    // Create file with data
    template<typename T>
    tinyHandle createFile(std::string name, T&& data, tinyHandle parent = {}) {
        if (!parent) parent = rootHandle_;
        Node* p = fnodes_.get(parent);
        if (!p || p->isFile()) return {};

        name = resolveUniqueName(parent, std::move(name));

        tinyHandle dataHandle = registry_.emplace<T>(std::forward<T>(data));
        typeInfo(dataHandle.tID()); // Ensure TypeInfo exists

        Node file;
        file.name = std::move(name);
        file.parent = parent;
        file.data = dataHandle;
        file.fs_ = this;

        tinyHandle h = fnodes_.emplace(std::move(file));
        p->children.push_back(h);

        rDataToFile_[dataHandle] = h;

        updatePathCache(h);

        return h;
    }

// ------------------------------- File/folder operations -------------------------------

    bool move(tinyHandle nodeHandle, tinyHandle newParentHandle) {
        Node* node = fnodes_.get(nodeHandle);
        Node* newParent = fnodes_.get(newParentHandle);
        if (!node || !newParent || newParent->isFile()) return false;
        if (nodeHandle == newParentHandle) return false;
        if (isDescendant(nodeHandle, newParentHandle)) return false;

        if (node->parent) {
            Node* oldParent = fnodes_.get(node->parent);
            if (oldParent) oldParent->eraseChild(nodeHandle);
        }

        node->parent = newParentHandle;
        newParent->children.push_back(nodeHandle);

        updatePathCacheRecursive(nodeHandle);

        return true;
    }

    void rename(tinyHandle nodeHandle, std::string newName) {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        newName = resolveUniqueName(node->parent, std::move(newName), nodeHandle);
        node->name = std::move(newName);

        updatePathCacheRecursive(nodeHandle);
    }

    // Return a queue of nodes in a depth-first manner
    std::vector<tinyHandle> fQueue(tinyHandle nodeHandle) {
        std::vector<tinyHandle> queue;
        std::function<void(tinyHandle)> addToQueue = [&](tinyHandle h) {
            Node* node = fnodes_.get(h);
            if (!node) return;

            for (tinyHandle child : node->children) {
                addToQueue(child);
            }

            queue.push_back(h);
        };
        addToQueue(nodeHandle);

        return queue;
    }

    void rm(tinyHandle nodeHandle) {
        if (!nodeHandle) return;

        const Node* targetNode = fnodes_.get(nodeHandle);
        if (!targetNode) return;

        std::vector<tinyHandle> rmQueue = fQueue(nodeHandle);

        // Sort by removal order
        std::sort(rmQueue.begin(), rmQueue.end(), [&](tinyHandle a, tinyHandle b) {
            const Node* nodeA = fnodes_.get(a);
            const Node* nodeB = fnodes_.get(b);
            if (!nodeA || !nodeB) return false;

            const TypeInfo* tInfoA = typeInfo(nodeA->data.tID());
            const TypeInfo* tInfoB = typeInfo(nodeB->data.tID());
            uint8_t orderA = tInfoA ? tInfoA->rmOrder : 255;
            uint8_t orderB = tInfoB ? tInfoB->rmOrder : 255;

            return orderA < orderB;
        });

        if (targetNode->parent) {
            Node* parentNode = fnodes_.get(targetNode->parent);
            if (parentNode) parentNode->eraseChild(nodeHandle);
        }

        // No need for child-parent updates
        // since all nodes are being removed anyway
        for (tinyHandle h : rmQueue) { 
            Node* node = fnodes_.get(h); // Queue ensures validity
            if (node->isFile()) r().remove(node->data);

            fnodes_.remove(h);
        }
    }

    // Remove this node only
    void rmRaw(tinyHandle nodeHandle) {
        if (!nodeHandle) return;

        const Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        tinyHandle rescueHandle = node->parent;
        Node* parentNode = fnodes_.get(rescueHandle);
        if (!parentNode) return; // Should not happen

        // Rebranch all children to rescue parent
        for (tinyHandle childHandle : node->children) {
            if (Node* childNode = fnodes_.get(childHandle)) {
                childNode->parent = rescueHandle;
                parentNode->children.push_back(childHandle);
            }
        }

        // Remove the node
        parentNode->eraseChild(nodeHandle);

        r().remove(node->data);
        fnodes_.remove(nodeHandle);
    }

// ------------------------------- File/folder info -------------------------------

    const std::string& name(tinyHandle nodeHandle) const noexcept {
        const Node* node = fnodes_.get(nodeHandle);
        static const std::string empty;
        return node ? node->name : empty;
    }

    const char* nameCStr(tinyHandle nodeHandle) const noexcept {
        const Node* node = fnodes_.get(nodeHandle);
        return node ? node->name.c_str() : nullptr;
    }

    template<typename T>
    [[nodiscard]] T* data(tinyHandle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? registry_.get<T>(node->data) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* data(tinyHandle fileHandle) const noexcept {
        return const_cast<tinyFS*>(this)->data<T>(fileHandle);
    }

    [[nodiscard]] tinyHandle dataHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? node->data : tinyHandle();
    }

    [[nodiscard]] tinyType::ID typeID(tinyHandle fileHandle) const noexcept {
        if (const Node* node = fnodes_.get(fileHandle)) return node->typeID();
        return 0;
    }

    [[nodiscard]] TypeInfo* typeInfo(tinyHandle fileHandle) noexcept {
        return typeInfo(typeID(fileHandle));
    }

    [[nodiscard]] const TypeInfo* typeInfo(tinyHandle fileHandle) const noexcept {
        return typeInfo(typeID(fileHandle));
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

// ------------------------------- Type info -------------------------------

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

// ------------------------------- Some accessors -------------------------------

    // Restricted access to fnodes
    [[nodiscard]] const Node* fNode(tinyHandle fh) const noexcept { return fnodes_.get(fh); }
    [[nodiscard]] const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

    [[nodiscard]] tinyRegistry& r() noexcept { return registry_; }
    [[nodiscard]] const tinyRegistry& r() const noexcept { return registry_; }

    // Backward
    tinyHandle rDataToFile(tinyHandle rh) const noexcept {
        auto it = rDataToFile_.find(rh);
        return it != rDataToFile_.end() ? it->second : tinyHandle();
    }

// --------------------------- Cool static utilities ----------------------------

    static std::string pName(const std::string& filepath, bool withExt = true) noexcept {
        size_t pos = filepath.find_last_of("/\\");
        std::string filename = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
        
        if (!withExt) {
            size_t dotPos = filename.find_last_of('.');
            if (dotPos != std::string::npos) {
                filename = filename.substr(0, dotPos);
            }
        }
        
        return filename;
    }

    static std::string pExt(const std::string& filename) noexcept {
        size_t pos = filename.find_last_of('.');

        if (pos == std::string::npos || pos == filename.size() - 1) return "";
        return filename.substr(pos + 1);
    }

private:
    tinyPool<Node> fnodes_;
    tinyRegistry   registry_;
    tinyHandle     rootHandle_;

    std::unordered_map<tinyHandle, std::vector<tinyHandle>> pathCache_;
    std::unordered_map<tinyHandle, tinyHandle> rDataToFile_;
    std::unordered_map<tinyType::ID, TypeInfo> typeInfo_;

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