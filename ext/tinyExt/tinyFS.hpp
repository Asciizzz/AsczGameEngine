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
        tinyFS* fs_ = nullptr;
    };

    struct TypeInfo {
        std::string ext;
        uint8_t     color[3] = {255, 255, 255};

        // Removal behavior
        bool        deferRm = true;
        uint8_t     rmOrder = 0;    // Lower = removed first

        /* Note: rmOrder is just local removal order between types, so repetition is allowed */

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

// ------------------------------- Node creation -------------------------------

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

        tinyHandle dataHandle = registry_.emplace<T>(std::forward<T>(data));
        typeInfo(dataHandle.tID()); // Ensure TypeInfo exists

        Node file;
        file.name = std::move(name);
        file.parent = parent;
        file.data = dataHandle;
        file.fs_ = this;

        tinyHandle h = fnodes_.emplace(std::move(file));
        p->children.push_back(h);

        dataToFile_[dataHandle] = h;

        updatePathCache(h);

        return h;
    }

    template<typename T>
    tinyHandle createFile(std::string name, T&& data) {
        return createFile(rootHandle_, std::move(name), std::forward<T>(data));
    }

// ------------------------------- Safe removal -------------------------------

    void fRemove(tinyHandle nodeHandle) {
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

        // Sort queue based on removal order
        std::sort(queue.begin(), queue.end(), [this](tinyHandle a, tinyHandle b) {
            const Node* nodeA = fnodes_.get(a);
            const Node* nodeB = fnodes_.get(b);
            if (!nodeA || !nodeB) return false;

            const TypeInfo* tInfoA = typeInfo(nodeA->data.tID());
            const TypeInfo* tInfoB = typeInfo(nodeB->data.tID());
            uint8_t orderA = tInfoA ? tInfoA->rmOrder : 255;
            uint8_t orderB = tInfoB ? tInfoB->rmOrder : 255;

            return orderA < orderB;
        });

        // Execute removals based on sorted order
        for (tinyHandle h : queue) {
            Node* node = fnodes_.get(h);
            if (!node) continue;

            if (node->isFile()) {
                tinyHandle dataHandle = node->data;
                const TypeInfo* tInfo = typeInfo(dataHandle.tID());

                if (tInfo && tInfo->deferRm) deferredRms_.push_back(dataHandle);
                else                         registry_.remove(dataHandle);

                dataToFile_.erase(dataHandle);
                pathCache_.erase(h);
            }

            if (Node* parent = fnodes_.get(node->parent)) {
                parent->children.erase(
                    std::remove(parent->children.begin(), parent->children.end(), h),
                    parent->children.end()
                );
            }

            fnodes_.remove(h);
        }
    }

    // 0 = remove all types
    void execDeferredRms(tinyType::ID typeID = 0) {
        for (auto it = deferredRms_.begin(); it != deferredRms_.end(); ) {
            if (it->tID() == typeID || typeID != 0) {
                registry_.remove(*it);
                it = deferredRms_.erase(it);
            } else {
                ++it;
            }
        }
    }

    template<typename T>
    void execDeferredRms() {
        execDeferredRms(tinyType::TypeID<T>());
    }

    bool hasDeferredRms(tinyType::ID typeID = 0) const noexcept {
        for (const auto& handle : deferredRms_) {
            if (handle.tID() == typeID || typeID == 0) {
                return true;
            }
        }
        return false;
    }

    template<typename T>
    bool hasDeferredRms() const noexcept {
        return hasDeferredRms(tinyType::TypeID<T>());
    }

    // Deferred removal by key

    void setDeferredRmsKey(const char* key, std::vector<tinyType::ID> typeIDs) {
        deferredRmsKeys_[key] = std::unordered_set<tinyType::ID>(typeIDs.begin(), typeIDs.end());
    }
    void clearDeferredRmsKey(const char* key) {
        deferredRmsKeys_.erase(key);
    }

    void execDeferredRms(const char* key) {
        auto it = deferredRmsKeys_.find(key);
        if (it == deferredRmsKeys_.end()) return;

        const auto& typeIDSet = it->second;

        for (auto rit = deferredRms_.rbegin(); rit != deferredRms_.rend(); ) {
            if (typeIDSet.find(rit->tID()) != typeIDSet.end()) {
                registry_.remove(*rit);
                rit = std::vector<tinyHandle>::reverse_iterator(deferredRms_.erase(std::next(rit).base()));
            } else {
                ++rit;
            }
        }
    }

// ------------------------------- Other operations -------------------------------

    bool fMove(tinyHandle nodeHandle, tinyHandle newParentHandle) {
        Node* node = fnodes_.get(nodeHandle);
        Node* newParent = fnodes_.get(newParentHandle);
        if (!node || !newParent || newParent->isFile()) return false;
        if (nodeHandle == newParentHandle) return false;
        if (isDescendant(nodeHandle, newParentHandle)) return false;

        if (node->parent) {
            Node* oldParent = fnodes_.get(node->parent);
            if (oldParent) {
                oldParent->children.erase(
                    std::remove(oldParent->children.begin(), oldParent->children.end(), nodeHandle),
                    oldParent->children.end()
                );
            }
        }

        node->parent = newParentHandle;
        newParent->children.push_back(nodeHandle);

        updatePathCacheRecursive(nodeHandle);

        return true;
    }

    void fRename(tinyHandle nodeHandle, std::string newName) {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        newName = resolveUniqueName(node->parent, std::move(newName), nodeHandle);
        node->name = std::move(newName);

        updatePathCacheRecursive(nodeHandle);
    }

// ------------------------------- File data -------------------------------

    const std::string& fName(tinyHandle nodeHandle) const noexcept {
        const Node* node = fnodes_.get(nodeHandle);
        static const std::string empty;
        return node ? node->name : empty;
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

    [[nodiscard]] tinyHandle fDataHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? node->data : tinyHandle();
    }

    [[nodiscard]] tinyType::ID fRTypeID(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node || !node->isFile()) return 0;

        tinyHandle dataHandle = node->data;
        return dataHandle.tID();
    }

    [[nodiscard]] const char* fPath(tinyHandle handle, const char* rootAlias = nullptr) const noexcept {
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

    tinyHandle dataToFile(tinyHandle dataHandle) const noexcept {
        auto it = dataToFile_.find(dataHandle);
        return it != dataToFile_.end() ? it->second : tinyHandle();
    }

    [[nodiscard]] const Node* fNode(tinyHandle h) const noexcept { return fnodes_.get(h); }
    [[nodiscard]] const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

    [[nodiscard]] tinyRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const tinyRegistry& registry() const noexcept { return registry_; }

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
    std::unordered_map<tinyHandle, tinyHandle> dataToFile_;

    std::unordered_map<tinyType::ID, TypeInfo> typeInfo_;
    std::vector<tinyHandle> deferredRms_;

    std::unordered_map<const char*, std::unordered_set<tinyType::ID>> deferredRmsKeys_;

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