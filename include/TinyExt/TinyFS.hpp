#pragma once

#include "tinyExt/tinyRegistry.hpp"

#include <string>
#include <sstream>

#include <iostream>

class tinyFS {
public:
    struct Node {
        std::string name;                     // segment name (relative)
        tinyHandle parent;                    // parent node handle
        std::vector<tinyHandle> children;     // child node handles
        typeHandle tHandle;                   // metadata / registry handle if file

        enum class Type { Folder, File, Other } type = Type::Folder;

        struct CFG {
            bool hidden = false;
            bool deletable = true;
        } cfg;

        bool hidden() const noexcept { return cfg.hidden; }
        bool deletable() const noexcept { return cfg.deletable; }

        template<typename T>
        bool isType() const noexcept { return tHandle.isType<T>(); }
        bool isFile() const noexcept { return type == Type::File; }
        bool isFolder() const noexcept { return type == Type::Folder; }

        bool hasData() const noexcept { return tHandle.valid(); }

        bool hasChild(tinyHandle childHandle) const noexcept {
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }

        int childIndex(tinyHandle childHandle) const noexcept {
            auto it = std::find(children.begin(), children.end(), childHandle);
            return (it != children.end()) ? static_cast<int>(std::distance(children.begin(), it)) : -1;
        }

        int addChild(tinyHandle childHandle) { // Push back may throw
            if (hasChild(childHandle)) return -1;
            children.push_back(childHandle);
            return static_cast<int>(children.size()) - 1;
        }

        bool removeChild(tinyHandle childHandle) noexcept {
            int index = childIndex(childHandle);
            if (index != -1) {
                children.erase(children.begin() + index);
                return true;
            }
            return false;
        }
    };

    struct TypeExt {
        // Identification
        std::string ext;
        bool safeDelete; // CRITICAL
        uint8_t priority;

        // Safe Delete explanation:
        // =
        // Some files' data contains Vulkan resources which
        // could potentially be in use (command buffers etc.)
        // thus cannot be safely deleted at arbitrary times.
        // -
        // This flag indicates whether the file can be instantly
        // delete or add to a pending deletion queue for further
        // deletion processing.
        // -
        // For safety, all types default safeDelete = false;

        // Style
        float color[3];

        // Assume folder, max priority
        TypeExt(const std::string& ext = "", bool safeDelete = false, uint8_t priority = UINT8_MAX, float r = 1.0f, float g = 1.0f, float b = 1.0f)
        : ext(ext), priority(priority), safeDelete(safeDelete) {
            color[0] = r;
            color[1] = g;
            color[2] = b;
        }

        // Helpful comparison operator for sorting
        // Order: priority desc, then ext asc
        bool operator<(const TypeExt& other) const noexcept {
            if (priority != other.priority) {
                return priority > other.priority; // Higher priority first
            }
            return ext < other.ext; // Lexicographical order
        }

        bool operator>(const TypeExt& other) const noexcept {
            return other < *this;
        }

        // Compare for equality
        bool operator==(const TypeExt& other) const noexcept {
            return 
                ext == other.ext &&
                priority == other.priority &&
                color[0] == other.color[0] &&
                color[1] == other.color[1] &&
                color[2] == other.color[2];
        }

        // Implicit conversion to string for easy access
        operator std::string() const noexcept { return ext; }

        bool empty() const noexcept { return ext.empty(); }
    };


    tinyFS() noexcept {
        Node rootNode;
        rootNode.name = ".root";
        rootNode.parent = tinyHandle();
        rootNode.type = Node::Type::Folder;
        rootNode.cfg.deletable = false; // root is not deletable

        // Insert root and store explicitly the returned handle
        rootHandle_ = fnodes_.add(std::move(rootNode));
    }

    // ---------- Basic access ----------

    tinyHandle rootHandle() const noexcept { return rootHandle_; }

    // Case sensitivity control
    bool caseSensitive() const noexcept { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) noexcept { caseSensitive_ = caseSensitive; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) noexcept {
        Node* root = fnodes_.get(rootHandle_);
        if (root) root->name = rootPath;
    }

    // ---------- Creation ----------

