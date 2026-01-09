#pragma once

#include "ascReg.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>

/* Virtual File System (FS), Important:

Does NOT have any real file I/O capabilities!

Only exists as abstraction layer to manage hierarchical data storage

*/

namespace Asc {

class FS {
public:
    struct Node {
        std::string             name;
        Handle              parent;
        std::vector<Handle> children;
        Handle              data;

        const char* cname() const noexcept { return name.c_str(); }

        [[nodiscard]] bool isFile() const noexcept   { return data.valid(); }
        [[nodiscard]] bool isFolder() const noexcept { return !data.valid(); }

        [[nodiscard]] Type::ID typeID() const noexcept { return data.tID(); }

        int whereChild(Handle childHandle) const noexcept {
            for (size_t i = 0; i < children.size(); ++i) {
                if (children[i] == childHandle) return static_cast<int>(i);
            }
            return -1;
        }

        int addChild(Handle childHandle) {
            if (whereChild(childHandle) != -1) return -1;

            children.push_back(childHandle);
            return static_cast<int>(children.size() - 1);
        }

        void eraseChild(Handle childHandle) {
            children.erase(
                std::remove(children.begin(), children.end(), childHandle),
                children.end()
            );
        }
    };

    struct TypeInfo {
        std::string ext;
        uint8_t     color[3] = {255, 255, 255};
        uint8_t     rmOrder = 0;    // Lower = erased first

        std::function<void(Handle fileHandle, FS& fs, void* userData)> onCreate;
        std::function<void(Handle fileHandle, FS& fs, void* userData)> onReload;
        std::function<bool(Handle fileHandle, FS& fs, void* userData)> onDelete; // only delete file if returns true

        [[nodiscard]] const char* c_str() const noexcept { return ext.c_str(); }
    };

    FS() noexcept {
        Node root;
        root.name = "root";
        root.parent = {};
        root.data = {};
        rootHandle_ = fnodes_.emplace(std::move(root));

        // Ensure void type info exists
        typeInfo(Type::TypeID<void>());
    }

    ~FS() {
        std::vector<Type::ID> typeOrder;
        for (const auto& [tID, tInfo] : typeInfo_) {
            typeOrder.push_back(tID);
        }
        std::sort(typeOrder.begin(), typeOrder.end(), [&](Type::ID a, Type::ID b) {
            return typeInfo_[a].rmOrder < typeInfo_[b].rmOrder;
        });

        for (Type::ID tID : typeOrder) {
            registry_.clear(tID);
        }
    }

    [[nodiscard]] Handle rootHandle() const noexcept { return rootHandle_; }

// ------------------------------- Node creation -------------------------------

    Handle createFolder(std::string name, Handle parent = {}) {
        if (!parent) parent = rootHandle_;
        Node* p = node(parent);
        if (!p || p->isFile()) return {};

        name = resolveUniqueName(parent, std::move(name));

        Node folder;
        folder.name = std::move(name);
        folder.parent = parent;
        folder.data = {};

        Handle h = fnodes_.emplace(std::move(folder));

        p = node(parent); // To avoid invalidation
        p->children.push_back(h);
        return h;
    }

    // Create file with data
    template<typename T>
    Handle createFile(std::string name, T&& data, Handle parent = {}, void* userData = nullptr) {
        if (!parent) parent = rootHandle_;
        Node* p = node(parent);
        if (!p || p->isFile()) return {};

        name = resolveUniqueName(parent, std::move(name));

        Handle dataHandle = registry_.emplace<T>(std::forward<T>(data));

        Node file;
        file.name = std::move(name);
        file.parent = parent;
        file.data = dataHandle;

        Handle h = fnodes_.emplace(std::move(file));
        TypeInfo* tInfo = typeInfo(dataHandle.tID()); // Ensure TypeInfo exists
        if (tInfo && tInfo->onCreate) {
            tInfo->onCreate(h, *this, userData);
        }

        p = node(parent);
        p->children.push_back(h);

        rDataToFile_[dataHandle] = h;

        updatePathCache(h);

        return h;
    }

// ------------------------------- File/folder operations -------------------------------