    // Folder creation (non-template overload)
    tinyHandle addFolder(tinyHandle parentHandle, const std::string& name, Node::CFG cfg = {}) {
        return addFNodeImpl(parentHandle, name, cfg);
    }
    tinyHandle addFolder(const std::string& name, Node::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    tinyHandle addFile(tinyHandle parentHandle, const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFNodeImpl<T>(parentHandle, name, std::forward<T>(data), cfg);
    }

    template<typename T>
    tinyHandle addFile(const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFile<T>(rootHandle_, name, std::forward<T>(data), cfg);
    }

    // ---------- Move with cycle prevention ----------
    
    bool fMove(tinyHandle nodeHandle, tinyHandle parentHandle) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;
        if (nodeHandle == parentHandle) return false; // Move to self

        Node* node = fnodes_.get(nodeHandle);
        if (!node) return false;

        if (node->parent == parentHandle) return true; // already there

        // Avoid parent being a descendant of node
        if (isDescendant(nodeHandle, parentHandle)) return false;

        Node* newParent = fnodes_.get(parentHandle);
        if (newParent) newParent->addChild(nodeHandle);
        else return false;

        // remove from old parent children vector
        Node* oldParent = fnodes_.get(node->parent);
        if (oldParent) oldParent->removeChild(nodeHandle);

        node->name = resolveRepeatName(parentHandle, node->name);
        node->parent = parentHandle;

        return true;
    }

    // ---------- "Safe" recursive remove ----------

    bool fRemove(tinyHandle handle, bool recursive = true) noexcept {
        Node* node = fnodes_.get(handle);
        if (!node || !node->deletable()) return false;

        // Public interface - use the node's parent as the rescue parent
        tinyHandle rescueParent = node->parent;
        if (!fnodes_.valid(rescueParent)) {
            rescueParent = rootHandle_;
        }

        return fRemoveRecursive(handle, rescueParent, recursive);
    }

    bool fFlatten(tinyHandle handle) noexcept {
        return fRemove(handle, false);
    }

    // ---------- Data Inspection ----------

    template<typename T>
    T* fData(tinyHandle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        if (!node || !node->hasData()) return nullptr;

        return registry_.get<T>(node->tHandle);
    }

    template<typename T>
    const T* fData(tinyHandle fileHandle) const noexcept {
        return const_cast<tinyFS*>(this)->fData<T>(fileHandle);
    }

    typeHandle fTypeHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->tHandle : typeHandle();
    }

    template<typename T>
    bool fIsType(tinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->isType<T>() : false;
    }

    // ---------- Node access ----------

    const Node* fNode(tinyHandle fileHandle) const noexcept {
        return fnodes_.get(fileHandle);
    }

    const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

    // ---------- Type extension management ----------

    template<typename T>
    void setTypeExt(const std::string& ext, bool safeDelete = true, uint8_t priority = 0, float r = 1.0f, float g = 1.0f, float b = 1.0f) noexcept {
        TypeExt typeExt;
        typeExt.ext = ext;
        typeExt.safeDelete = safeDelete;
        typeExt.priority = priority;
        typeExt.color[0] = r;
        typeExt.color[1] = g;
        typeExt.color[2] = b;

        typeHashToExt[typeid(T).hash_code()] = typeExt;
    }

    // Get the full TypeExt info for a type
    TypeExt typeExt(size_t typeHash) const noexcept {
        auto it = typeHashToExt.find(typeHash);
        return (it != typeHashToExt.end()) ? it->second : TypeExt();
    }

    template<typename T>
    TypeExt typeExt() const noexcept {
        return typeExt(typeid(T).hash_code());
    }

    // Get the full TypeExt info for a file
    TypeExt fTypeExt(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);

        // Invalid, minimum priority
        if (!node) return TypeExt();

        // Folder type, max priority
        if (node->isFolder()) return TypeExt("", true, UINT8_MAX);

        return typeExt(node->tHandle.typeHash);
    }

    bool fSafeDelete(tinyHandle fileHandle) const noexcept {
        return fTypeExt(fileHandle).safeDelete;
    }

    template<typename T>
    bool safeDelete() const noexcept {
        return typeExt<T>().safeDelete;
    }

    bool safeDelete(size_t typeHash) const noexcept {
        return typeExt(typeHash).safeDelete;
    }

    // ---------- Registry data management ---------
    
    // No non-const access for safety
    const tinyRegistry& registry() const noexcept { return registry_; }

    void* rGet(typeHandle th) noexcept { return registry_.get(th); }
    const void* rGet(typeHandle th) const noexcept { return registry_.get(th); }

    template<typename T>
    T* rGet(typeHandle th) noexcept { return registry_.get<T>(th); }
    template<typename T>
    const T* rGet(typeHandle th) const noexcept { return registry_.get<T>(th); }

    template<typename T>
    T* rGet(tinyHandle h) noexcept { return registry_.get<T>(h); }
    template<typename T>
    const T* rGet(tinyHandle h) const noexcept { return registry_.get<T>(h); }

    template<typename T>
    bool rHas(const tinyHandle& handle) const noexcept { return registry_.has<T>(handle); }
    bool rHas(const typeHandle& th) const noexcept { return registry_.has(th); }

    template<typename T>
    typeHandle rAdd(T&& val) {
        return registry_.add<T>(std::forward<T>(val));
    }

    template<typename T>
    void rRemove(const tinyHandle& handle) noexcept {
        rRemove(typeHandle::make<T>(handle));
    }

    void rRemove(const typeHandle& th) noexcept {
        if (!registry_.has(th)) return; // nothing to remove :/

        if (safeDelete(th.typeHash)) registry_.tInstaRm(th); // Safe to remove instantly
        else                         registry_.tQueueRm(th); // Queue for pending removal
    }

    template<typename T>
    bool rTHasPendingRms() const noexcept {
        return registry_.tHasPendingRms<T>();
    }

    template<typename T>
    void rTFlushRm(uint32_t index) noexcept {
        registry_.tFlushRm<T>(index);
    }

    template<typename T>
    void rTFlushAllRms() noexcept {
        registry_.tFlushAllRms<T>();
    }


    bool rHasPendingRms() const noexcept {
        return registry_.hasPendingRms();
    }

    void rFlushAllRms() noexcept {
        registry_.flushAllRms();
    }

private:
    tinyPool<Node> fnodes_;
    tinyRegistry registry_;
    tinyHandle rootHandle_;
    bool caseSensitive_{false}; // Global case sensitivity setting

    // Type to extension info map (using new TypeExt structure)
    UnorderedMap<size_t, TypeExt> typeHashToExt;

    bool namesEqual(const std::string& a, const std::string& b) const noexcept {
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

    bool hasRepeatName(tinyHandle parentHandle, const std::string& name) const noexcept {
        if (!fnodes_.valid(parentHandle)) return false;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return false;

        for (const tinyHandle& childHandle : parent->children) {
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }

    std::string resolveRepeatName(tinyHandle parentHandle, const std::string& baseName) const noexcept {
        if (!fnodes_.valid(parentHandle)) return baseName;

        // Check if parent has children with the same name
        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return baseName;

        std::string resolvedName = baseName;
        size_t maxNumberedIndex = 0;

        for (const tinyHandle& childHandle : parent->children) {
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            // Check if name matches baseName exactly
            if (namesEqual(child->name, resolvedName)) {
                maxNumberedIndex += 1;
                resolvedName = baseName + " (" + std::to_string(maxNumberedIndex) + ")";
            }
        }

        return resolvedName;
    }

    bool isDescendant(tinyHandle possibleAncestor, tinyHandle possibleDescendant) const noexcept {
        if (!fnodes_.valid(possibleAncestor) || !fnodes_.valid(possibleDescendant))
            return true; // For safety, treat invalid handles as descendants

        const Node* current = fnodes_.get(possibleDescendant);
        while (current && fnodes_.valid(current->parent)) {
            if (current->parent == possibleAncestor)
                return true; // Found ancestor
            current = fnodes_.get(current->parent);
        }
        return false;
    }

    template<typename T>
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, T&& data, Node::CFG cfg) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;

        Node child;
        child.name = resolveRepeatName(parentHandle, name);
        child.parent = parentHandle;
        child.cfg = cfg;
        child.type = Node::Type::File;
        child.tHandle = registry_.add(std::forward<T>(data));

        tinyHandle h = fnodes_.add(std::move(child));
        if (Node* parent = fnodes_.get(parentHandle)) {
            parent->addChild(h);
        }

        return h;
    }

    // Overload for folder creation
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, Node::CFG cfg) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;

        Node folder;
        folder.name = resolveRepeatName(parentHandle, name);
        folder.parent = parentHandle;
        folder.cfg = cfg;

        tinyHandle h = fnodes_.add(std::move(folder));
        if (auto* parent = fnodes_.get(parentHandle)) {
            parent->addChild(h);
        }

        return h;
    }

    // Internal recursive function that tracks the original parent for non-deletable rescues
    bool fRemoveRecursive(tinyHandle handle, tinyHandle rescueParent, bool recursive) {
        Node* node = fnodes_.get(handle);
        if (!node) return false;

        // Note: We can assume node is deletable since non-deletable nodes are moved, not recursed

        // copy children to avoid mutation during recursion
        std::vector<tinyHandle> childCopy = node->children;
        for (tinyHandle ch : childCopy) {
            Node* child = fnodes_.get(ch);
            if (!child) continue;
            
            if (child->deletable() && recursive) {
                // Child is deletable and we're in normal delete mode - remove it recursively
                fRemoveRecursive(ch, rescueParent, recursive);
            } else {
                // Child is non-deletable OR we're in flatten mode - move it to the rescue parent
                fMove(ch, rescueParent);
            }
        }

        // Special removal of data (queue vs instant depending on safeDelete)
        if (node->hasData()) rRemove(node->tHandle);

        // remove from parent children list
        if (fnodes_.valid(node->parent)) {
            Node* parent = fnodes_.get(node->parent);
            parent->removeChild(handle);
        }

        // finally remove node from pool
        fnodes_.instaRm(handle);
        return true;
    }

};