    bool move(Handle nodeHandle, Handle newParentHandle) {
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

    void rename(Handle nodeHandle, std::string newName) {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        newName = resolveUniqueName(node->parent, std::move(newName), nodeHandle);
        node->name = std::move(newName);

        updatePathCacheRecursive(nodeHandle);
    }

    // Return a queue of nodes in a depth-first manner
    std::vector<Handle> fQueue(Handle nodeHandle) {
        std::vector<Handle> queue;
        std::function<void(Handle)> addToQueue = [&](Handle h) {
            Node* node = fnodes_.get(h);
            if (!node) return;

            queue.push_back(h);

            for (Handle child : node->children) {
                addToQueue(child);
            }
        };
        addToQueue(nodeHandle);

        return queue;
    }

    void rm(Handle nodeHandle, void* userData = nullptr) {
        if (!nodeHandle) return;

        const Node* targetNode = fnodes_.get(nodeHandle);
        if (!targetNode) return;

        std::vector<Handle> rmQueue = fQueue(nodeHandle);

        // Sort by removal order
        std::sort(rmQueue.begin(), rmQueue.end(), [&](Handle a, Handle b) {
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
        // since all nodes are being erased anyway
        for (Handle h : rmQueue) { 
            Node* node = fnodes_.get(h); // Queue ensures validity

            Handle dataHandle = node->data;

            TypeInfo* tInfo = typeInfo(dataHandle.tID());
            if (tInfo && tInfo->onDelete) {
                if (!tInfo->onDelete(h, *this, userData)) {
                    continue; // Skip deletion
                }
            }
            
            r().erase(dataHandle);

            fnodes_.erase(h);
        }
    }

    // erase this node only
    void rmRaw(Handle nodeHandle, void* userData = nullptr) {
        if (!nodeHandle) return;

        const Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        Handle rescueHandle = node->parent;
        Node* parentNode = fnodes_.get(rescueHandle);
        if (!parentNode) return; // Should not happen

        // Rebranch all children to rescue parent
        for (Handle childHandle : node->children) {
            if (Node* childNode = fnodes_.get(childHandle)) {
                childNode->parent = rescueHandle;
                parentNode->children.push_back(childHandle);
            }
        }

        // erase the node
        parentNode->eraseChild(nodeHandle);

        TypeInfo* tInfo = typeInfo(node->data.tID());
        if (tInfo && tInfo->onDelete) {
            if (!tInfo->onDelete(nodeHandle, *this, userData)) {
                return; // Skip deletion
            }
        }

        r().erase(node->data);
        fnodes_.erase(nodeHandle);
    }

    void reload(Handle fileHandle, void* userData = nullptr) {
        if (!fileHandle) return;

        const Node* node = fnodes_.get(fileHandle);
        if (!node || !node->isFile()) return;

        TypeInfo* tInfo = typeInfo(node->data.tID());
        if (tInfo && tInfo->onReload) {
            tInfo->onReload(fileHandle, *this, userData);
        }
    }

// ------------------------------- File/folder info -------------------------------

    const std::string& name(Handle nodeHandle) const noexcept {
        const Node* node = fnodes_.get(nodeHandle);
        static const std::string empty;
        return node ? node->name : empty;
    }

    const char* nameCStr(Handle nodeHandle) const noexcept {
        const Node* node = fnodes_.get(nodeHandle);
        return node ? node->name.c_str() : nullptr;
    }

    template<typename T>
    [[nodiscard]] T* data(Handle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? registry_.get<T>(node->data) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* data(Handle fileHandle) const noexcept {
        return const_cast<FS*>(this)->data<T>(fileHandle);
    }

    [[nodiscard]] Handle dataHandle(Handle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node && node->isFile() ? node->data : Handle();
    }

    [[nodiscard]] Type::ID typeID(Handle fileHandle) const noexcept {
        if (const Node* node = fnodes_.get(fileHandle)) return node->typeID();
        return 0;
    }

    [[nodiscard]] TypeInfo* typeInfo(Handle fileHandle) noexcept {
        return typeInfo(typeID(fileHandle));
    }

    [[nodiscard]] const TypeInfo* typeInfo(Handle fileHandle) const noexcept {
        return typeInfo(typeID(fileHandle));
    }

    [[nodiscard]] const char* path(Handle handle, const char* rootAlias = nullptr) const noexcept {
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

    TypeInfo* typeInfo(Type::ID typeID) noexcept {
        // Lazily create if not exists
        auto [it, inserted] = typeInfo_.try_emplace(typeID);
        return &it->second;
    }

    template<typename T>
    TypeInfo* typeInfo() noexcept {
        return typeInfo(Type::TypeID<T>());
    }

    const TypeInfo* typeInfo(Type::ID typeID) const noexcept {
        auto it = typeInfo_.find(typeID);
        return it != typeInfo_.end() ? &it->second : nullptr;
    }

    template<typename T>
    const TypeInfo* typeInfo() const noexcept {
        return typeInfo(Type::TypeID<T>());
    }

// ------------------------------- Some accessors -------------------------------

    // Restricted access to fnodes
    [[nodiscard]] const Node* fNode(Handle fh) const noexcept { return fnodes_.get(fh); }
    [[nodiscard]] const Pool<Node>& fNodes() const noexcept { return fnodes_; }

    [[nodiscard]] TINY_FORCE_INLINE Reg& r() noexcept { return registry_; }
    [[nodiscard]] TINY_FORCE_INLINE const Reg& r() const noexcept { return registry_; }
    
    template<typename T> [[nodiscard]] TINY_FORCE_INLINE T* rGet(Handle h) noexcept { return registry_.get<T>(h); }
    template<typename T> [[nodiscard]] TINY_FORCE_INLINE const T* rGet(Handle h) const noexcept { return registry_.get<T>(h); }

    // Backward
    Handle rDataToFile(Handle rh) const noexcept {
        auto it = rDataToFile_.find(rh);
        return it != rDataToFile_.end() ? it->second : Handle();
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
    Pool<Node> fnodes_;
    Reg   registry_;
    Handle     rootHandle_;

    TINY_FORCE_INLINE Node* node(Handle nodeHandle) noexcept { return fnodes_.get(nodeHandle); }

    std::unordered_map<Handle, std::vector<Handle>> pathCache_;
    std::unordered_map<Handle, Handle> rDataToFile_;
    std::unordered_map<Type::ID, TypeInfo> typeInfo_;

    std::string resolveUniqueName(Handle parent, std::string name, Handle exclude = {}) const {
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

    bool hasChildWithName(const Node* parent, const std::string& name, Handle exclude) const {
        for (Handle h : parent->children) {
            if (h == exclude) continue;
            const Node* child = fnodes_.get(h);
            if (child && child->name == name) return true;
        }
        return false;
    }

    void updatePathCache(Handle h) {
        std::vector<Handle> path;
        Handle cur = h;
        while (cur) {
            path.push_back(cur);
            const Node* n = fnodes_.get(cur);
            if (!n) break;
            cur = n->parent;
        }
        std::reverse(path.begin(), path.end());
        pathCache_[h] = std::move(path);
    }

    void updatePathCacheRecursive(Handle h) {
        updatePathCache(h);
        Node* node = fnodes_.get(h);
        if (node) {
            for (Handle child : node->children) {
                updatePathCacheRecursive(child);
            }
        }
    }

    bool isDescendant(Handle ancestor, Handle descendant) const {
        Handle cur = fnodes_.get(descendant)->parent;
        while (cur) {
            if (cur == ancestor) return true;
            cur = fnodes_.get(cur)->parent;
        }
        return false;
    }
};

using NodeFS = FS::Node;

}